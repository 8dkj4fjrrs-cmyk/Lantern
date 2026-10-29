#pragma once

// Reading and safely writing whole text files.

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

} // namespace lantern
