#include "Preferences.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>

namespace rolo::editor {

namespace {

std::string_view trim(std::string_view text)
{
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!text.empty() && isSpace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

} // namespace

bool Preferences::isValidKey(std::string_view key)
{
    if (key.empty() || key.size() > 128) {
        return false;
    }
    for (char c : key) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.'
                        || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

int Preferences::parse(std::string_view text)
{
    int skipped = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        const std::string_view line = trim(text.substr(start, end - start));
        start = end + 1;

        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            ++skipped;
            continue;
        }
        const std::string_view key = trim(line.substr(0, eq));
        if (!isValidKey(key)) {
            ++skipped;
            continue;
        }
        m_values[std::string(key)] = std::string(trim(line.substr(eq + 1)));
    }
    return skipped;
}

std::string Preferences::serialize() const
{
    std::string out = "# Rolo Editor preferences. Safe to delete: defaults are restored.\n";
    out += "format = " + std::to_string(kFormatVersion) + "\n";
    for (const auto& [key, value] : m_values) {
        if (key == "format") {
            continue;
        }
        out += key + " = " + value + "\n";
    }
    return out;
}

Status Preferences::load(const std::filesystem::path& path)
{
    const std::optional<std::string> text = readTextFile(path);
    if (!text) {
        return Status::success(); // first run: no file yet
    }
    const int skipped = parse(*text);
    if (skipped > 0) {
        return Status::failure(std::to_string(skipped) + " unreadable line(s) in " + path.string()
                               + " were ignored.");
    }
    return Status::success();
}

Status Preferences::save(const std::filesystem::path& path) const
{
    return writeTextFileAtomically(path, serialize());
}

bool Preferences::contains(std::string_view key) const
{
    return m_values.find(key) != m_values.end();
}

std::optional<std::string> Preferences::getString(std::string_view key) const
{
    if (auto it = m_values.find(key); it != m_values.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string Preferences::getString(std::string_view key, std::string_view fallback) const
{
    return getString(key).value_or(std::string(fallback));
}

bool Preferences::getBool(std::string_view key, bool fallback) const
{
    const std::optional<std::string> value = getString(key);
    if (!value) {
        return fallback;
    }
    if (*value == "true" || *value == "1" || *value == "yes") {
        return true;
    }
    if (*value == "false" || *value == "0" || *value == "no") {
        return false;
    }
    return fallback;
}

int Preferences::getInt(std::string_view key, int fallback) const
{
    const std::optional<std::string> value = getString(key);
    if (!value) {
        return fallback;
    }
    int result = 0;
    const char* end = value->data() + value->size();
    const auto [ptr, ec] = std::from_chars(value->data(), end, result);
    return (ec == std::errc{} && ptr == end) ? result : fallback;
}

void Preferences::setString(std::string_view key, std::string_view value)
{
    if (!isValidKey(key)) {
        return;
    }
    std::string clean(trim(value));
    for (char& c : clean) {
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    m_values[std::string(key)] = std::move(clean);
}

void Preferences::setBool(std::string_view key, bool value)
{
    setString(key, value ? "true" : "false");
}

void Preferences::setInt(std::string_view key, int value)
{
    setString(key, std::to_string(value));
}

} // namespace rolo::editor
