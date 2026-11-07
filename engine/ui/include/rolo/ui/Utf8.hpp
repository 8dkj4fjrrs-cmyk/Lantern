#pragma once

// UTF-8 helpers. Text in Rolo is always UTF-8 in std::string; these
// functions step through it one character (code point) at a time, which is
// what caret movement and drawing need.

#include <cstddef>
#include <string>
#include <string_view>

namespace rolo::ui::utf8 {

inline constexpr char32_t kReplacement = 0xFFFD; // shown for broken byte sequences

/// Decodes the code point starting at byte `index` and advances `index` past it.
/// Invalid bytes decode as U+FFFD and advance by one byte, so loops always end.
char32_t decode(std::string_view text, std::size_t& index);

/// Appends the UTF-8 encoding of `cp` to `out` (invalid code points become U+FFFD).
void append(std::string& out, char32_t cp);

/// Byte index of the start of the next / previous character (clamped to the text).
std::size_t next(std::string_view text, std::size_t index);
std::size_t previous(std::string_view text, std::size_t index);

/// Number of code points.
std::size_t length(std::string_view text);

/// True for characters that separate words when moving by word (spaces and punctuation).
bool isWordSeparator(char32_t cp);

/// Start of the word before / end of the word after `index` (for Option/Ctrl+arrow).
std::size_t previousWordStart(std::string_view text, std::size_t index);
std::size_t nextWordEnd(std::string_view text, std::size_t index);

} // namespace rolo::ui::utf8
