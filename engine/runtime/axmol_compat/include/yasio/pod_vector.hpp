// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Rolo's replacement for yasio's pod_vector, the growable array that Axmol's
// core uses for byte buffers, vertex arrays and scheduler lists. Written from
// scratch for Rolo; it contains no yasio code (see ../../README.md and
// ADR 0012). It keeps yasio's names so Axmol compiles without edits.
//
// How it differs from std::vector, and why Axmol wants that:
// - It only holds "plain old data" (types that can be copied with memcpy and
//   need no destructor), so it can move its contents with realloc.
// - resize() does NOT zero new elements. Buffers about to be overwritten (a file
//   being read, vertices being generated) skip a pointless pass over memory.
// - Its memory comes from malloc/realloc and is returned with free. That lets
//   release_pointer() hand the block to code that will later call free() on it,
//   which Axmol's Data and image classes do.
#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace yasio {
// Memory for pod_vector: realloc and free, as explained above.
template <typename T> struct buffer_allocator {
    using value_type = T;
    static T* reallocate(void* block, std::size_t /*oldCount*/, std::size_t newCount)
    {
        return static_cast<T*>(std::realloc(block, newCount * sizeof(T)));
    }
    static void deallocate(void* block, std::size_t /*count*/) { std::free(block); }
};

// True for the one-byte types a byte buffer may hold.
template <typename T>
struct is_byte_type
    : std::bool_constant<sizeof(T) == 1 && (std::is_integral_v<T> || std::is_same_v<T, std::byte>)> {};

