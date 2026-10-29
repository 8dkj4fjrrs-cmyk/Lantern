#include "EditorState.hpp"

#include <algorithm>

namespace lantern::editor {

namespace {
constexpr std::string_view kViewport = "panel.viewport";
constexpr std::string_view kHierarchy = "panel.hierarchy";
constexpr std::string_view kInspector = "panel.inspector";
constexpr std::string_view kConsole = "panel.console";
constexpr std::string_view kWidth = "window.width";
constexpr std::string_view kHeight = "window.height";
} // namespace

void EditorState::setWindowSize(int width, int height)
{
    // A size of 0 means "unknown" (e.g. the window never opened): keep the old one.
    if (width > 0) {
        windowWidth = std::clamp(width, kMinWindowSize, kMaxWindowSize);
    }
    if (height > 0) {
        windowHeight = std::clamp(height, kMinWindowSize, kMaxWindowSize);
    }
}

void EditorState::loadFrom(const Preferences& prefs)
{
    panels.viewport = prefs.getBool(kViewport, panels.viewport);
    panels.hierarchy = prefs.getBool(kHierarchy, panels.hierarchy);
    panels.inspector = prefs.getBool(kInspector, panels.inspector);
    panels.console = prefs.getBool(kConsole, panels.console);
    setWindowSize(prefs.getInt(kWidth, windowWidth), prefs.getInt(kHeight, windowHeight));
}

void EditorState::saveTo(Preferences& prefs) const
{
    prefs.setBool(kViewport, panels.viewport);
    prefs.setBool(kHierarchy, panels.hierarchy);
    prefs.setBool(kInspector, panels.inspector);
    prefs.setBool(kConsole, panels.console);
    prefs.setInt(kWidth, windowWidth);
    prefs.setInt(kHeight, windowHeight);
}

} // namespace lantern::editor
