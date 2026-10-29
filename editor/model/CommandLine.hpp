#pragma once

// Command-line options for lantern_editor. No UI code, so it is unit-tested.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace lantern::editor {

struct EditorOptions {
    bool showHelp = false;
    bool showVersion = false;

    /// Folder for preferences and layout. Empty = the per-user default.
    /// Tests point this at a temporary folder so they never touch real settings.
    std::string configDirectory;

    /// Ignore the saved panel layout and start with the default one.
    bool resetLayout = false;

    std::uint64_t maxFrames = 0;   // 0 = until closed
    std::string screenshotPath;    // empty = none
};

struct EditorParseResult {
    EditorOptions options;
    std::string error; // empty on success
    bool ok() const noexcept { return error.empty(); }
};

/// Parses arguments *after* the program name.
EditorParseResult parseEditorCommandLine(std::span<const std::string_view> args);

std::string editorHelpText();

} // namespace lantern::editor
