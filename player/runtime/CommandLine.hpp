#pragma once

// Command-line options for lantern_player.
//
// Kept separate from main.cpp (and free of any backend code) so the parsing
// rules can be unit-tested without opening a window.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace lantern::player {

struct PlayerOptions {
    // What to do.
    bool showHelp = false;
    bool showVersion = false;

    // Window.
    std::string title = "Lantern Engine";
    int width = 1280;
    int height = 720;

    // Run behaviour.
    std::uint64_t maxFrames = 0;  // 0 = until closed
    std::string screenshotPath;   // empty = none
    bool showStats = true;
};

/// Result of parsing: either options, or an error message for the user.
struct ParseResult {
    PlayerOptions options;
    std::string error; // empty on success
    bool ok() const noexcept { return error.empty(); }
};

/// Parses arguments *after* the program name.
ParseResult parseCommandLine(std::span<const std::string_view> args);

/// Text printed by --help.
std::string helpText();

} // namespace lantern::player
