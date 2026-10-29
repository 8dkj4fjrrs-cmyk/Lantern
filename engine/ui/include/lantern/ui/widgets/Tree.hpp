#pragma once

// TreeView: an expandable, selectable tree (the Hierarchy panel).
// PropertyList: label/value rows (the Inspector panel, read-only for now).

#include "lantern/ui/Icons.hpp"
#include "lantern/ui/widgets/Scroll.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace lantern::ui {

struct TreeItem {
    std::uint64_t id = 0;        // unique and non-zero
    std::uint64_t parentId = 0;  // 0 = top level
    int depth = 0;
    std::string label;
    std::string detail;          // shown dimmed after the label
    bool hasChildren = false;
    std::optional<Icon> icon;    // drawn before the label
};

class TreeView : public Widget {
public:
    TreeView();

    /// Items in tree order: each parent directly followed by its descendants.
    void setItems(std::vector<TreeItem> items);
    const std::vector<TreeItem>& items() const noexcept { return m_items; }

    std::uint64_t selectedId() const noexcept { return m_selected; }
    /// Selects without notifying (e.g. when the selection is restored).
    void setSelectedId(std::uint64_t id) { m_selected = id; }
    void setOnSelect(std::function<void(std::uint64_t)> onSelect) { m_onSelect = std::move(onSelect); }

    /// Items shallower than this start expanded (until the user collapses them).
    void setAutoExpandDepth(int depth) { m_autoExpandDepth = depth; }
    bool isExpanded(const TreeItem& item) const;
    void setExpanded(std::uint64_t id, bool expanded);

    void setEmptyText(std::string text) { m_emptyText = std::move(text); }

    /// Indices into items() of the rows currently shown (collapsed branches hidden).
    const std::vector<std::size_t>& visibleRows() const noexcept { return m_visible; }

    Size measure(Context& ctx, Size available) override;
    void arrange(Context& ctx, Rect rect) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onScroll(Context& ctx, const ScrollEvent& event) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    void onPointerMove(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;
    bool onKeyDown(Context& ctx, const KeyEvent& event) override;

private:
    void rebuildVisible();
    std::optional<std::size_t> visibleRowAt(Vec2 point) const;
    Rect chevronRect(const Rect& row, int depth, const Theme& theme) const;
    void select(Context& ctx, std::uint64_t id);
    void revealSelection(Context& ctx);

    std::vector<TreeItem> m_items;
    std::vector<std::size_t> m_visible;
    std::unordered_set<std::uint64_t> m_expanded;   // explicitly opened
    std::unordered_set<std::uint64_t> m_collapsed;  // explicitly closed
    int m_autoExpandDepth = 2;
    std::uint64_t m_selected = 0;
    std::function<void(std::uint64_t)> m_onSelect;
    std::string m_emptyText;
    ScrollState m_scroll;
    float m_rowHeight = 24.0f;
};

struct PropertyRow {
    std::string label;
    std::string value;
    bool isHeading = false;  // a section title spanning both columns
};

class PropertyList : public Widget {
public:
    PropertyList() = default;

    void setTitle(std::string title, std::string subtitle = {});
    void setRows(std::vector<PropertyRow> rows) { m_rows = std::move(rows); }
    /// Shown below the rows, wrapped (e.g. a hint about editing).
    void setFootnote(std::string text) { m_footnote = std::move(text); }
    /// Shown instead of everything when there is nothing to inspect.
    void setEmptyText(std::string text) { m_emptyText = std::move(text); }
    void setEmpty(bool empty) { m_empty = empty; }

    Size measure(Context& ctx, Size available) override;
    void arrange(Context& ctx, Rect rect) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onScroll(Context& ctx, const ScrollEvent& event) override;

private:
    float contentHeight(Context& ctx, float width) const;

    std::string m_title;
    std::string m_subtitle;
    std::vector<PropertyRow> m_rows;
    std::string m_footnote;
    std::string m_emptyText;
    bool m_empty = true;
    ScrollState m_scroll;
};

} // namespace lantern::ui
