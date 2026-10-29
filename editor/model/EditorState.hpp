#pragma once

// Editor-wide state that is independent of the UI toolkit: which panels are
// open, what is selected, the window size, and how they are remembered
// between sessions.

#include "Preferences.hpp"

#include <cstdint>

namespace lantern::editor {

struct PanelVisibility {
    bool viewport = true;
    bool hierarchy = true;
    bool inspector = true;
    bool console = true;
};

struct EditorState {
    static constexpr int kDefaultWindowWidth = 1600;
    static constexpr int kDefaultWindowHeight = 900;
    static constexpr int kMinWindowSize = 480;
    static constexpr int kMaxWindowSize = 16384;

    PanelVisibility panels;
    int windowWidth = kDefaultWindowWidth;
    int windowHeight = kDefaultWindowHeight;

    /// Selected object in the running scene (session-only id); 0 = nothing.
    /// Deliberately not saved: these ids are only valid while the editor runs.
    std::uint64_t selectedNodeId = 0;

    /// Set by "View > Reset Layout"; the UI rebuilds the default docking
    /// layout on its next frame and clears the flag.
    bool layoutResetRequested = false;

    /// Reads remembered values; anything missing or out of range keeps its default.
    void loadFrom(const Preferences& prefs);

    /// Writes the values worth remembering.
    void saveTo(Preferences& prefs) const;

    /// Records the window size (clamped to sensible limits).
    void setWindowSize(int width, int height);
};

} // namespace lantern::editor
