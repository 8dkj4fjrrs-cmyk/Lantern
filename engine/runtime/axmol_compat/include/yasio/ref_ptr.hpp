// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Rolo's replacement for yasio's ref_ptr, used by Axmol's SimpleTimer.
// Written from scratch for Rolo; it contains no yasio code (see ../../README.md).
//
// A smart pointer for objects that count their own references through
// retain() and release(). Constructing one from a raw pointer *adopts* the
// reference the object was created with (it does not retain); copies retain,
// and destruction releases.
#pragma once

#include <cstddef>
#include <utility>

namespace yasio {
template <typename T> class ref_ptr {
  public:
    ref_ptr() noexcept = default;
    ref_ptr(std::nullptr_t) noexcept {}
    explicit ref_ptr(T* adopted) noexcept : m_ptr(adopted) {}

    ref_ptr(const ref_ptr& other) noexcept : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            m_ptr->retain();
    }
    ref_ptr(ref_ptr&& other) noexcept : m_ptr(std::exchange(other.m_ptr, nullptr)) {}

    ~ref_ptr() { reset(); }

    ref_ptr& operator=(const ref_ptr& other) noexcept
    {
        if (other.m_ptr)
            other.m_ptr->retain();
        reset(other.m_ptr);
        return *this;
    }
    ref_ptr& operator=(ref_ptr&& other) noexcept
    {
        if (this != &other)
            reset(std::exchange(other.m_ptr, nullptr));
        return *this;
    }
    ref_ptr& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    // Releases the current object (if any) and adopts `adopted`.
    void reset(T* adopted = nullptr) noexcept
    {
        T* old = std::exchange(m_ptr, adopted);
        if (old)
            old->release();
    }

    // Gives up ownership without releasing; the caller now owns the reference.
    T* release() noexcept { return std::exchange(m_ptr, nullptr); }

    T* get() const noexcept { return m_ptr; }
    T& operator*() const noexcept { return *m_ptr; }
    T* operator->() const noexcept { return m_ptr; }
    operator T*() const noexcept { return m_ptr; }

  private:
    T* m_ptr = nullptr;
};
} // namespace yasio
