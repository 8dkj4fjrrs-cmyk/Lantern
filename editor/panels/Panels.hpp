#pragma once

// The editor's panels, built from Lantern UI widgets.
//
// Each panel is a widget that lives in the editor's DockSpace for the whole
// session. Once per frame the editor calls update(), which refreshes what the
// panel shows from the running scene or the console log. Panels read and
// change editor state from editor/model; they never talk to the backend
// directly, only through lantern::UiFrame.
//
// Controls for features that do not exist yet are shown disabled, with a
// tooltip saying which milestone brings them (see kComingIn* below), so the
// editor's layout is complete without pretending to do more than it does.

#include "../model/ConsoleLog.hpp"
#include "../model/EditorState.hpp"

#include "lantern/runtime/Application.hpp"
#include "lantern/ui/widgets/Basic.hpp"
#include "lantern/ui/widgets/Box.hpp"
#include "lantern/ui/widgets/Inspector.hpp"
#include "lantern/ui/widgets/Scroll.hpp"
#include "lantern/ui/widgets/TextField.hpp"
#include "lantern/ui/widgets/Tree.hpp"

#include <array>
#include <cstdint>
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
inline constexpr const char* kComingWithScenes = "Coming in Milestone 3, with Lantern scenes and projects";
inline constexpr const char* kComingWithEditing = "Coming with scene editing (Milestone 3)";
inline constexpr const char* kComingWithBuilds = "Coming in Milestone 8: building your game for players";
inline constexpr const char* kComingLater = "Coming in a later milestone";

/// A toolbar strip along the top of a panel.
ui::Row& makePanelToolbar(ui::Widget& parent);
/// A small icon-only button for toolbars. Without an action it is shown
/// disabled; its tooltip then says when the feature arrives.
ui::Button& addToolButton(ui::Widget& toolbar, ui::Icon icon, std::string tooltip, std::function<void()> action = {});

/// The game world as the player would see it, scaled to fit the panel.
class ViewportPanel final : public ui::Column {
public:
    ViewportPanel();
    void update(UiFrame& frame);

private:
    ui::ImageView* m_image = nullptr;
    ui::Label* m_zoom = nullptr;
};

/// Tree of objects in the running scene; clicking selects, typing filters.
class HierarchyPanel final : public ui::Column {
public:
    explicit HierarchyPanel(EditorState& state);
    void update(UiFrame& frame);

private:
    EditorState& m_state;
    ui::TreeView* m_tree = nullptr;
    std::string m_filter;
};

/// The project's files. Projects arrive in Milestone 3; until then it says so.
class AssetsPanel final : public ui::Column {
public:
    AssetsPanel();
};

/// Previews the contents of the folder selected in Assets (Milestone 3).
class AssetsPreviewPanel final : public ui::Column {
public:
    AssetsPreviewPanel();
};

/// Properties of the selected object (read-only until Milestone 3).
class InspectorPanel final : public ui::Column {
public:
    explicit InspectorPanel(EditorState& state);
    void update(UiFrame& frame);

private:
    struct Fields {
        ui::ValueBox* active = nullptr;
        ui::ValueBox* name = nullptr;
        ui::Section* node = nullptr;
        std::array<ui::ValueBox*, 2> position{};
        ui::ValueBox* rotation = nullptr;
        std::array<ui::ValueBox*, 2> scale{};
        std::array<ui::ValueBox*, 2> size{};
        ui::Section* rendering = nullptr;
        ui::ValueBox* kind = nullptr;
        ui::ValueBox* drawOrder = nullptr;
        ui::ValueBox* visible = nullptr;
        ui::Section* hierarchy = nullptr;
        ui::ValueBox* parent = nullptr;
        ui::ValueBox* children = nullptr;
        ui::ValueBox* sessionId = nullptr;
    };

    void showHelp(const char* title, const char* text);

    EditorState& m_state;
    ui::Widget* m_empty = nullptr;    // shown when nothing is selected
    ui::ScrollView* m_details = nullptr;
    Fields m_fields;
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
