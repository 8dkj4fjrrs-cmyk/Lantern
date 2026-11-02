#pragma once

// Implementation of lantern::substituteVariables (a template, so it lives in
// a header). Included at the end of Logic.hpp.

namespace lantern {

template <class Lookup>
std::string substituteVariables(std::string_view text, Lookup&& lookup)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '{' && i + 1 < text.size() && text[i + 1] == '{') {
            out += '{';
            ++i;
            continue;
        }
        if (c != '{') {
            out += c;
            continue;
        }
        const std::size_t close = text.find('}', i + 1);
        if (close == std::string_view::npos) {
            out.append(text.substr(i));
            break;
        }
        const std::string_view name = text.substr(i + 1, close - i - 1);
        if (const std::optional<std::string> value = lookup(name)) {
            out += *value;
        } else {
            out.append(text.substr(i, close - i + 1)); // unknown: left as written
        }
        i = close;
    }
    return out;
}

} // namespace lantern
