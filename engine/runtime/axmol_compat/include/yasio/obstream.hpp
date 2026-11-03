// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Lantern's replacement for yasio's binary writer, used by Axmol's UserDefault
// (settings file). Written from scratch for Lantern; it contains no yasio code
// (see ../../README.md). ibstream.hpp reads what this writes.
#pragma once

#include "yasio/byte_buffer.hpp"
#include "yasio/byte_order.hpp"
#include "yasio/string_view.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace yasio {
template <bool NetworkOrder> class basic_obstream {
  public:
    basic_obstream() = default;
    explicit basic_obstream(std::size_t capacity) { m_buffer.reserve(capacity); }

    // Writes a number: big-endian for obstream, this computer's order for
    // fast_obstream.
    template <typename T> void write(T value)
    {
        static_assert(std::is_arithmetic_v<T>, "write() takes numbers; use write_bytes for other data");
        const T stored = NetworkOrder ? detail::hostToNetwork(value) : value;
        write_bytes(&stored, sizeof(T));
    }
    // Writes a number into memory that already exists, in the same byte order.
    template <typename T> static void swrite(void* destination, T value) noexcept
    {
        const T stored = NetworkOrder ? detail::hostToNetwork(value) : value;
        std::memcpy(destination, &stored, sizeof(T));
    }

    // "7-bit encoding": seven bits of the number per byte, lowest first; the
    // top bit of each byte says whether another byte follows. Small numbers
    // (most string lengths) take one byte. Negative numbers are written as their
    // unsigned bit pattern.
    template <typename Int> void write_ix(Int value)
    {
        static_assert(std::is_integral_v<Int>);
        auto bits = static_cast<std::make_unsigned_t<Int>>(value);
        while (bits >= 0x80u) {
            write_byte(static_cast<std::uint8_t>((bits & 0x7Fu) | 0x80u));
            bits >>= 7;
        }
        write_byte(static_cast<std::uint8_t>(bits));
    }

    // A string: its length (7-bit encoded), then its bytes.
    void write_v(cxx17::string_view text)
    {
        write_ix(static_cast<int>(text.size()));
        write_bytes(text.data(), text.size());
    }

    void write_byte(std::uint8_t byte) { m_buffer.push_back(static_cast<char>(byte)); }
    void write_bytes(const void* bytes, std::size_t count)
    {
        if (count == 0)
            return;
        const auto* first = static_cast<const char*>(bytes);
        m_buffer.append(first, first + count);
    }
    void write_bytes(cxx17::string_view bytes) { write_bytes(bytes.data(), bytes.size()); }

    char* data() noexcept { return m_buffer.data(); }
    const char* data() const noexcept { return m_buffer.data(); }
    std::size_t length() const noexcept { return m_buffer.size(); }
    bool empty() const noexcept { return m_buffer.empty(); }
    void clear() noexcept { m_buffer.clear(); }
    sbyte_buffer& buffer() noexcept { return m_buffer; }
    const sbyte_buffer& buffer() const noexcept { return m_buffer; }

  private:
    sbyte_buffer m_buffer;
};

using obstream = basic_obstream<true>;
using fast_obstream = basic_obstream<false>;
} // namespace yasio
