#pragma once

// The editor's panels, built from Lantern UI widgets.
//
// Each panel is a widget that lives in the editor's DockSpace for the whole
// session. Once per frame the editor calls update(), which refreshes what the
// panel shows. Panels never change the scene directly: they ask
// EditorServices to run a Command, so every edit can be undone.
//
// Controls for features that do not exist yet are shown disabled, with a
// tooltip saying which milestone brings them (kComing* below), so the
// editor's layout is complete without pretending to do more than it does.

#include "../model/ConsoleLog.hpp"
#include "../model/EditorState.hpp"
#include "../model/SceneDocument.hpp"

#include "lantern/assets/AssetDatabase.hpp"
#include "lantern/runtime/Application.hpp"
#include "lantern/scene/Project.hpp"
#include "lantern/ui/widgets/Basic.hpp"
#include "lantern/ui/widgets/Box.hpp"
#include "lantern/ui/widgets/Fields.hpp"
#include "lantern/ui/widgets/Inspector.hpp"
#include "lantern/ui/widgets/Scroll.hpp"
#include "lantern/ui/widgets/TextField.hpp"
#include "lantern/ui/widgets/Tree.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace lantern::editor {

// Panel ids are keys in the saved layout file, so they must never change.
inline constexpr const char* kViewportPanel = "viewport";
inline constexpr const char* kHierarchyPanel = "hierarchy";
inline constexpr const char* kAssetsPanel = "assets";
inline constexpr const char* kInspectorPanel = "inspector";
inline constexpr const char* kPreviewPanel = "preview";
inline constexpr const char* kConsolePanel = "console";

// Tooltips for controls that are visible but not working yet.
inline constexpr const char* kComingWithAssets = "Coming in Milestone 4, with importing images and other assets";
inline constexpr const char* kComingWithPlayMode = "Coming in Milestone 5, with gameplay code and play mode";
inline constexpr const char* kComingWithBuilds = "Coming in Milestone 8: building your game for players";

enum class NewEntityKind { Empty, Rectangle, Ellipse, Sprite, Camera };

/// What panels can ask the editor to do. Implemented by EditorApp.
class EditorServices {
public:
    virtual ~EditorServices() = default;

    virtual SceneDocument& document() = 0;
    /// Goes up whenever another document is opened (scene addresses can be reused).
    virtual std::uint64_t documentGeneration() const = 0;
    virtual const ComponentRegistry& registry() const = 0;
    /// The open project, if any.
    virtual const Project* project() const = 0;
    /// The open project's assets (null without a project).
    virtual AssetDatabase* assets() = 0;
    virtual ui::Context& ui() = 0;

    // Selection.
    virtual EntityId selection() const = 0;
    virtual bool isSceneSelected() const = 0;
    virtual void select(EntityId id) = 0;
    virtual void selectScene() = 0;
    /// Selecting an asset (in Assets or Assets Preview) shows it in the Inspector.
    virtual AssetId selectedAsset() const = 0;
    virtual void selectAsset(AssetId id) = 0;
    /// The project folder new imports go into (project-relative, e.g. "assets").
    virtual std::string importFolder() const = 0;
    virtual void setImportFolder(std::string folder) = 0;
    /// Copies files into the project as assets (they get IDs and .meta files).
    virtual void importFiles(const std::vector<std::filesystem::path>& files) = 0;
    /// Creates an object showing `asset` (an image becomes a Sprite) at a
    /// world position, e.g. when an asset is dropped on the Scene view.
    virtual void placeAsset(AssetId asset, Vec2 worldPosition) = 0;
    /// Called by a widget dragging an asset when the mouse is released at
    /// `windowPoint`; the editor decides what the drop means there.
    virtual void dropAsset(AssetId asset, ui::Vec2 windowPoint) = 0;

    /// Runs a command on the open scene (undoable). False if it changed nothing.
    virtual bool execute(std::unique_ptr<Command> command) = 0;
    /// Ends a continuous edit (e.g. a drag), so the next edit is a new undo step.
    virtual void endEdit() = 0;

    // Actions shared by menus, toolbars and panels.
    virtual void createEntity(NewEntityKind kind, bool asChildOfSelection) = 0;
    virtual void deleteSelection() = 0;
    virtual void duplicateSelection() = 0;
    virtual void renameSelection() = 0;
    virtual void openScene(const std::filesystem::path& path) = 0;
    virtual void newScene() = 0;
    virtual void newProject() = 0;
    virtual void openProject() = 0;
    virtual void showMessage(const std::string& title, const std::string& text) = 0;
    /// Asks for files to import (the dialog explains drag and drop too).
    virtual void showImportDialog() = 0;
};

/// A toolbar strip along the top of a panel.
ui::Row& makePanelToolbar(ui::Widget& parent);
/// A small icon-only button for toolbars. Without an action it is shown
/// disabled; its tooltip then says when the feature arrives.
ui::Button& addToolButton(ui::Widget& toolbar, ui::Icon icon, std::string tooltip, std::function<void()> action = {});
/// Opens a menu just below `anchor`.
void openMenuAt(ui::Context& ctx, ui::Rect anchor, std::vector<ui::MenuItem> items);

