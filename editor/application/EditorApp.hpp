#pragma once

// The editor application: owns the editor state, builds the workspace
// (menu bar, dockable panels, status bar) from Lantern UI widgets, and
// remembers the layout and preferences between sessions.

#include "../model/CommandLine.hpp"
#include "../model/ConsoleLog.hpp"
#include "../model/EditorState.hpp"
#include "../model/Preferences.hpp"
#include "../panels/Panels.hpp"

#include "lantern/core/Engine.hpp"
#include "lantern/runtime/Application.hpp"
#include "lantern/ui/DockLayout.hpp"

#include <filesystem>

namespace lantern::ui {
class Context;
class DockSpace;
class Label;
} // namespace lantern::ui

namespace lantern::editor {

class EditorApp {
public:
    EditorApp(Engine& engine, ConsoleLog& console, EditorOptions options);

    /// Opens the editor window and blocks until it closes.
    Status run();

    /// The panel arrangement used on first start and by View > Reset Layout:
    ///   +-----------+---------------------+-----------+
    ///   | Hierarchy |      Viewport       | Inspector |
    ///   |           +---------------------+           |
    ///   |           |      Console        |           |
    ///   +-----------+---------------------+-----------+
    static ui::DockLayout defaultLayout();

private:
    // Called by the runtime (see PresentationSettings).
    void setupUi(UiFrame& frame);
    void updateUi(UiFrame& frame);
    void shutdownUi(UiFrame& frame);

    void buildWorkspace(UiFrame& frame);
    void restoreLayout();
    void updateStatusBar(UiFrame& frame);
    void showAbout(ui::Context& ctx);

    void loadSettings();
    void saveSettings();
    void saveLayout();

    Logger& log() { return m_engine.logger(); }

    Engine& m_engine;
    ConsoleLog& m_console;
    EditorOptions m_options;

    EditorState m_state;
    Preferences m_preferences;
    std::filesystem::path m_configDir;   // empty = do not persist anything

    // Widgets (owned by the UI context; valid between setupUi and shutdownUi).
    ui::DockSpace* m_dock = nullptr;
    ViewportPanel* m_viewport = nullptr;
    HierarchyPanel* m_hierarchy = nullptr;
    InspectorPanel* m_inspector = nullptr;
    ConsolePanel* m_consolePanel = nullptr;
    ui::Label* m_statusText = nullptr;
    ui::Label* m_statusProblems = nullptr;

    bool m_quitRequested = false;   // File > Quit or the shortcut
    bool m_layoutDirty = false;
    double m_layoutChangedAt = 0.0;
};

} // namespace lantern::editor
