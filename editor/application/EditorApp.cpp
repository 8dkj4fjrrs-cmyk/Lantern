#include "EditorApp.hpp"

#include "../model/EditorPaths.hpp"
#include "Theme.hpp"

#include "lantern/core/Version.hpp"

#include <imgui.h>
#include <imgui_internal.h> // DockBuilder (layout) and the status bar

#include <string>
#include <utility>

namespace lantern::editor {

namespace {

constexpr std::string_view kCategory = "editor";
constexpr const char* kDockspaceName = "LanternDockspace";
constexpr const char* kAboutPopup = "About Lantern Engine";

#if defined(__APPLE__)
constexpr const char* kQuitShortcutLabel = "Cmd+Q";
#else
constexpr const char* kQuitShortcutLabel = "Ctrl+Q";
#endif

} // namespace

EditorApp::EditorApp(Engine& engine, ConsoleLog& console, EditorOptions options)
    : m_engine(engine)
    , m_console(console)
    , m_options(std::move(options))
{
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
void EditorApp::loadSettings()
{
    m_configDir = m_options.configDirectory.empty() ? userConfigDirectory()
                                                    : std::filesystem::path(m_options.configDirectory);
    if (m_configDir.empty()) {
        log().warning(kCategory, "Could not find your home folder; layout and preferences will not be saved.");
        return;
    }
    log().info(kCategory, "Settings folder: " + m_configDir.string());

    if (const Status status = m_preferences.load(m_configDir / kPreferencesFileName); !status) {
        log().warning(kCategory, status.message);
    }
    m_state.loadFrom(m_preferences);
    if (m_options.resetLayout) {
        m_state.panels = PanelVisibility{};
    }
}

void EditorApp::saveSettings()
{
    if (m_configDir.empty()) {
        return;
    }
    m_state.saveTo(m_preferences);
    if (const Status status = m_preferences.save(m_configDir / kPreferencesFileName); !status) {
        log().error(kCategory, "Could not save preferences: " + status.message);
    }
}

void EditorApp::saveLayout()
{
    if (m_configDir.empty()) {
        return;
    }
    std::size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    if (const Status status = writeTextFileAtomically(m_configDir / kLayoutFileName, std::string_view(data, size));
        !status) {
        log().error(kCategory, "Could not save the panel layout: " + status.message);
    }
    ImGui::GetIO().WantSaveIniSettings = false;
}

// ---------------------------------------------------------------------------
// Runtime callbacks
// ---------------------------------------------------------------------------
void EditorApp::setupUi()
{
    applyLanternTheme();

    if (m_configDir.empty() || m_options.resetLayout) {
        m_state.layoutResetRequested = true;
        return;
    }
    if (const auto layout = readTextFile(m_configDir / kLayoutFileName)) {
        ImGui::LoadIniSettingsFromMemory(layout->data(), layout->size());
        log().info(kCategory, "Restored the saved panel layout.");
    }
}

void EditorApp::shutdownUi()
{
    saveLayout();
}

void EditorApp::drawFrame(UiFrame& ui)
{
    // Cmd+Q / Ctrl+Q quits from anywhere in the editor.
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Q, ImGuiInputFlags_RouteGlobal)) {
        ui.requestQuit();
    }

    drawMenuBar(ui);
    drawStatusBar(ui);   // before the dockspace, so the dockspace fits above it
    drawDockspace();

    drawViewportPanel(m_state.panels.viewport, ui);
    drawHierarchyPanel(m_state.panels.hierarchy, ui, m_state);
    drawInspectorPanel(m_state.panels.inspector, ui, m_state);
    m_consolePanel.draw(m_state.panels.console, m_console);
    drawAboutPopup();

    // ImGui asks to save a few seconds after the layout changes.
    if (ImGui::GetIO().WantSaveIniSettings) {
        saveLayout();
    }
}

