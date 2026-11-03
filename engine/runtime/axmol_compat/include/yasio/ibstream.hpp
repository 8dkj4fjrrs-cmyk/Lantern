// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Lantern's replacement for yasio's binary readers, used by Axmol's UserDefault
// (settings file) and Metal shader loader. Written from scratch for Lantern; it
// contains no yasio code (see ../../README.md).
//
// Two flavours read the same layout with different byte orders:
// - ibstream_view reads numbers stored big-endian ("network order"), as
//   obstream writes them, so the files are the same on every computer;
// - fast_ibstream_view reads numbers in this computer's own order, for data
//   made on the same kind of machine (compiled shaders).
// Strings are stored as a "7-bit encoded" length followed by the bytes; see
// obstream.hpp for that encoding.
#pragma once

#include "yasio/byte_order.hpp"
#include "yasio/string_view.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace yasio {
template <bool NetworkOrder> class basic_ibstream_view {
  public:
    basic_ibstream_view() noexcept = default;
    basic_ibstream_view(const void* data, std::size_t size) noexcept { reset(data, size); }

    void reset(const void* data, std::size_t size) noexcept
    {
        m_first = static_cast<const char*>(data);
        m_last = m_first + size;
        m_pos = m_first;
    }

    // Reads a number written by obstream::write<T>.
    template <typename T> T read() { return sread<T>(consume(sizeof(T))); }
    // Reads a number from raw memory, converting its byte order.
    template <typename T> static T sread(const void* source) noexcept
    {
        T value;
        std::memcpy(&value, source, sizeof(T));
        return NetworkOrder ? detail::networkToHost(value) : value;
    }

    std::uint8_t read_byte() { return static_cast<std::uint8_t>(*consume(1)); }

    // Reads an int written with the 7-bit encoding. The result must fit in 32
    // bits; anything longer is a damaged file.
    template <typename Int = int> Int read_ix()
    {
        std::uint64_t result = 0;
        for (int shift = 0; shift < 64; shift += 7) {
            const std::uint8_t byte = read_byte();
            result |= static_cast<std::uint64_t>(byte & 0x7Fu) << shift;
            if ((byte & 0x80u) == 0) {
                if constexpr (sizeof(Int) < sizeof(std::uint64_t)) {
                    if (result > static_cast<std::make_unsigned_t<Int>>(-1))
                        throw std::out_of_range("ibstream: 7-bit encoded number too large");
                }
                return static_cast<Int>(result);
            }
        }
        throw std::out_of_range("ibstream: 7-bit encoded number too long");
    }

    // A string written by obstream::write_v. The view points into the data.
    cxx17::string_view read_v()
    {
        const int length = read_ix<int>();
        return read_bytes(length);
    }

    cxx17::string_view read_bytes(int length)
    {
        if (length <= 0)
            return {};
        return cxx17::string_view(consume(static_cast<std::size_t>(length)),
                                  static_cast<std::size_t>(length));
    }
    void read_bytes(void* destination, int length)
    {
        if (length > 0)
            std::memcpy(destination, consume(static_cast<std::size_t>(length)),
                        static_cast<std::size_t>(length));
    }
    void read_bytes(std::string& destination, int length) { destination.assign(read_bytes(length)); }

    void advance(std::ptrdiff_t offset) { m_pos += offset; }
    std::ptrdiff_t tell() const noexcept { return m_pos - m_first; }
    // Moves the read position as fseek would (SEEK_SET, SEEK_CUR, SEEK_END)
    // and returns it; a SEEK_CUR beyond either end stops at that end.
    std::ptrdiff_t seek(std::ptrdiff_t offset, int whence) noexcept
    {
        switch (whence) {
        case SEEK_SET:
            m_pos = m_first + offset;
            break;
        case SEEK_CUR:
            m_pos += offset;
            if (m_pos < m_first)
                m_pos = m_first;
            else if (m_pos > m_last)
                m_pos = m_last;
            break;
        case SEEK_END:
            m_pos = m_last - offset;
            break;
        default:
            break;
        }
        return m_pos - m_first;
    }

    bool eof() const noexcept { return m_pos >= m_last; }
    bool empty() const noexcept { return m_first == m_last; }
    std::size_t length() const noexcept { return static_cast<std::size_t>(m_last - m_first); }
    const char* data() const noexcept { return m_first; }

  private:
    // Returns the next `count` bytes and moves past them. Reading past the end
    // throws instead of reading whatever memory follows.
    const char* consume(std::size_t count)
    {
        if (m_pos < m_first || static_cast<std::size_t>(m_last - m_pos) < count)
            throw std::out_of_range("ibstream: read past the end of the data");
        const char* at = m_pos;
        m_pos += count;
        return at;
    }

    const char* m_first = nullptr;
    const char* m_last = nullptr;
    const char* m_pos = nullptr;
};

using ibstream_view = basic_ibstream_view<true>;
using fast_ibstream_view = basic_ibstream_view<false>;
// Axmol only uses ibstream for its static sread(), so the view serves.
using ibstream = ibstream_view;
using fast_ibstream = fast_ibstream_view;
} // namespace yasio