template <typename T, typename Alloc = buffer_allocator<T>> class pod_vector {
    static_assert(std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>,
                  "pod_vector moves its elements with memcpy/realloc, so they must be plain data");

    template <typename It>
    using if_iterator = std::enable_if_t<
        std::is_base_of_v<std::input_iterator_tag, typename std::iterator_traits<It>::iterator_category>,
        int>;

  public:
    using value_type = T;
    using allocator_type = Alloc;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // --- construction ------------------------------------------------------
    // Note: no std::initializer_list constructor, on purpose. Axmol writes
    // `pod_vector<Vec2> v{count}` and means "count elements", not "one element".
    pod_vector() noexcept = default;
    explicit pod_vector(size_type count) { resize(count); }
    pod_vector(size_type count, const_reference value) { resize(count, value); }
    template <typename It, if_iterator<It> = 0> pod_vector(It first, It last) { assign(first, last); }
    pod_vector(const pod_vector& other) { assign(other.begin(), other.end()); }
    pod_vector(pod_vector&& other) noexcept { steal(other); }
    ~pod_vector() { freeStorage(); }

    pod_vector& operator=(const pod_vector& other)
    {
        if (this != &other)
            assign(other.begin(), other.end());
        return *this;
    }
    pod_vector& operator=(pod_vector&& other) noexcept
    {
        if (this != &other) {
            freeStorage();
            steal(other);
        }
        return *this;
    }

    template <typename It, if_iterator<It> = 0> void assign(It first, It last)
    {
        clear();
        append(first, last);
    }
    void assign(const pod_vector& other)
    {
        if (this != &other)
            assign(other.begin(), other.end());
    }
    void assign(pod_vector&& other) { *this = std::move(other); }

    // --- element access ----------------------------------------------------
    reference operator[](size_type index) { return at(index); }
    const_reference operator[](size_type index) const { return at(index); }
    // Like Axmol's original, at() checks the index in debug builds only.
    reference at(size_type index)
    {
        assert(index < m_size);
        return m_data[index];
    }
    const_reference at(size_type index) const
    {
        assert(index < m_size);
        return m_data[index];
    }
    reference front() { return at(0); }
    const_reference front() const { return at(0); }
    reference back() { return at(m_size - 1); }
    const_reference back() const { return at(m_size - 1); }
    pointer data() noexcept { return m_data; }
    const_pointer data() const noexcept { return m_data; }

    iterator begin() noexcept { return m_data; }
    iterator end() noexcept { return m_data + m_size; }
    const_iterator begin() const noexcept { return m_data; }
    const_iterator end() const noexcept { return m_data + m_size; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

    // --- size --------------------------------------------------------------
    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type size_bytes() const noexcept { return m_size * sizeof(T); }
    size_type capacity() const noexcept { return m_capacity; }
    // (std::numeric_limits<...>::max) in brackets: Windows.h may define a max()
    // macro, which a following "(" would trigger.
    static constexpr size_type max_size() noexcept
    {
        return (std::numeric_limits<size_type>::max)() / sizeof(T);
    }

    void reserve(size_type capacity)
    {
        if (capacity > m_capacity)
            reallocateExactly(capacity);
    }
    void shrink_to_fit()
    {
        if (m_size == 0)
            freeStorage();
        else if (m_size != m_capacity)
            reallocateExactly(m_size);
    }
    void clear() noexcept { m_size = 0; }

    // New elements are left uninitialised (see the top of the file).
    void resize(size_type newSize)
    {
        growFor(newSize);
        m_size = newSize;
    }
    void resize(size_type newSize, const_reference value)
    {
        const size_type oldSize = m_size;
        resize(newSize);
        if (newSize > oldSize)
            std::fill(m_data + oldSize, m_data + newSize, value);
    }
    // Grows by `count` uninitialised (or `value`) elements.
    void expand(size_type count) { resize(m_size + count); }
    void expand(size_type count, const_reference value) { resize(m_size + count, value); }
    // Resizes and sets every byte of every element to zero.
    void reset(size_type newSize)
    {
        resize(newSize);
        if (m_size != 0)
            std::memset(static_cast<void*>(m_data), 0, size_bytes());
    }
    // As C++23's basic_string::resize_and_overwrite: makes room for `maxSize`
    // elements, lets `op(data, maxSize)` fill them, and keeps the size it returns.
    template <typename Operation> void resize_and_overwrite(size_type maxSize, Operation op)
    {
        reserve(maxSize);
        const auto finalSize = static_cast<size_type>(std::move(op)(m_data, maxSize));
        assert(finalSize <= maxSize);
        m_size = finalSize;
    }

    // --- adding and removing -----------------------------------------------
    template <typename... Args> reference emplace_back(Args&&... args)
    {
        // Build the value first: args may refer to an element of this vector,
        // which growing would move.
        T value = makeValue(std::forward<Args>(args)...);
        growFor(m_size + 1);
        m_data[m_size] = value;
        return m_data[m_size++];
    }
    void push_back(const value_type& value) { emplace_back(value); }
    void push_back(value_type&& value) { emplace_back(value); }
    void pop_back()
    {
        if (m_size != 0)
            --m_size;
    }

    template <typename It, if_iterator<It> = 0> pod_vector& append(It first, It last)
    {
        return insertRange(m_size, first, last), *this;
    }
    pod_vector& append(size_type count, const_reference value)
    {
        expand(count, value);
        return *this;
    }
    template <typename Container> pod_vector& operator+=(const Container& other)
    {
        return append(std::begin(other), std::end(other));
    }
    pod_vector& operator+=(const_reference value)
    {
        push_back(value);
        return *this;
    }

    template <typename It, if_iterator<It> = 0> iterator insert(const_iterator pos, It first, It last)
    {
        return insertRange(indexOf(pos), first, last);
    }
    iterator insert(const_iterator pos, size_type count, const_reference value)
    {
        const T copy = value; // `value` may live inside this vector
        const size_type index = openGap(indexOf(pos), count);
        std::fill(m_data + index, m_data + index + count, copy);
        return m_data + index;
    }
    iterator insert(const_iterator pos, const value_type& value) { return insert(pos, size_type{1}, value); }
    iterator insert(const_iterator pos, value_type&& value) { return insert(pos, size_type{1}, value); }
    template <typename... Args> iterator emplace(const_iterator pos, Args&&... args)
    {
        return insert(pos, size_type{1}, makeValue(std::forward<Args>(args)...));
    }

    iterator erase(const_iterator pos) { return erase(pos, pos + 1); }
    iterator erase(const_iterator first, const_iterator last)
    {
        const size_type from = indexOf(first);
        const size_type to = indexOf(last);
        assert(from <= to && to <= m_size);
        if (from != to) {
            std::memmove(static_cast<void*>(m_data + from), m_data + to, (m_size - to) * sizeof(T));
            m_size -= to - from;
        }
        return m_data + from;
    }

    void swap(pod_vector& other) noexcept
    {
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    // Position of the first element equal to `value`, or -1.
    std::ptrdiff_t index_of(const_reference value) const
    {
        const auto it = std::find(begin(), end(), value);
        return it == end() ? -1 : it - begin();
    }

    // --- handing the memory block over -------------------------------------
    // Gives the block to the caller, who must free() it; the vector is empty after.
    template <typename Int> pointer detach_abi(Int& length) noexcept
    {
        length = static_cast<Int>(m_size);
        pointer block = m_data;
        m_data = nullptr;
        m_size = m_capacity = 0;
        return block;
    }
    pointer detach_abi() noexcept
    {
        size_type ignored = 0;
        return detach_abi(ignored);
    }
    pointer release_pointer() noexcept { return detach_abi(); }
    // Takes ownership of a malloc'ed block holding `length` elements.
    void attach_abi(pointer block, size_type length)
    {
        freeStorage();
        m_data = block;
        m_size = m_capacity = length;
    }

  private:
    template <typename... Args> static T makeValue(Args&&... args)
    {
        // Aggregates (structs without constructors, like Axmol's vertex types)
        // are built with braces; everything else with parentheses.
        if constexpr (std::is_constructible_v<T, Args&&...>)
            return T(std::forward<Args>(args)...);
        else
            return T{std::forward<Args>(args)...};
    }

    size_type indexOf(const_iterator pos) const noexcept { return static_cast<size_type>(pos - m_data); }

    // Makes `count` uninitialised slots at `index`, shifting the tail right.
    size_type openGap(size_type index, size_type count)
    {
        assert(index <= m_size);
        if (count == 0)
            return index;
        growFor(m_size + count);
        std::memmove(static_cast<void*>(m_data + index + count), m_data + index,
                     (m_size - index) * sizeof(T));
        m_size += count;
        return index;
    }

    template <typename It> iterator insertRange(size_type index, It first, It last)
    {
        if constexpr (std::is_base_of_v<std::forward_iterator_tag,
                                        typename std::iterator_traits<It>::iterator_category>) {
            const auto count = static_cast<size_type>(std::distance(first, last));
            if (count == 0)
                return m_data + index;
            // The source may be inside this vector: copy it out before moving things.
            pod_vector source;
            source.reallocateExactly(count);
            std::copy(first, last, source.m_data);
            source.m_size = count;
            openGap(index, count);
            std::memcpy(static_cast<void*>(m_data + index), source.m_data, count * sizeof(T));
        } else {
            for (size_type i = index; first != last; ++first, ++i)
                insert(m_data + i, static_cast<T>(*first));
        }
        return m_data + index;
    }

    // Ensures room for `needed` elements, growing by half again each time so
    // that adding elements one by one stays cheap on average.
    void growFor(size_type needed)
    {
        if (needed <= m_capacity)
            return;
        if (needed > max_size())
            throw std::bad_alloc();
        const size_type geometric =
            m_capacity > max_size() - m_capacity / 2 ? max_size() : m_capacity + m_capacity / 2;
        reallocateExactly(needed > geometric ? needed : geometric);
    }

    void reallocateExactly(size_type capacity)
    {
        T* block = Alloc::reallocate(m_data, m_capacity, capacity);
        if (block == nullptr)
            throw std::bad_alloc();
        m_data = block;
        m_capacity = capacity;
    }

    void freeStorage() noexcept
    {
        if (m_data)
            Alloc::deallocate(m_data, m_capacity);
        m_data = nullptr;
        m_size = m_capacity = 0;
    }

    void steal(pod_vector& other) noexcept
    {
        m_data = std::exchange(other.m_data, nullptr);
        m_size = std::exchange(other.m_size, 0);
        m_capacity = std::exchange(other.m_capacity, 0);
    }

    T* m_data = nullptr;
    size_type m_size = 0;
    size_type m_capacity = 0;
};

template <typename T, typename Alloc = buffer_allocator<T>> using array_buffer = pod_vector<T, Alloc>;

// Removes every element equal to `value`; returns how many were removed.
template <typename Container, typename Value>
inline std::size_t erase(Container& container, const Value& value)
{
    const auto newEnd = std::remove(container.begin(), container.end(), value);
    const auto removed = static_cast<std::size_t>(std::distance(newEnd, container.end()));
    container.erase(newEnd, container.end());
    return removed;
}

// Removes every element for which `predicate` is true; returns how many.
template <typename Container, typename Predicate>
inline std::size_t erase_if(Container& container, Predicate predicate)
{
    const auto newEnd = std::remove_if(container.begin(), container.end(), predicate);
    const auto removed = static_cast<std::size_t>(std::distance(newEnd, container.end()));
    container.erase(newEnd, container.end());
    return removed;
}

// Inserts into an already-sorted container, after any equal elements. Keeping
// equal elements in insertion order matters: Axmol's scheduler relies on it
// so callbacks of the same priority run in the order they were added.
template <typename Container>
inline typename Container::iterator insert_sorted(Container& container,
                                                  const typename Container::value_type& value)
{
    return container.insert(std::upper_bound(container.begin(), container.end(), value), value);
}
template <typename Container, typename Less>
inline typename Container::iterator insert_sorted(Container& container,
                                                  const typename Container::value_type& value, Less less)
{
    return container.insert(std::upper_bound(container.begin(), container.end(), value, less), value);
}
} // namespace yasio
