#include "rolo/ui/widgets/Scroll.hpp"

#include "rolo/ui/Context.hpp"

#include <algorithm>
#include <cmath>

namespace rolo::ui {

// --- ScrollState ---------------------------------------------------------------------

float ScrollState::maxOffset() const noexcept
{
    return std::max(0.0f, m_content - m_view);
}

void ScrollState::setExtent(float contentHeight, float viewHeight)
{
    m_content = std::max(0.0f, contentHeight);
    m_view = std::max(0.0f, viewHeight);
    setOffset(m_offset);
}

void ScrollState::setOffset(float offset)
{
    m_offset = std::clamp(offset, 0.0f, maxOffset());
}

void ScrollState::ensureVisible(float top, float bottom)
{
    if (top < m_offset) {
        setOffset(top);
    } else if (bottom > m_offset + m_view) {
        setOffset(bottom - m_view);
    }
}

Rect ScrollState::trackRect(Rect view, const Theme& theme) const
{
    const float w = theme.scrollbarWidth;
    return Rect{view.right() - w - 2.0f, view.y + 2.0f, w, std::max(0.0f, view.height - 4.0f)};
}

std::optional<Rect> ScrollState::thumbRect(Rect view, const Theme& theme) const
{
    if (m_content <= m_view + 0.5f || m_content <= 0.0f) {
        return std::nullopt; // everything fits: no scrollbar
    }
    const Rect track = trackRect(view, theme);
    const float length = std::max(24.0f, track.height * (m_view / m_content));
    const float travel = track.height - length;
    const float y = track.y + (maxOffset() > 0.0f ? travel * (m_offset / maxOffset()) : 0.0f);
    return Rect{track.x, std::round(y), track.width, std::round(length)};
}

bool ScrollState::pointerDown(Rect view, const Theme& theme, Vec2 point)
{
    const auto thumb = thumbRect(view, theme);
    if (!thumb) {
        return false;
    }
    const Rect track = trackRect(view, theme).inset(Insets{-3.0f, 0.0f, -3.0f, 0.0f}); // easier to grab
    if (!track.contains(point)) {
        return false;
    }
    if (thumb->contains(point)) {
        m_grab = point.y - thumb->y;
    } else {
        // Clicked the track: jump so the thumb centres on the click.
        m_grab = thumb->height * 0.5f;
        pointerMove(view, theme, point);
    }
    m_dragging = true;
    return true;
}

void ScrollState::pointerMove(Rect view, const Theme& theme, Vec2 point)
{
    const auto thumb = thumbRect(view, theme);
    if (!thumb) {
        return;
    }
    const Rect track = trackRect(view, theme);
    const float travel = track.height - thumb->height;
    if (travel <= 0.0f) {
        return;
    }
    const float t = (point.y - m_grab - track.y) / travel;
    setOffset(t * maxOffset());
}

void ScrollState::paint(DrawList& draw, Rect view, const Theme& theme, bool hovered) const
{
    const auto thumb = thumbRect(view, theme);
    if (!thumb) {
        return;
    }
    const Color colour = m_dragging ? theme.textDim : (hovered ? theme.textFaint : theme.controlPressed);
    draw.fillRoundedRect(*thumb, theme.scrollbarWidth * 0.5f, colour);
}

// --- ScrollView -----------------------------------------------------------------------------

Size ScrollView::measure(Context& ctx, Size available)
{
    // A scroll view takes what it is given; its content may be taller.
    Size content;
    if (!children().empty()) {
        content = children().front()->measure(ctx, {available.width, kUnbounded});
    }
    return applyFixedSize({std::min(content.width, available.width), std::min(content.height, available.height)});
}

void ScrollView::arrange(Context& ctx, Rect rect)
{
    setBounds(rect);
    if (children().empty()) {
        return;
    }
    Widget& content = *children().front();
    const Size size = content.measure(ctx, {rect.width, kUnbounded});
    m_scroll.setExtent(size.height, rect.height);
    content.arrange(ctx, Rect{rect.x, rect.y - m_scroll.offset(), rect.width, std::max(size.height, rect.height)});
}

void ScrollView::paint(Context& ctx, DrawList& draw)
{
    draw.pushClip(bounds());
    paintChildren(ctx, draw);
    draw.popClip();
    m_scroll.paint(draw, bounds(), ctx.theme(), bounds().contains(ctx.pointerPosition()));
}

Widget* ScrollView::hitTest(Vec2 point)
{
    if (!isVisible() || !bounds().contains(point)) {
        return nullptr;
    }
    const Theme fallback;
    const Theme& theme = context() != nullptr ? context()->theme() : fallback;
    if (m_scroll.thumbRect(bounds(), theme) && m_scroll.trackRect(bounds(), theme).inset(-3.0f).contains(point)) {
        return this; // the scrollbar belongs to the view, not the content under it
    }
    return Widget::hitTest(point);
}

bool ScrollView::onScroll(Context&, const ScrollEvent& event)
{
    const float before = m_scroll.offset();
    m_scroll.scrollBy(-event.delta.y);
    return m_scroll.offset() != before || m_scroll.maxOffset() > 0.0f;
}

bool ScrollView::onPointerDown(Context& ctx, const PointerEvent& event)
{
    return event.button == MouseButton::Left && m_scroll.pointerDown(bounds(), ctx.theme(), event.position);
}

void ScrollView::onPointerMove(Context& ctx, const PointerEvent& event)
{
    if (m_scroll.isDragging()) {
        m_scroll.pointerMove(bounds(), ctx.theme(), event.position);
    }
}

bool ScrollView::onPointerUp(Context&, const PointerEvent&)
{
    m_scroll.pointerUp();
    return true;
}

// --- ListView -------------------------------------------------------------------------------

ListView::ListView()
{
    setFocusable(true);
}

void ListView::setRowCount(std::size_t count)
{
    m_rowCount = count;
    if (m_selected && *m_selected >= count) {
        m_selected.reset();
    }
}

void ListView::setHeader(float height, std::function<void(Context&, DrawList&, Rect)> painter)
{
    m_headerHeight = height;
    m_headerPainter = std::move(painter);
}

void ListView::setSelectedRow(std::optional<std::size_t> row)
{
    m_selected = (row && *row < m_rowCount) ? row : std::nullopt;
}

Rect ListView::rowsArea() const
{
    const Rect b = bounds();
    return Rect{b.x, b.y + m_headerHeight, b.width, std::max(0.0f, b.height - m_headerHeight)};
}

Size ListView::measure(Context&, Size available)
{
    return applyFixedSize({available.width, std::min(available.height,
                                                     m_headerHeight + m_rowHeight * static_cast<float>(m_rowCount))});
}

void ListView::arrange(Context& ctx, Rect rect)
{
    Widget::arrange(ctx, rect);
    const Rect area = rowsArea();
    m_scroll.setExtent(m_rowHeight * static_cast<float>(m_rowCount), area.height);
    if (m_stickToEnd && m_wasAtEnd) {
        m_scroll.scrollToEnd();
    }
    m_wasAtEnd = m_scroll.atEnd();
}

std::optional<std::size_t> ListView::rowAt(Vec2 point) const
{
    const Rect area = rowsArea();
    if (!area.contains(point) || m_rowHeight <= 0.0f) {
        return std::nullopt;
    }
    const auto row = static_cast<std::size_t>((point.y - area.y + m_scroll.offset()) / m_rowHeight);
    return row < m_rowCount ? std::optional<std::size_t>(row) : std::nullopt;
}

void ListView::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect b = bounds();
    if (m_headerPainter && m_headerHeight > 0.0f) {
        const Rect header{b.x, b.y, b.width, m_headerHeight};
        draw.pushClip(header);
        m_headerPainter(ctx, draw, header);
        draw.popClip();
    }

