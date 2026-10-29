#pragma once

// DockSpace: the editor's main area. Shows panels in tab groups arranged by a
// DockLayout, and lets the user
//   * drag the gaps between groups to resize them,
//   * click tabs to switch, and the "x" to close a panel,
//   * drag a tab onto another group's centre (to join it as a tab) or onto one
//     of its edges (to split the group) — a highlighted preview shows where.

#include "lantern/ui/DockLayout.hpp"
#include "lantern/ui/Widget.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace lantern::ui {

class DockSpace : public Widget {
public:
    DockSpace() = default;

    /// Registers a panel. `id` is used in saved layouts, so keep it stable.
    template <class T>
    T& addPanel(std::string id, std::string title, std::unique_ptr<T> content)
    {
        T& ref = addChild(std::move(content));
        m_panels.push_back(Panel{std::move(id), std::move(title), &ref});
        return ref;
    }

    /// The arrangement used at first start and by resetLayout().
    void setDefaultLayout(DockLayout layout);
    const DockLayout& defaultLayout() const noexcept { return m_default; }
    void setLayout(DockLayout layout);
    const DockLayout& layout() const noexcept { return m_layout; }
    void resetLayout();

    bool isOpen(std::string_view id) const { return m_layout.contains(id); }
    /// Closes or re-opens a panel (re-opened panels go back near their default place).
    void setOpen(const std::string& id, bool open);
    std::vector<std::string> panelIds() const;
    std::string titleOf(std::string_view id) const;

    /// Called whenever the user changes the layout (to save it).
    void setOnLayoutChanged(std::function<void()> callback) { m_onChanged = std::move(callback); }

    void arrange(Context& ctx, Rect rect) override;
    void paint(Context& ctx, DrawList& draw) override;
    Widget* hitTest(Vec2 point) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    void onPointerMove(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;
    CursorShape cursor(Vec2 point) const override;

    /// Screen rectangle of a panel's tab (empty if hidden). For tests.
    Rect tabRect(std::string_view id) const;

private:
    struct Panel {
        std::string id;
        std::string title;
        Widget* content;
    };
    struct Tab {
        std::string id;
        Rect rect;
        Rect close;
        bool active;
    };
    struct DropTarget {
        std::string anchor;
        DockSide side;
        Rect preview;
    };
    enum class Drag { None, Splitter, Tab };

    const Panel* findPanel(std::string_view id) const;
    const DockLayout::SplitterRect* splitterAt(Vec2 point) const;
    const Tab* tabAt(Vec2 point) const;
    std::optional<DropTarget> dropTargetAt(Vec2 point) const;
    void changed();

    std::vector<Panel> m_panels;
    DockLayout m_layout;
    DockLayout m_default;
    std::function<void()> m_onChanged;

    // Geometry from the last arrange().
    std::vector<DockLayout::GroupRect> m_groups;
    std::vector<DockLayout::SplitterRect> m_splitters;
    std::vector<Tab> m_tabs;
    std::vector<Rect> m_contentRects; // per group, below the tab bar

    // Interaction.
    Drag m_drag = Drag::None;
    DockLayout::Node* m_dragSplit = nullptr;
    Rect m_dragArea;
    std::string m_dragPanel;
    Vec2 m_pressPosition;
    bool m_tabDragging = false;
    bool m_pressedClose = false;
    std::optional<DropTarget> m_drop;
};

} // namespace lantern::ui
