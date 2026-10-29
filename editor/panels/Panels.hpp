#pragma once

// The editor's panels (Dear ImGui). Each panel is drawn once per frame.
// Panels read and change editor state from editor/model; they never talk to
// the backend directly — only through lantern::UiFrame.

#include "../model/ConsoleLog.hpp"
#include "../model/EditorState.hpp"

#include "lantern/runtime/Application.hpp"

#include <cstdint>

namespace lantern::editor {

// Window names double as ImGui ids and as keys in the saved layout file,
// so they must never change.
inline constexpr const char* kViewportPanel = "Viewport";
inline constexpr const char* kHierarchyPanel = "Hierarchy";
inline constexpr const char* kInspectorPanel = "Inspector";
inline constexpr const char* kConsolePanel = "Console";

/// The game world as the player would see it, scaled to fit the panel.
void drawViewportPanel(bool& open, UiFrame& ui);

/// Tree of objects in the running scene; clicking selects.
void drawHierarchyPanel(bool& open, UiFrame& ui, EditorState& state);

/// Properties of the selected object (read-only until Milestone 3).
void drawInspectorPanel(bool& open, UiFrame& ui, EditorState& state);

/// Log messages from the engine, the backend and the editor.
class ConsolePanel {
public:
    void draw(bool& open, ConsoleLog& log);

private:
    ConsoleFilter m_filter;
    bool m_autoScroll = true;
    std::uint64_t m_lastSeen = 0;
    int m_scrollFrames = 0;
};

} // namespace lantern::editor
