#include "EditorApp.hpp"

#include "../model/EditorPaths.hpp"

#include "lantern/core/Version.hpp"
#include "lantern/ui/Context.hpp"
#include "lantern/ui/widgets/Basic.hpp"
#include "lantern/ui/widgets/Box.hpp"
#include "lantern/ui/widgets/Dialog.hpp"
#include "lantern/ui/widgets/DockSpace.hpp"
#include "lantern/ui/widgets/Inspector.hpp"
#include "lantern/ui/widgets/Menu.hpp"

#include <string>
#include <utility>

namespace lantern::editor {

namespace {

constexpr std::string_view kCategory = "editor";

// A saved layout is written this long after the last change (and on exit),
// so dragging a splitter does not write the file on every frame.
constexpr double kLayoutSaveDelaySeconds = 1.0;

// Every panel, in View-menu order: id, title, tab icon.
struct PanelInfo {
    const char* id;
    const char* title;
    ui::Icon icon;
};
constexpr PanelInfo kPanels[] = {
    {kHierarchyPanel, "Hierarchy", ui::Icon::Layers},
    {kAssetsPanel, "Assets", ui::Icon::Folder},
    {kViewportPanel, "Scene", ui::Icon::Scene},
    {kPreviewPanel, "Assets Preview", ui::Icon::Eye},
    {kConsolePanel, "Console", ui::Icon::Terminal},
    {kInspectorPanel, "Inspector", ui::Icon::Sliders},
};

bool isEditorPanel(std::string_view id)
{
    for (const PanelInfo& panel : kPanels) {
        if (id == panel.id) {
            return true;
        }
    }
    return false;
}

// A disabled toolbar button for a feature that is not built yet.
ui::Button& addComingSoonButton(ui::Row& bar, ui::Icon icon, std::string text, std::string tooltip)
{
    auto& button = bar.emplace<ui::Button>(std::move(text));
    button.setIcon(icon);
    button.setStyle(ui::Button::Style::Subtle);
    button.setEnabled(false);
    button.setTooltip(std::move(tooltip));
    return button;
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
    // Order matters: each dock splits the group it is placed beside, so the
    // left column and the Inspector are added first to span the full height.
    ui::DockLayout layout = ui::DockLayout::tabs({kViewportPanel});
    layout.dock(kHierarchyPanel, kViewportPanel, ui::DockSide::Left, 0.21f);
    layout.dock(kAssetsPanel, kHierarchyPanel, ui::DockSide::Bottom, 0.45f);
    layout.dock(kInspectorPanel, kViewportPanel, ui::DockSide::Right, 0.29f);
    layout.dock(kPreviewPanel, kViewportPanel, ui::DockSide::Bottom, 0.32f);
    layout.dock(kConsolePanel, kPreviewPanel, ui::DockSide::Center);
    layout.activate(kConsolePanel);
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
        for (const PanelInfo& panel : kPanels) {
            const char* id = panel.id;
            ui::MenuItem item;
            item.label = panel.title;
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

    // --- Main toolbar ---------------------------------------------------------------
    //   [scene] [play] [reload]  ......................  [build] [project folder]
    auto& toolbar = root->emplace<ui::Row>(4.0f, ui::Insets::symmetric(8.0f, 0.0f));
    toolbar.setBackground(theme.panelHeader);
    toolbar.setCrossAlign(ui::CrossAlign::Center);
    toolbar.setFixedSize({0.0f, theme.toolbarHeight});
    toolbar.emplace<ui::Spacer>();
    auto& scene = toolbar.emplace<ui::ValueBox>("Start Scene", ui::ValueBox::Kind::Choice);
    scene.setStretch(0.0f);
    scene.setFixedSize({190.0f, 0.0f});
    scene.setTooltip(std::string("The scene being edited. Choosing scenes: ") + kComingWithScenes);
    addComingSoonButton(toolbar, ui::Icon::Play, "", std::string("Play the game in the editor. ") + kComingLater)
        .setFixedSize({30.0f, 0.0f});
    addComingSoonButton(toolbar, ui::Icon::Refresh, "", std::string("Reload the scene. ") + kComingWithScenes)
        .setFixedSize({30.0f, 0.0f});
    toolbar.emplace<ui::Spacer>();
    addComingSoonButton(toolbar, ui::Icon::Send, "Build", kComingWithBuilds);
    addComingSoonButton(toolbar, ui::Icon::Folder, "", std::string("Open the project folder. ") + kComingWithScenes)
        .setFixedSize({30.0f, 0.0f});

    // --- Panels ------------------------------------------------------------------
    auto& dock = root->emplace<ui::DockSpace>();
    dock.setStretch(1.0f);
    for (const PanelInfo& panel : kPanels) {
        std::unique_ptr<ui::Widget> content;
        const std::string id = panel.id;
        if (id == kViewportPanel) {
            auto p = std::make_unique<ViewportPanel>();
            m_viewport = p.get();
            content = std::move(p);
        } else if (id == kHierarchyPanel) {
            auto p = std::make_unique<HierarchyPanel>(m_state);
            m_hierarchy = p.get();
            content = std::move(p);
        } else if (id == kInspectorPanel) {
            auto p = std::make_unique<InspectorPanel>(m_state);
            m_inspector = p.get();
            content = std::move(p);
        } else if (id == kConsolePanel) {
            auto p = std::make_unique<ConsolePanel>(m_console);
            m_consolePanel = p.get();
            content = std::move(p);
        } else if (id == kAssetsPanel) {
            content = std::make_unique<AssetsPanel>();
        } else {
            content = std::make_unique<AssetsPreviewPanel>();
        }
        dock.addPanel(id, panel.title, std::move(content), panel.icon);
    }
    dock.setDefaultLayout(defaultLayout());
    dock.setOnLayoutChanged([this, &ctx] {
        m_layoutDirty = true;
        m_layoutChangedAt = ctx.time();
    });
    m_dock = &dock;

    // --- Status bar ----------------------------------------------------------------
    //   60 fps  ........................  (i) 12  (!) 0  (x) 0   Version 0.2.0
    auto& status = root->emplace<ui::Row>(5.0f, ui::Insets::symmetric(theme.padding, 0.0f));
    status.setBackground(theme.panelHeader);
    status.setCrossAlign(ui::CrossAlign::Center);
    status.setFixedSize({0.0f, theme.statusBarHeight});
    m_statusFps = &status.emplace<ui::Label>("", ui::Label::Style::Small);
    m_statusFps->setColor(theme.textFaint);
    m_statusFps->setTooltip("Frames drawn per second");
    status.emplace<ui::Spacer>();
    const std::pair<ui::Icon, ui::Color> counters[] = {
        {ui::Icon::Info, theme.debug}, {ui::Icon::Warning, theme.warning}, {ui::Icon::Error, theme.error}};
    const char* tips[] = {"Messages in the Console", "Warnings in the Console", "Errors in the Console"};
    for (std::size_t i = 0; i < 3; ++i) {
        status.emplace<ui::IconView>(counters[i].first, 13.0f, counters[i].second);
        m_statusCounts[i] = &status.emplace<ui::Label>("0", ui::Label::Style::Small);
        m_statusCounts[i]->setColor(theme.textDim);
        m_statusCounts[i]->setTooltip(tips[i]);
        status.emplace<ui::Spacer>(0.0f).setFixedSize({6.0f, 1.0f});
    }
    status.emplace<ui::Spacer>(0.0f).setFixedSize({6.0f, 1.0f});
    auto& version = status.emplace<ui::Label>("Version " + std::string(versionString()), ui::Label::Style::Small);
    version.setColor(theme.textDim);

    ctx.setRoot(std::move(root));

    // Cmd+Q (macOS) / Ctrl+Q quits from anywhere in the editor.
    ui::Modifiers primary;
    (ctx.platform().macConventions ? primary.super : primary.ctrl) = true;
    ctx.addShortcut(ui::Key::Q, primary, [this] { m_quitRequested = true; });
}

void EditorApp::updateStatusBar(UiFrame& frame)
{
    m_statusFps->setText(std::to_string(static_cast<int>(frame.framesPerSecond() + 0.5f)) + " fps");
    const std::size_t messages = m_console.count(LogLevel::Trace) + m_console.count(LogLevel::Debug)
                                 + m_console.count(LogLevel::Info);
    m_statusCounts[0]->setText(std::to_string(messages));
    m_statusCounts[1]->setText(std::to_string(m_console.count(LogLevel::Warning)));
    m_statusCounts[2]->setText(std::to_string(m_console.count(LogLevel::Error)));
}

void EditorApp::showAbout(ui::Context& ctx)
{
    auto dialog = std::make_unique<ui::Dialog>("About Lantern Engine", 400.0f);
    ui::Column& body = dialog->body();
    body.emplace<ui::Label>("Lantern Engine " + std::string(versionString()), ui::Label::Style::Strong);
    body.emplace<ui::Label>("A C++-first 2D game engine and editor.", ui::Label::Style::Dim);
    // The runtime backend is an implementation detail, so it is not named here.
    // It is still reported by --version and in the Console, for bug reports.
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
    m_statusFps = nullptr;
    m_statusCounts = {};
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