// ---------------------------------------------------------------------------
// Workspace
// ---------------------------------------------------------------------------
void EditorApp::drawMenuBar(UiFrame& ui)
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Quit", kQuitShortcutLabel)) {
            ui.requestQuit();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem(kViewportPanel, nullptr, &m_state.panels.viewport);
        ImGui::MenuItem(kHierarchyPanel, nullptr, &m_state.panels.hierarchy);
        ImGui::MenuItem(kInspectorPanel, nullptr, &m_state.panels.inspector);
        ImGui::MenuItem(kConsolePanel, nullptr, &m_state.panels.console);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            m_state.panels = PanelVisibility{};
            m_state.layoutResetRequested = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About Lantern Engine...")) {
            m_openAbout = true;
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void EditorApp::drawStatusBar(UiFrame& ui)
{
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    const float height = ImGui::GetFrameHeight();
    if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, height, flags)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::TextDisabled("Lantern Engine %s  |  %s  |  %.0f fps", versionString().data(),
                                runtimeBackendName().data(), static_cast<double>(ui.framesPerSecond()));
            const std::size_t errors = m_console.count(LogLevel::Error);
            const std::size_t warnings = m_console.count(LogLevel::Warning);
            if (errors + warnings > 0) {
                ImGui::TextDisabled("|");
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "%zu error(s), %zu warning(s)", errors,
                                   warnings);
            }
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

void EditorApp::drawDockspace()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiID dockspaceId = ImGui::GetID(kDockspaceName);

    // Build the default layout on first run (no saved layout) or on request:
    //   +-----------+---------------------+-----------+
    //   | Hierarchy |      Viewport       | Inspector |
    //   |           +---------------------+           |
    //   |           |      Console        |           |
    //   +-----------+---------------------+-----------+
    if (m_state.layoutResetRequested || ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
        m_state.layoutResetRequested = false;
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        ImGuiID center = dockspaceId;
        const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
        const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
        const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);

        ImGui::DockBuilderDockWindow(kHierarchyPanel, left);
        ImGui::DockBuilderDockWindow(kInspectorPanel, right);
        ImGui::DockBuilderDockWindow(kConsolePanel, bottom);
        ImGui::DockBuilderDockWindow(kViewportPanel, center);
        ImGui::DockBuilderFinish(dockspaceId);
        log().info(kCategory, "Using the default panel layout.");
    }

    ImGui::DockSpaceOverViewport(dockspaceId, viewport);
}

void EditorApp::drawAboutPopup()
{
    if (m_openAbout) {
        ImGui::OpenPopup(kAboutPopup);
        m_openAbout = false;
    }
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(kAboutPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Lantern Engine %s", versionString().data());
        ImGui::TextDisabled("A C++-first 2D game engine and editor.");
        ImGui::Separator();
        ImGui::Text("Runtime backend: %s", runtimeBackendName().data());
        ImGui::Text("Editor UI: Dear ImGui %s", ImGui::GetVersion());
        ImGui::Separator();
        ImGui::Text("Copyright (c) 2026 Oscar Hird. MIT License.");
        ImGui::TextDisabled("Third-party licenses: THIRD_PARTY_NOTICES.md");
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(120.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// Run
// ---------------------------------------------------------------------------
Status EditorApp::run()
{
    loadSettings();

    PresentationSettings presentation;
    presentation.worldDisplay = WorldDisplay::Offscreen;
    presentation.ui = [this](UiFrame& ui) { drawFrame(ui); };
    presentation.uiSetup = [this] { setupUi(); };
    presentation.uiShutdown = [this] { shutdownUi(); };
    presentation.uiFontFile = findUiFont();
    presentation.uiFontSize = 15.0f;

    Application app(m_engine,
                    WindowSettings{.title = "Lantern Editor",
                                   .width = m_state.windowWidth,
                                   .height = m_state.windowHeight,
                                   .resizable = true},
                    std::move(presentation));

    const Status status = app.run(RunOptions{.maxFrames = m_options.maxFrames,
                                             .screenshotPath = m_options.screenshotPath,
                                             .showStats = false,
                                             .escapeQuits = false});

    const RunReport& report = app.lastRunReport();
    m_state.setWindowSize(report.finalWindowWidth, report.finalWindowHeight);
    saveSettings();
    return status;
}

} // namespace lantern::editor
