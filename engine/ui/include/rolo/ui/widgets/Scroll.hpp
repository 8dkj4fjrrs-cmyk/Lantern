#pragma once

// Scrolling.
//
//   ScrollState  vertical scroll position + a draggable scrollbar; shared by
//                every scrolling widget.
//   ScrollView   scrolls one child widget of any height.
//   ListView     a "virtual" list: only the rows on screen are drawn, so it
//                stays fast with tens of thousands of rows (the Console).

#include "rolo/ui/Theme.hpp"
#include "rolo/ui/Widget.hpp"

#include <cstddef>
#include <functional>
#include <optional>

namespace rolo::ui {

class ScrollState {
public:
    float offset() const noexcept { return m_offset; }
    float maxOffset() const noexcept;
    bool atEnd() const noexcept { return m_offset >= maxOffset() - 0.5f; }

    void setExtent(float contentHeight, float viewHeight);
    void setOffset(float offset);
    void scrollBy(float delta) { setOffset(m_offset + delta); }
    void scrollToEnd() { setOffset(maxOffset()); }
    /// Scrolls the least amount so that [top, bottom] (content coordinates) is visible.
    void ensureVisible(float top, float bottom);

    /// The scrollbar sits inside `view` along its right edge.
    Rect trackRect(Rect view, const Theme& theme) const;
    std::optional<Rect> thumbRect(Rect view, const Theme& theme) const;

    /// Returns true if the press grabbed the scrollbar.
    bool pointerDown(Rect view, const Theme& theme, Vec2 point);
    void pointerMove(Rect view, const Theme& theme, Vec2 point);
    void pointerUp() { m_dragging = false; }
    bool isDragging() const noexcept { return m_dragging; }

    void paint(DrawList& draw, Rect view, const Theme& theme, bool hovered) const;

private:
    float m_offset = 0.0f;
    float m_content = 0.0f;
    float m_view = 0.0f;
    bool m_dragging = false;
    float m_grab = 0.0f; // where on the thumb the drag started
};

class ScrollView : public Widget {
public:
    ScrollView() = default;

    /// The scrolled content (replaces any previous one).
    template <class T>
    T& setContent(std::unique_ptr<T> content)
    {
        clearChildren();
        return addChild(std::move(content));
    }

    ScrollState& scroll() noexcept { return m_scroll; }

    Size measure(Context& ctx, Size available) override;
    void arrange(Context& ctx, Rect rect) override;
    void paint(Context& ctx, DrawList& draw) override;
    Widget* hitTest(Vec2 point) override;
    bool onScroll(Context& ctx, const ScrollEvent& event) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    void onPointerMove(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;

private:
    ScrollState m_scroll;
};

/// What a ListView row painter is told about the row it draws.
struct RowInfo {
    std::size_t index = 0;
    Rect rect;             // where to draw it
    bool selected = false;
    bool hovered = false;
    bool focused = false;  // the list has keyboard focus
};

class ListView : public Widget {
public:
    using RowPainter = std::function<void(Context&, DrawList&, const RowInfo&)>;

    ListView();

    void setRowCount(std::size_t count);
    std::size_t rowCount() const noexcept { return m_rowCount; }
    void setRowHeight(float height) { m_rowHeight = height; }
    float rowHeight() const noexcept { return m_rowHeight; }
    void setRowPainter(RowPainter painter) { m_painter = std::move(painter); }

    /// Optional fixed header drawn above the rows (e.g. column titles).
    void setHeader(float height, std::function<void(Context&, DrawList&, Rect)> painter);

    /// Selection (single row). Rows are not selectable unless enabled.
    void setSelectable(bool selectable) { m_selectable = selectable; }
    std::optional<std::size_t> selectedRow() const noexcept { return m_selected; }
    void setSelectedRow(std::optional<std::size_t> row);
    void setOnSelect(std::function<void(std::optional<std::size_t>)> onSelect) { m_onSelect = std::move(onSelect); }
    /// Double-click or Enter.
    void setOnActivate(std::function<void(std::size_t)> onActivate) { m_onActivate = std::move(onActivate); }

    /// Keep showing the newest rows while scrolled to the end (log views).
    void setStickToEnd(bool stick) { m_stickToEnd = stick; }
    ScrollState& scroll() noexcept { return m_scroll; }

    /// Text shown when there are no rows.
    void setEmptyText(std::string text) { m_emptyText = std::move(text); }

    Size measure(Context& ctx, Size available) override;
    void arrange(Context& ctx, Rect rect) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onScroll(Context& ctx, const ScrollEvent& event) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    void onPointerMove(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;
    bool onKeyDown(Context& ctx, const KeyEvent& event) override;

    /// Row under a window-coordinate point, if any.
    std::optional<std::size_t> rowAt(Vec2 point) const;

private:
    Rect rowsArea() const;
    void select(Context& ctx, std::optional<std::size_t> row);

    std::size_t m_rowCount = 0;
    float m_rowHeight = 24.0f;
    RowPainter m_painter;
    float m_headerHeight = 0.0f;
    std::function<void(Context&, DrawList&, Rect)> m_headerPainter;
    bool m_selectable = false;
    std::optional<std::size_t> m_selected;
    std::function<void(std::optional<std::size_t>)> m_onSelect;
    std::function<void(std::size_t)> m_onActivate;
    bool m_stickToEnd = false;
    bool m_wasAtEnd = true;
    std::string m_emptyText;
    ScrollState m_scroll;
};

} // namespace rolo::ui
