#pragma once

// Exporting a project as a game that runs without the editor.
//
// An exported game is Lantern's game runtime (lantern_player, renamed after
// the game) with the project's files bundled beside it, plus the license
// texts of everything inside it. The runtime finds the bundled game by
// itself (see player/runtime/main.cpp), so players just open it:
//
//   Linux, Windows                      macOS
//   My Game/                            My Game.app/
//     My Game(.exe)    the runtime        Contents/MacOS/lantern_player
//     axslc/           its shaders        Contents/Resources/axslc/
//     (*.dll)          (Windows)          Contents/Resources/game/
//     game/            the project        Contents/Resources/licenses/
//     licenses/        license texts      Contents/Info.plist (the game's name)
//     README.txt
//
// Games are exported for the computer the editor runs on (no cross-export).
// See docs/exporting.md.

#include "EditorPaths.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace lantern::editor {

struct ExportOptions {
    std::filesystem::path project;   // the project folder
    std::filesystem::path output;    // the folder the game is created in
    std::string gameName;            // names the game's folder / app and program
    std::filesystem::path player;    // lantern_player(.exe), or lantern_player.app on macOS
    std::filesystem::path licenses;  // Lantern's licenses/ folder
    HostOs os = currentOs();
    std::string version;             // Lantern's version, written in README.txt
};

struct ExportResult {
    bool ok = false;
    std::filesystem::path game;      // what was made: the game's folder, or its .app
    std::filesystem::path program;   // the program to run
    std::string error;               // why it failed (empty when ok)
    std::uint64_t files = 0;         // files written
    std::uint64_t bytes = 0;
};

/// A file and folder name made from a game's name: letters, digits, spaces,
/// '-' and '_' kept; anything else dropped. "Game" if nothing is left.
std::string exportFileName(const std::string& gameName);

/// Exports the game. An earlier export of the same game in `output` is
/// replaced; any other existing folder of that name is left alone (an error).
ExportResult exportGame(const ExportOptions& options);

} // namespace lantern::editor
