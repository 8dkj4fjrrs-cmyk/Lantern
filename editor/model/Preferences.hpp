#pragma once

// A tiny settings store saved as a readable text file of "key = value" lines.
//
//     # Lantern Editor preferences
//     format = 1
//     panel.console = true
//     window.width = 1600
//
// Used for the editor's own preferences (panel visibility, window size).
// Unknown keys are kept, so newer and older editor versions can share a file.

#include "lantern/core/Engine.hpp" // lantern::Status

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace lantern::editor {

class Preferences {
public:
    static constexpr int kFormatVersion = 1;

    /// Reads `path`. A missing file is not an error (you get empty preferences).
    /// Malformed lines are skipped and reported in the returned message.
    Status load(const std::filesystem::path& path);

    /// Writes all values to `path`, creating folders as needed. The file is
    /// written to a temporary name first and then swapped in, so a crash
    /// mid-save never leaves a half-written file.
    Status save(const std::filesystem::path& path) const;

    /// Parses file contents (what load() does after reading the file).
    /// Returns how many lines were skipped as malformed.
    int parse(std::string_view text);

    /// Produces file contents (what save() writes).
    std::string serialize() const;

    bool contains(std::string_view key) const;
    std::optional<std::string> getString(std::string_view key) const;
    std::string getString(std::string_view key, std::string_view fallback) const;
    bool getBool(std::string_view key, bool fallback) const;
    int getInt(std::string_view key, int fallback) const;

    /// Keys may use letters, digits, '.', '_' and '-'. Invalid keys are ignored.
    /// Line breaks in values are replaced by spaces.
    void setString(std::string_view key, std::string_view value);
    void setBool(std::string_view key, bool value);
    void setInt(std::string_view key, int value);

    std::size_t size() const noexcept { return m_values.size(); }

    static bool isValidKey(std::string_view key);

private:
    std::map<std::string, std::string, std::less<>> m_values;
};

/// Reads a whole text file. Returns nullopt if it does not exist or cannot be read.
std::optional<std::string> readTextFile(const std::filesystem::path& path);

/// Writes a whole text file safely (temporary file + rename), creating folders.
Status writeTextFileAtomically(const std::filesystem::path& path, std::string_view contents);

} // namespace lantern::editor