    const Rect area = rowsArea();
    draw.pushClip(area);
    if (m_rowCount == 0 && !m_emptyText.empty()) {
        draw.text(t.font, t.fontSize, {area.x + t.padding, area.y + t.padding}, t.textFaint, m_emptyText);
    }

    const std::optional<std::size_t> hoveredRow =
        (isHovered() && !m_scroll.isDragging()) ? rowAt(ctx.pointerPosition()) : std::nullopt;
    if (m_rowCount > 0 && m_painter) {
        const auto first = static_cast<std::size_t>(std::floor(m_scroll.offset() / m_rowHeight));
        const auto visibleRows = static_cast<std::size_t>(std::ceil(area.height / m_rowHeight)) + 1;
        const std::size_t last = std::min(m_rowCount, first + visibleRows);
        for (std::size_t i = first; i < last; ++i) {
            RowInfo info;
            info.index = i;
            info.rect = Rect{area.x, area.y + static_cast<float>(i) * m_rowHeight - m_scroll.offset(), area.width,
                             m_rowHeight};
            info.selected = m_selected && *m_selected == i;
            info.hovered = hoveredRow && *hoveredRow == i;
            info.focused = isFocused();
            if (info.selected) {
                draw.fillRect(info.rect, info.focused ? t.selectionFocused : t.selection);
            } else if (info.hovered && m_selectable) {
                draw.fillRect(info.rect, t.hover);
            }
            m_painter(ctx, draw, info);
        }
    }
    draw.popClip();
    m_scroll.paint(draw, area, t, isHovered() || m_scroll.isDragging());
}

