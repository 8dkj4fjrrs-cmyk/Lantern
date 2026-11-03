#pragma once

// Reading and safely writing whole text files, and finding the program's own file.

#include "lantern/core/Engine.hpp" // Status

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace lantern {

/// The whole file, or nothing if it cannot be read.
std::optional<std::string> readTextFile(const std::filesystem::path& path);

/// Writes to a temporary file next to `path`, then renames it into place, so
/// a crash or full disk never leaves a half-written file behind. Creates the
/// folder if needed.
Status writeTextFileAtomically(const std::filesystem::path& path, std::string_view contents);

/// The running program's own file (empty if the system cannot tell). Used to
/// find files that ship beside it, e.g. an exported game's data.
std::filesystem::path executablePath();

/// The value of an environment variable such as HOME or APPDATA; nothing if
/// it is not set or is empty. (std::getenv is deprecated by Microsoft's
/// compiler, which builds with warnings as errors here, so this wraps the
/// safe Windows equivalent.) Read-only; call it from the main thread.
std::optional<std::string> environmentVariable(const char* name);

} // namespace lantern
