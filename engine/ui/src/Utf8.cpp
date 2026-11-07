#include "rolo/ui/Utf8.hpp"

#include <algorithm>

namespace rolo::ui::utf8 {

namespace {
bool isContinuation(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}
} // namespace

char32_t decode(std::string_view text, std::size_t& index)
{
    if (index >= text.size()) {
        return 0;
    }
    const auto c0 = static_cast<unsigned char>(text[index]);
    if (c0 < 0x80) {
        ++index;
        return c0;
    }

    int extra = 0;
    char32_t cp = 0;
    char32_t minimum = 0;
    if ((c0 & 0xE0) == 0xC0) {
        extra = 1;
        cp = c0 & 0x1F;
        minimum = 0x80;
    } else if ((c0 & 0xF0) == 0xE0) {
        extra = 2;
        cp = c0 & 0x0F;
        minimum = 0x800;
    } else if ((c0 & 0xF8) == 0xF0) {
        extra = 3;
        cp = c0 & 0x07;
        minimum = 0x10000;
    } else {
        ++index;
        return kReplacement;
    }

    if (index + static_cast<std::size_t>(extra) >= text.size()) { // sequence cut off by the end
        ++index;
        return kReplacement;
    }
    for (int i = 1; i <= extra; ++i) {
        const auto c = static_cast<unsigned char>(text[index + static_cast<std::size_t>(i)]);
        if (!isContinuation(c)) {
            ++index;
            return kReplacement;
        }
        cp = (cp << 6) | (c & 0x3F);
    }
    if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        ++index;
        return kReplacement;
    }
    index += static_cast<std::size_t>(extra) + 1;
    return cp;
}

void append(std::string& out, char32_t cp)
{
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        cp = kReplacement;
    }
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

std::size_t next(std::string_view text, std::size_t index)
{
    if (index >= text.size()) {
        return text.size();
    }
    std::size_t i = index;
    decode(text, i);
    return i;
}

std::size_t previous(std::string_view text, std::size_t index)
{
    if (index == 0) {
        return 0;
    }
    std::size_t i = std::min(index, text.size()) - 1;
    // Step back over continuation bytes (at most 3).
    int steps = 0;
    while (i > 0 && steps < 3 && isContinuation(static_cast<unsigned char>(text[i]))) {
        --i;
        ++steps;
    }
    return i;
}

std::size_t length(std::string_view text)
{
    std::size_t count = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        decode(text, i);
        ++count;
    }
    return count;
}

bool isWordSeparator(char32_t cp)
{
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r') {
        return true;
    }
    if (cp < 0x80) {
        const bool alnum = (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') || cp == '_';
        return !alnum;
    }
    return false; // treat non-ASCII letters as word characters
}

std::size_t previousWordStart(std::string_view text, std::size_t index)
{
    std::size_t i = std::min(index, text.size());
    // Skip separators to the left, then the word.
    while (i > 0) {
        const std::size_t p = previous(text, i);
        std::size_t tmp = p;
        if (!isWordSeparator(decode(text, tmp))) {
            break;
        }
        i = p;
    }
    while (i > 0) {
        const std::size_t p = previous(text, i);
        std::size_t tmp = p;
        if (isWordSeparator(decode(text, tmp))) {
            break;
        }
        i = p;
    }
    return i;
}

std::size_t nextWordEnd(std::string_view text, std::size_t index)
{
    std::size_t i = std::min(index, text.size());
    while (i < text.size()) {
        std::size_t tmp = i;
        if (!isWordSeparator(decode(text, tmp))) {
            break;
        }
        i = tmp;
    }
    while (i < text.size()) {
        std::size_t tmp = i;
        if (isWordSeparator(decode(text, tmp))) {
            break;
        }
        i = tmp;
    }
    return i;
}

} // namespace rolo::ui::utf8
