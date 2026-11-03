// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Lantern's replacement for the string helpers Axmol's core takes from yasio.
// Written from scratch for Lantern; it contains no yasio code. It keeps yasio's
// header name and the names Axmol calls (cxx17::string_view, cxx20::starts_with,
// ...) so Axmol compiles without edits. See ../../README.md and ADR 0012.
//
// Since C++17 the standard library has std::string_view, and C++20 has
// starts_with/ends_with as members, so most of this is a thin forwarding layer.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace cxx17 {
using std::basic_string_view;
using std::string_view;
using std::u16string_view;
using std::u32string_view;
using std::wstring_view;

inline namespace literals {
inline namespace string_view_literals {
using namespace std::string_view_literals;
} // namespace string_view_literals
} // namespace literals
} // namespace cxx17

namespace cxx20 {
// Free-function forms of C++20's starts_with/ends_with. Overloads (rather than
// templates) so that std::string, string literals and string_views all convert.
constexpr bool starts_with(std::string_view text, std::string_view prefix) noexcept
{
    return text.starts_with(prefix);
}
constexpr bool starts_with(std::string_view text, char prefix) noexcept
{
    return text.starts_with(prefix);
}
constexpr bool starts_with(std::wstring_view text, std::wstring_view prefix) noexcept
{
    return text.starts_with(prefix);
}
constexpr bool starts_with(std::wstring_view text, wchar_t prefix) noexcept
{
    return text.starts_with(prefix);
}

constexpr bool ends_with(std::string_view text, std::string_view suffix) noexcept
{
    return text.ends_with(suffix);
}
constexpr bool ends_with(std::string_view text, char suffix) noexcept
{
    return text.ends_with(suffix);
}
constexpr bool ends_with(std::wstring_view text, std::wstring_view suffix) noexcept
{
    return text.ends_with(suffix);
}
constexpr bool ends_with(std::wstring_view text, wchar_t suffix) noexcept
{
    return text.ends_with(suffix);
}

// "ic" = ignore case. Only ASCII letters are folded, which is what file paths,
// URL prefixes and extensions (the uses in Axmol) need.
namespace ic {
namespace detail {
template <typename Char> constexpr Char toLowerAscii(Char c) noexcept
{
    return (c >= Char('A') && c <= Char('Z')) ? Char(c - Char('A') + Char('a')) : c;
}

template <typename Char>
constexpr bool equalsAt(std::basic_string_view<Char> text, std::size_t offset,
                        std::basic_string_view<Char> part) noexcept
{
    for (std::size_t i = 0; i < part.size(); ++i) {
        if (toLowerAscii(text[offset + i]) != toLowerAscii(part[i]))
            return false;
    }
    return true;
}
} // namespace detail

constexpr bool iequals(std::string_view lhs, std::string_view rhs) noexcept
{
    return lhs.size() == rhs.size() && detail::equalsAt(lhs, 0, rhs);
}
constexpr bool iequals(std::wstring_view lhs, std::wstring_view rhs) noexcept
{
    return lhs.size() == rhs.size() && detail::equalsAt(lhs, 0, rhs);
}

constexpr bool starts_with(std::string_view text, std::string_view prefix) noexcept
{
    return text.size() >= prefix.size() && detail::equalsAt(text, 0, prefix);
}
constexpr bool starts_with(std::wstring_view text, std::wstring_view prefix) noexcept
{
    return text.size() >= prefix.size() && detail::equalsAt(text, 0, prefix);
}

constexpr bool ends_with(std::string_view text, std::string_view suffix) noexcept
{
    return text.size() >= suffix.size() && detail::equalsAt(text, text.size() - suffix.size(), suffix);
}
constexpr bool ends_with(std::wstring_view text, std::wstring_view suffix) noexcept
{
    return text.size() >= suffix.size() && detail::equalsAt(text, text.size() - suffix.size(), suffix);
}
} // namespace ic
} // namespace cxx20
