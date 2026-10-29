#pragma once

// Editor-wide state that is independent of the UI toolkit: which panels are
// open, what is selected, the window size, and how they are remembered
// between sessions.

#include "Preferences.hpp"

#include <cstdint>
#include <string>
#include <vector>

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

    /// The selected entity's ID value (0 = nothing; kSceneSelected = the
    /// scene itself). Not saved: selection is part of a session, not the scene.
    std::uint64_t selectedNodeId = 0;
    static constexpr std::uint64_t kSceneSelected = ~std::uint64_t{0};

    /// Project folders opened recently, newest first (at most kMaxRecent).
    std::vector<std::string> recentProjects;
    static constexpr std::size_t kMaxRecent = 8;
    /// The scene file open when the editor last closed (reopened next time).
    std::string lastScene;

    /// Puts `folder` at the top of the recent list (no duplicates).
    void addRecentProject(const std::string& folder);
    void removeRecentProject(const std::string& folder);

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
