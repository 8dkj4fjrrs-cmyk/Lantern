#include "EditorApp.hpp"

#include "../model/EditorPaths.hpp"

#include "lantern/core/Version.hpp"
#include "lantern/ui/Context.hpp"
#include "lantern/ui/widgets/Basic.hpp"
#include "lantern/ui/widgets/Box.hpp"
#include "lantern/ui/widgets/Dialog.hpp"
#include "lantern/ui/widgets/DockSpace.hpp"
#include "lantern/ui/widgets/Menu.hpp"

#include <string>
#include <utility>

namespace lantern::editor {

namespace {

constexpr std::string_view kCategory = "editor";

// A saved layout is written this long after the last change (and on exit),
// so dragging a splitter does not write the file on every frame.
constexpr double kLayoutSaveDelaySeconds = 1.0;

bool isEditorPanel(std::string_view id)
{
    return id == kViewportPanel || id == kHierarchyPanel || id == kInspectorPanel || id == kConsolePanel;
}

} // namespace

EditorApp::EditorApp(Engine& engine, ConsoleLog& console, EditorOptions options)
    : m_engine(engine)
    , m_console(console)
    , m_options(std::move(options))
{
}

ui::DockLayout EditorApp::defaultLayout()
{
    ui::DockLayout layout = ui::DockLayout::tabs({kViewportPanel});
    layout.dock(kHierarchyPanel, kViewportPanel, ui::DockSide::Left, 0.18f);
    layout.dock(kInspectorPanel, kViewportPanel, ui::DockSide::Right, 0.29f);
    layout.dock(kConsolePanel, kViewportPanel, ui::DockSide::Bottom, 0.3f);
    return layout;
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
    m_layoutDirty = false;
    if (m_configDir.empty() || m_dock == nullptr) {
        return;
    }
    const std::string text = m_dock->layout().serialize() + "\n";
    if (const Status status = writeTextFileAtomically(m_configDir / kLayoutFileName, text); !status) {
        log().error(kCategory, "Could not save the panel layout: " + status.message);
    }
}

void EditorApp::restoreLayout()
{
    if (!m_configDir.empty() && !m_options.resetLayout) {
        if (const auto text = readTextFile(m_configDir / kLayoutFileName)) {
            if (auto layout = ui::DockLayout::parse(*text, isEditorPanel)) {
                m_dock->setLayout(std::move(*layout));
                log().info(kCategory, "Restored the saved panel layout.");
                return;
            }
            log().warning(kCategory, "The saved panel layout could not be read; using the default one.");
        }
    }
    m_dock->resetLayout();
    log().info(kCategory, "Using the default panel layout.");
}

// ---------------------------------------------------------------------------
// Workspace
// ---------------------------------------------------------------------------
void EditorApp::buildWorkspace(UiFrame& frame)
{
    ui::Context& ctx = frame.ui();
    const ui::Theme& theme = ctx.theme();

    auto root = std::make_unique<ui::Column>(0.0f);
    root->setBackground(theme.window);

    // --- Menu bar --------------------------------------------------------------
    auto& menuBar = root->emplace<ui::MenuBar>();
    {
        ui::MenuItem quit;
        quit.label = "Quit Lantern Editor";
        quit.shortcut = ui::primaryShortcutLabel(ctx.platform(), "Q");
        quit.action = [this] { m_quitRequested = true; };
        menuBar.addMenu({"File", {std::move(quit)}});
    }
    {
        std::vector<ui::MenuItem> items;
        for (const char* id : {kViewportPanel, kHierarchyPanel, kInspectorPanel, kConsolePanel}) {
            ui::MenuItem item;
            item.label = std::string(1, static_cast<char>(id[0] - ('a' - 'A'))) + (id + 1);
            item.isChecked = [this, id] { return m_dock->isOpen(id); };
            item.action = [this, id] { m_dock->setOpen(id, !m_dock->isOpen(id)); };
            items.push_back(std::move(item));
        }
        items.push_back(ui::MenuItem::divider());
        ui::MenuItem reset;
        reset.label = "Reset Layout";
        reset.action = [this] {
            m_dock->resetLayout();
            log().info(kCategory, "Panel layout reset.");
        };
        items.push_back(std::move(reset));
        menuBar.addMenu({"View", std::move(items)});
    }
    {
        ui::MenuItem about;
        about.label = "About Lantern Engine…";
        about.action = [this, &ctx] { showAbout(ctx); };
        menuBar.addMenu({"Help", {std::move(about)}});
    }

    // --- Panels ------------------------------------------------------------------
    auto& dock = root->emplace<ui::DockSpace>();
    dock.setStretch(1.0f);
    m_viewport = &dock.addPanel(kViewportPanel, "Viewport", std::make_unique<ViewportPanel>());
    m_hierarchy = &dock.addPanel(kHierarchyPanel, "Hierarchy", std::make_unique<HierarchyPanel>(m_state));
    m_inspector = &dock.addPanel(kInspectorPanel, "Inspector", std::make_unique<InspectorPanel>(m_state));
    m_consolePanel = &dock.addPanel(kConsolePanel, "Console", std::make_unique<ConsolePanel>(m_console));
    dock.setDefaultLayout(defaultLayout());
    dock.setOnLayoutChanged([this, &ctx] {
        m_layoutDirty = true;
        m_layoutChangedAt = ctx.time();
    });
    m_dock = &dock;

    // --- Status bar ----------------------------------------------------------------
    auto& status = root->emplace<ui::Row>(theme.spacing * 2.0f, ui::Insets::symmetric(theme.padding, 0.0f));
    status.setBackground(theme.panelHeader);
    status.setCrossAlign(ui::CrossAlign::Center);
    status.setFixedSize({0.0f, theme.statusBarHeight});
    m_statusText = &status.emplace<ui::Label>("", ui::Label::Style::Small);
    m_statusText->setColor(theme.textDim);
    status.emplace<ui::Spacer>();
    m_statusProblems = &status.emplace<ui::Label>("", ui::Label::Style::Small);

    ctx.setRoot(std::move(root));

    // Cmd+Q (macOS) / Ctrl+Q quits from anywhere in the editor.
    ui::Modifiers primary;
    (ctx.platform().macConventions ? primary.super : primary.ctrl) = true;
    ctx.addShortcut(ui::Key::Q, primary, [this] { m_quitRequested = true; });
}

void EditorApp::updateStatusBar(UiFrame& frame)
{
    m_statusText->setText("Lantern Engine " + std::string(versionString()) + "   ·   "
                          + std::string(runtimeBackendName()) + "   ·   "
                          + std::to_string(static_cast<int>(frame.framesPerSecond() + 0.5f)) + " fps");

    const std::size_t errors = m_console.count(LogLevel::Error);
    const std::size_t warnings = m_console.count(LogLevel::Warning);
    std::string problems;
    if (errors > 0) {
        problems = std::to_string(errors) + (errors == 1 ? " error" : " errors");
    }
    if (warnings > 0) {
        problems += (problems.empty() ? "" : ", ") + std::to_string(warnings)
                    + (warnings == 1 ? " warning" : " warnings");
    }
    const ui::Theme& theme = frame.ui().theme();
    m_statusProblems->setText(problems);
    m_statusProblems->setColor(errors > 0 ? theme.error : theme.warning);
    m_statusProblems->setVisible(!problems.empty());
}

void EditorApp::showAbout(ui::Context& ctx)
{
    auto dialog = std::make_unique<ui::Dialog>("About Lantern Engine", 400.0f);
    ui::Column& body = dialog->body();
    body.emplace<ui::Label>("Lantern Engine " + std::string(versionString()), ui::Label::Style::Strong);
    body.emplace<ui::Label>("A C++-first 2D game engine and editor.", ui::Label::Style::Dim);
    body.emplace<ui::Separator>();
    body.emplace<ui::Label>("Runtime backend: " + std::string(runtimeBackendName()));
    body.emplace<ui::Label>("Editor interface: Lantern UI");
    body.emplace<ui::Separator>();
    body.emplace<ui::Label>("Copyright (c) 2026 Oscar Hird. MIT License.");
    body.emplace<ui::Label>("Third-party licenses are listed in THIRD_PARTY_NOTICES.md.", ui::Label::Style::Faint)
        .setWrap(true);
    dialog->addButton("Close", ui::Button::Style::Primary);
    ui::showDialog(ctx, std::move(dialog));
}

// ---------------------------------------------------------------------------
// Runtime callbacks
// ---------------------------------------------------------------------------
void EditorApp::setupUi(UiFrame& frame)
{
    buildWorkspace(frame);
    restoreLayout();
    m_layoutDirty = false;
}

void EditorApp::updateUi(UiFrame& frame)
{
    if (m_quitRequested) {
        frame.requestQuit();
    }
    m_viewport->update(frame);
    m_hierarchy->update(frame);
    m_inspector->update(frame);
    m_consolePanel->update(frame);
    updateStatusBar(frame);

    if (m_layoutDirty && frame.ui().time() - m_layoutChangedAt >= kLayoutSaveDelaySeconds) {
        saveLayout();
    }
}

void EditorApp::shutdownUi(UiFrame& /*frame*/)
{
    saveLayout();
    m_state.panels.viewport = m_dock->isOpen(kViewportPanel);
    m_state.panels.hierarchy = m_dock->isOpen(kHierarchyPanel);
    m_state.panels.inspector = m_dock->isOpen(kInspectorPanel);
    m_state.panels.console = m_dock->isOpen(kConsolePanel);
    // The widgets are destroyed with the UI; forget them.
    m_dock = nullptr;
    m_viewport = nullptr;
    m_hierarchy = nullptr;
    m_inspector = nullptr;
    m_consolePanel = nullptr;
    m_statusText = nullptr;
    m_statusProblems = nullptr;
}

// ---------------------------------------------------------------------------
// Run
// ---------------------------------------------------------------------------
Status EditorApp::run()
{
    loadSettings();

    PresentationSettings presentation;
    presentation.worldDisplay = WorldDisplay::Offscreen;
    presentation.uiSetup = [this](UiFrame& frame) { setupUi(frame); };
    presentation.uiUpdate = [this](UiFrame& frame) { updateUi(frame); };
    presentation.uiShutdown = [this](UiFrame& frame) { shutdownUi(frame); };

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
