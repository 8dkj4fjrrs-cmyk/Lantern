#pragma once

// The editor application: owns the editor state, builds the dockable
// workspace each frame, and remembers layout and preferences between sessions.

#include "../model/CommandLine.hpp"
#include "../model/ConsoleLog.hpp"
#include "../model/EditorState.hpp"
#include "../model/Preferences.hpp"
#include "../panels/Panels.hpp"

#include "lantern/core/Engine.hpp"
#include "lantern/runtime/Application.hpp"

#include <filesystem>

namespace lantern::editor {

class EditorApp {
public:
    EditorApp(Engine& engine, ConsoleLog& console, EditorOptions options);

    /// Opens the editor window and blocks until it closes.
    Status run();

private:
    // Called by the runtime (see PresentationSettings).
    void setupUi();
    void drawFrame(UiFrame& ui);
    void shutdownUi();

    void drawMenuBar(UiFrame& ui);
    void drawStatusBar(UiFrame& ui);
    void drawDockspace();
    void drawAboutPopup();

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
    ConsolePanel m_consolePanel;
    bool m_openAbout = false;
};

} // namespace lantern::editor
