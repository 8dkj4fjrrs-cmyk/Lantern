#pragma once

// The editor's panels, built from Lantern UI widgets.
//
// Each panel is a widget that lives in the editor's DockSpace for the whole
// session. Once per frame the editor calls update(), which refreshes what the
// panel shows from the running scene or the console log. Panels read and
// change editor state from editor/model; they never talk to the backend
// directly, only through lantern::UiFrame.

#include "../model/ConsoleLog.hpp"
#include "../model/EditorState.hpp"

#include "lantern/runtime/Application.hpp"
#include "lantern/ui/widgets/Basic.hpp"
#include "lantern/ui/widgets/Box.hpp"
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
inline constexpr const char* kInspectorPanel = "inspector";
inline constexpr const char* kConsolePanel = "console";

/// The game world as the player would see it, scaled to fit the panel.
class ViewportPanel final : public ui::ImageView {
public:
    ViewportPanel();
    void update(UiFrame& frame);
};

/// Tree of objects in the running scene; clicking selects.
class HierarchyPanel final : public ui::TreeView {
public:
    explicit HierarchyPanel(EditorState& state);
    void update(UiFrame& frame);

private:
    EditorState& m_state;
};

/// Properties of the selected object (read-only until Milestone 3).
class InspectorPanel final : public ui::PropertyList {
public:
    explicit InspectorPanel(EditorState& state);
    void update(UiFrame& frame);

private:
    EditorState& m_state;
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
