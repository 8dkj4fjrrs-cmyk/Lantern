// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Byte-order conversion for Rolo's stream replacements (ibstream.hpp,
// obstream.hpp). Not a yasio header name: only those two include it.
//
// "Network order" is big-endian: the most significant byte first. Most
// computers today (x86, Apple silicon) are little-endian, so converting
// reverses the bytes; on a big-endian computer it does nothing.
#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace yasio::detail {
template <typename T> inline T reverseBytes(T value) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>);
    unsigned char bytes[sizeof(T)];
    std::memcpy(bytes, &value, sizeof(T));
    for (std::size_t i = 0; i < sizeof(T) / 2; ++i) {
        const unsigned char swapped = bytes[i];
        bytes[i] = bytes[sizeof(T) - 1 - i];
        bytes[sizeof(T) - 1 - i] = swapped;
    }
    std::memcpy(&value, bytes, sizeof(T));
    return value;
}

template <typename T> inline T hostToNetwork(T value) noexcept
{
    if constexpr (std::endian::native == std::endian::little && sizeof(T) > 1)
        return reverseBytes(value);
    else
        return value;
}

// The conversion is its own inverse.
template <typename T> inline T networkToHost(T value) noexcept
{
    return hostToNetwork(value);
}
} // namespace yasio::detail