/// The game world, scaled to fit. Click an object to select it.
/// The tools of the Scene view (W, E, R, T as in most editors).
enum class SceneTool { Move, Rotate, Scale, Rect };

/// The scene as you edit it: pan and zoom freely, click to select, and move,
/// rotate, scale or resize the selected object with the tools.
class ViewportPanel final : public ui::Column {
public:
    explicit ViewportPanel(EditorServices& editor);
    void update(UiFrame& frame);

    /// The world point under a window point, if it is over the Scene view.
    std::optional<Vec2> worldAt(ui::Vec2 windowPoint) const;
    void setTool(SceneTool tool);
    SceneTool tool() const noexcept { return m_tool; }
    /// Centres the view on the selected object (F).
    void frameSelection();
    /// Shows the whole game view (what the player sees).
    void fitGameView();

private:
    class SceneView;
    void openZoomMenu();

    EditorServices& m_editor;
    SceneView* m_view = nullptr;
    ui::Button* m_zoom = nullptr;
    std::array<ui::Button*, 4> m_toolButtons{};
    SceneTool m_tool = SceneTool::Move;
};

/// The scene's objects as a tree; select, create, delete, search.
class HierarchyPanel final : public ui::Column {
public:
    explicit HierarchyPanel(EditorServices& editor);
    void update(UiFrame& frame);

private:
    class SceneTree;
    void rebuild();

    EditorServices& m_editor;
    SceneTree* m_tree = nullptr;
    ui::Button* m_create = nullptr;
    std::string m_filter;
    std::uint64_t m_shownGeneration = 0;
    std::uint64_t m_shownRevision = 0;
    std::string m_shownFilter;
};

/// The project's files: double-click a scene to open it.
class AssetsPanel final : public ui::Column {
public:
    explicit AssetsPanel(EditorServices& editor);
    void update(UiFrame& frame);
    /// Re-reads the project folder (also done every few seconds).
    void refresh();

private:
    void rebuild();

    EditorServices& m_editor;
    ui::Widget* m_noProject = nullptr;
    ui::Widget* m_browser = nullptr;
    ui::TreeView* m_tree = nullptr;
    std::string m_filter;
    std::vector<std::string> m_paths;   // project-relative; item id = index + 1
    const Project* m_shownProject = nullptr;
    std::filesystem::path m_shownFolder;
    double m_lastScan = -100.0;
    std::uint64_t m_shownAssets = 0;
};

/// Thumbnails of the folder selected in Assets (Milestone 4).
/// The contents of the folder selected in Assets, as thumbnails. Click to
/// inspect, double-click to open, drag an image into the Scene view.
class AssetsPreviewPanel final : public ui::Column {
public:
    explicit AssetsPreviewPanel(EditorServices& editor);
    void update(UiFrame& frame);

private:
    class Grid;
    EditorServices& m_editor;
    ui::Label* m_path = nullptr;
    Grid* m_grid = nullptr;
};

/// The selected object's (or the scene's) properties, editable.
class InspectorPanel final : public ui::Column {
public:
    explicit InspectorPanel(EditorServices& editor);
    void update(UiFrame& frame);
    /// Puts keyboard focus in the name field (Edit > Rename).
    void focusName();

private:
    void rebuild();
    void buildEntity(ui::Column& content, const Entity& entity);
    void buildScene(ui::Column& content);
    void buildAsset(ui::Column& content, const AssetRecord& asset);
    void showCreateTilesetDialog(AssetId image);
    void confirmDeleteAsset(AssetId id);
    void buildComponent(ui::Column& content, const Entity& entity, const Component& component);
    void addPropertyEditor(ui::Section& section, EntityId id, const std::string& typeId, const PropertyInfo& info);
    void openComponentMenu(ui::Section& section, EntityId id, const std::string& typeId);
    void openAddComponentMenu(ui::Button& button, EntityId id);
    std::string signature() const;

    EditorServices& m_editor;
    ui::Widget* m_empty = nullptr;
    ui::ScrollView* m_details = nullptr;
    ui::TextField* m_name = nullptr;
    std::string m_builtFor;                      // signature() when last built
    UiFrame* m_frame = nullptr;                  // valid during update() (thumbnails)
    std::vector<std::function<void()>> m_refresh; // copy scene values into the widgets
};

/// Log messages from the engine, the backend and the editor.
class ConsolePanel final : public ui::Column {
public:
    explicit ConsolePanel(ConsoleLog& log);
    void update(UiFrame& frame);

private:
    struct LevelToggle {
        LogLevel level;
        const char* name;
        bool ConsoleFilter::*shown;
        ui::Checkbox* checkbox = nullptr;
    };

    void refresh();
    void paintHeader(ui::Context& ctx, ui::DrawList& draw, ui::Rect rect) const;
    void paintRow(ui::Context& ctx, ui::DrawList& draw, const ui::RowInfo& row) const;

    ConsoleLog& m_log;
    ConsoleFilter m_filter;
    std::vector<ConsoleEntry> m_entries;   // what the list shows
    std::uint64_t m_shownTotal = UINT64_MAX;
    std::size_t m_shownSize = 0;
    bool m_dirty = true;

    std::array<LevelToggle, 4> m_toggles;
    ui::ListView* m_list = nullptr;
    ui::Checkbox* m_autoScroll = nullptr;
};

} // namespace lantern::editor
