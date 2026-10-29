#pragma once

// The editor application: owns the open project and scene document, builds
// the workspace (menu bar, toolbar, dockable panels, status bar) from Lantern
// UI widgets, runs the file workflow (new/open/save, unsaved-changes
// prompts), and remembers layout and preferences between sessions.

#include "../model/CommandLine.hpp"
#include "../model/ConsoleLog.hpp"
#include "../model/EditorState.hpp"
#include "../model/Preferences.hpp"
#include "../model/SceneDocument.hpp"
#include "../panels/Panels.hpp"

#include "lantern/core/Engine.hpp"
#include "lantern/runtime/Application.hpp"
#include "lantern/scene/Project.hpp"
#include "lantern/ui/DockLayout.hpp"

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

namespace lantern::ui {
class Context;
class DockSpace;
class Label;
} // namespace lantern::ui

namespace lantern::editor {

class EditorApp final : public EditorServices {
public:
    EditorApp(Engine& engine, ConsoleLog& console, EditorOptions options);

    /// Opens the editor window and blocks until it closes.
    Status run();

    /// The panel arrangement used on first start and by View > Reset Layout,
    /// the classic game-editor layout:
    ///   +-----------+-----------------------------+-----------+
    ///   | Hierarchy |            Scene            | Inspector |
    ///   +-----------+                             |           |
    ///   |  Assets   +-----------------------------+           |
    ///   |           | Assets Preview | [Console]  |           |
    ///   +-----------+-----------------------------+-----------+
    static ui::DockLayout defaultLayout();

    // --- EditorServices ------------------------------------------------------------------
    SceneDocument& document() override { return *m_document; }
    std::uint64_t documentGeneration() const override { return m_documentGeneration; }
    const ComponentRegistry& registry() const override { return m_registry; }
    const Project* project() const override { return m_project ? &*m_project : nullptr; }
    ui::Context& ui() override { return *m_ui; }
    EntityId selection() const override;
    bool isSceneSelected() const override { return m_state.selectedNodeId == EditorState::kSceneSelected; }
    void select(EntityId id) override;
    void selectScene() override;
    AssetDatabase* assets() override { return m_assetDb.get(); }
    AssetId selectedAsset() const override { return m_selectedAsset; }
    void selectAsset(AssetId id) override;
    std::string importFolder() const override { return m_importFolder; }
    void setImportFolder(std::string folder) override { m_importFolder = std::move(folder); }
    void importFiles(const std::vector<std::filesystem::path>& files) override;
    void placeAsset(AssetId asset, Vec2 worldPosition) override;
    void dropAsset(AssetId asset, ui::Vec2 windowPoint) override;
    bool execute(std::unique_ptr<Command> command) override;
    void endEdit() override;
    void createEntity(NewEntityKind kind, bool asChildOfSelection) override;
    void deleteSelection() override;
    void duplicateSelection() override;
    void renameSelection() override;
    void openScene(const std::filesystem::path& path) override;
    void newScene() override;
    void newProject() override;
    void openProject() override;
    void showMessage(const std::string& title, const std::string& text) override;
    void showImportDialog() override;

private:
    // Called by the runtime (see PresentationSettings).
    void setupUi(UiFrame& frame);
    void updateUi(UiFrame& frame);
    void shutdownUi(UiFrame& frame);
    bool closeRequested();

    void buildWorkspace(UiFrame& frame);
    void buildMenus(ui::Widget& root);
    void buildToolbar(ui::Widget& root);
    void addShortcuts();
    void restoreLayout();
    void updateStatusBar(UiFrame& frame);
    void updateTitle(UiFrame& frame);
    void showAbout();

    // --- Files ---------------------------------------------------------------------------
    /// Opens the scene, project or start scene the editor should begin with.
    void openInitialContent();
    bool openProjectFolder(const std::filesystem::path& folder, bool reportErrors);
    void setDocument(std::unique_ptr<SceneDocument> document);
    void saveScene(std::function<void()> then = {});
    void saveSceneAs(std::function<void()> then = {});
    void reloadScene();
    void moveSelection(int direction);
    /// Runs `then` once unsaved changes are saved or discarded (asks first);
    /// does nothing if the user cancels.
    void confirmDiscard(const std::string& action, std::function<void()> then);
    void showNewProjectDialog(bool keepCurrentScene, std::function<void()> then = {});
    void showOpenProjectDialog();
    void reportIssues(const std::filesystem::path& file, const std::vector<FileIssue>& issues);
    /// Re-reads the project's assets (also every few seconds) and reports problems.
    void refreshAssets();
    void checkSceneAssets();


    void loadSettings();
    void saveSettings();
    void saveLayout();

    Logger& log() { return m_engine.logger(); }

    Engine& m_engine;
    ConsoleLog& m_console;
    EditorOptions m_options;
    ComponentRegistry m_registry = ComponentRegistry::withBuiltins();

    EditorState m_state;
    Preferences m_preferences;
    std::filesystem::path m_configDir;   // empty = do not persist anything

    std::optional<Project> m_project;
    std::unique_ptr<AssetDatabase> m_assetDb;                       // the project's assets
    std::vector<std::unique_ptr<AssetDatabase>> m_retiredAssetDbs;  // kept until the runtime lets go
    const AssetDatabase* m_runtimeAssets = nullptr;                 // what the runtime draws from
    double m_lastAssetScan = -100.0;
    AssetId m_selectedAsset;
    std::string m_importFolder = "assets";
    std::unique_ptr<SceneDocument> m_document;
    std::uint64_t m_documentGeneration = 1;
    std::shared_ptr<const Scene> m_shownScene;   // what the runtime is drawing
    std::string m_shownTitle;

    // Widgets (owned by the UI context; valid between setupUi and shutdownUi).
    ui::Context* m_ui = nullptr;
    ui::DockSpace* m_dock = nullptr;
    ViewportPanel* m_viewport = nullptr;
    HierarchyPanel* m_hierarchy = nullptr;
    AssetsPanel* m_assets = nullptr;
    AssetsPreviewPanel* m_preview = nullptr;
    InspectorPanel* m_inspector = nullptr;
    ConsolePanel* m_consolePanel = nullptr;
    ui::ChoiceField* m_sceneChoice = nullptr;
    std::vector<std::string> m_sceneChoices;   // project-relative paths behind the choice
    ui::Label* m_statusFps = nullptr;
    std::array<ui::Label*, 3> m_statusCounts{};   // messages, warnings, errors

    bool m_quitRequested = false;   // set once the user has confirmed quitting
    bool m_quitting = false;        // a quit is being confirmed (avoid two prompts)
    bool m_renamePending = false;   // focus the Inspector's name field after its next update
    bool m_layoutDirty = false;
    double m_layoutChangedAt = 0.0;
};

} // namespace lantern::editor