bool ListView::onScroll(Context&, const ScrollEvent& event)
{
    m_scroll.scrollBy(-event.delta.y);
    m_wasAtEnd = m_scroll.atEnd();
    return true;
}

void ListView::select(Context& ctx, std::optional<std::size_t> row)
{
    if (!m_selectable || row == m_selected) {
        return;
    }
    m_selected = row;
    if (m_onSelect) {
        ctx.post([this, row] {
            if (m_onSelect) {
                m_onSelect(row);
            }
        });
    }
}

bool ListView::onPointerDown(Context& ctx, const PointerEvent& event)
{
    if (event.button != MouseButton::Left) {
        return false;
    }
    if (m_scroll.pointerDown(rowsArea(), ctx.theme(), event.position)) {
        return true;
    }
    const auto row = rowAt(event.position);
    select(ctx, row);
    if (row && event.clickCount == 2 && m_onActivate) {
        const std::size_t r = *row;
        ctx.post([this, r] {
            if (m_onActivate) {
                m_onActivate(r);
            }
        });
    }
    return true;
}

void ListView::onPointerMove(Context& ctx, const PointerEvent& event)
{
    if (m_scroll.isDragging()) {
        m_scroll.pointerMove(rowsArea(), ctx.theme(), event.position);
        m_wasAtEnd = m_scroll.atEnd();
    }
}

bool ListView::onPointerUp(Context&, const PointerEvent&)
{
    m_scroll.pointerUp();
    return true;
}

bool ListView::onKeyDown(Context& ctx, const KeyEvent& event)
{
    if (m_rowCount == 0 || !m_selectable) {
        return false;
    }
    const std::size_t current = m_selected.value_or(0);
    std::optional<std::size_t> next;
    const auto page = static_cast<std::size_t>(std::max(1.0f, rowsArea().height / m_rowHeight - 1.0f));
    switch (event.key) {
    case Key::Up: next = m_selected ? (current > 0 ? current - 1 : 0) : 0; break;
    case Key::Down: next = m_selected ? std::min(current + 1, m_rowCount - 1) : 0; break;
    case Key::PageUp: next = current > page ? current - page : 0; break;
    case Key::PageDown: next = std::min(current + page, m_rowCount - 1); break;
    case Key::Home: next = 0; break;
    case Key::End: next = m_rowCount - 1; break;
    case Key::Enter:
        if (m_selected && m_onActivate) {
            const std::size_t r = *m_selected;
            ctx.post([this, r] {
                if (m_onActivate) {
                    m_onActivate(r);
                }
            });
        }
        return true;
    default: return false;
    }
    select(ctx, next);
    if (next) {
        const float top = static_cast<float>(*next) * m_rowHeight;
        m_scroll.ensureVisible(top, top + m_rowHeight);
    }
    return true;
}

} // namespace rolo::ui
