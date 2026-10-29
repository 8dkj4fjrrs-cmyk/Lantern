#pragma once

// Where the editor keeps per-user files, and which system font it uses.
// Operating-system differences live here and nowhere else in the editor.

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace lantern::editor {

/// Looks up an environment variable (injectable so tests need not touch the real environment).
using EnvironmentLookup = std::function<std::optional<std::string>(const char* name)>;

/// Reads the real process environment.
std::optional<std::string> realEnvironment(const char* name);

enum class HostOs { MacOS, Windows, Linux };

/// The operating system this program was built for.
HostOs currentOs() noexcept;

/// The folder for the editor's per-user settings (layout, preferences):
///   macOS:   ~/Library/Application Support/Lantern Engine/Editor
///   Windows: %APPDATA%\Lantern Engine\Editor
///   Linux:   $XDG_CONFIG_HOME/lantern-engine/editor (default ~/.config/...)
/// Returns an empty path if the home folder cannot be determined.
std::filesystem::path userConfigDirectory(HostOs os, const EnvironmentLookup& env);

/// Same, for the current OS and real environment.
std::filesystem::path userConfigDirectory();

/// File names inside the config directory.
inline constexpr const char* kPreferencesFileName = "editor.prefs";
inline constexpr const char* kLayoutFileName = "layout.ini";

/// Readable system fonts to try for the editor UI, best first.
std::vector<std::filesystem::path> uiFontCandidates(HostOs os, const EnvironmentLookup& env);

/// The first candidate that exists on this computer, or an empty path
/// (the UI then falls back to its built-in font).
std::filesystem::path findUiFont();

} // namespace lantern::editor
