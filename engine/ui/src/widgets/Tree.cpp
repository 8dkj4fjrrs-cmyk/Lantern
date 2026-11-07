#include "rolo/ui/widgets/Tree.hpp"

#include "rolo/ui/Context.hpp"
#include "rolo/ui/Icons.hpp"
#include "rolo/ui/widgets/Basic.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

namespace rolo::ui {

// --- TreeView ------------------------------------------------------------------------------

TreeView::TreeView()
{
    setFocusable(true);
}

void TreeView::setItems(std::vector<TreeItem> items)
{
    m_items = std::move(items);
    rebuildVisible();
    // Drop a selection whose item has gone.
    if (m_selected != 0
        && std::none_of(m_items.begin(), m_items.end(), [this](const TreeItem& i) { return i.id == m_selected; })) {
        m_selected = 0;
    }
}

bool TreeView::isExpanded(const TreeItem& item) const
{
    if (m_expanded.count(item.id) > 0) {
        return true;
    }
    if (m_collapsed.count(item.id) > 0) {
        return false;
    }
    return item.depth < m_autoExpandDepth;
}

void TreeView::setExpanded(std::uint64_t id, bool expanded)
{
    if (expanded) {
        m_expanded.insert(id);
        m_collapsed.erase(id);
    } else {
        m_collapsed.insert(id);
        m_expanded.erase(id);
    }
    rebuildVisible();
}

void TreeView::rebuildVisible()
{
    m_visible.clear();
    int hideDeeperThan = INT_MAX; // inside a collapsed branch
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        const TreeItem& item = m_items[i];
        if (item.depth > hideDeeperThan) {
            continue;
        }
        hideDeeperThan = INT_MAX;
        m_visible.push_back(i);
        if (item.hasChildren && !isExpanded(item)) {
            hideDeeperThan = item.depth;
        }
    }
}

Size TreeView::measure(Context& ctx, Size available)
{
    m_rowHeight = ctx.theme().rowHeight;
    return applyFixedSize({available.width, std::min(available.height, m_rowHeight * static_cast<float>(m_visible.size()))});
}

void TreeView::arrange(Context& ctx, Rect rect)
{
    Widget::arrange(ctx, rect);
    m_rowHeight = ctx.theme().rowHeight;
    m_scroll.setExtent(m_rowHeight * static_cast<float>(m_visible.size()) + 4.0f, rect.height);
}

std::optional<std::size_t> TreeView::visibleRowAt(Vec2 point) const
{
    const Rect b = bounds();
    if (!b.contains(point)) {
        return std::nullopt;
    }
    const auto row = static_cast<std::size_t>((point.y - b.y - 2.0f + m_scroll.offset()) / m_rowHeight);
    if (point.y - b.y - 2.0f + m_scroll.offset() < 0.0f || row >= m_visible.size()) {
        return std::nullopt;
    }
    return row;
}

Rect TreeView::chevronRect(const Rect& row, int depth, const Theme& theme) const
{
    return Rect{row.x + 4.0f + static_cast<float>(depth) * theme.indent, row.y, 16.0f, row.height};
}

void TreeView::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect b = bounds();
    draw.pushClip(b);
    if (m_items.empty() && !m_emptyText.empty()) {
        draw.text(t.font, t.fontSize, {b.x + t.padding, b.y + t.padding}, t.textFaint, m_emptyText);
    }

    const FontMetrics fm = ctx.atlas().metrics(t.font, t.fontSize);
    const auto hovered = (isHovered() && !m_scroll.isDragging()) ? visibleRowAt(ctx.pointerPosition()) : std::nullopt;
    const auto first = static_cast<std::size_t>(std::max(0.0f, std::floor((m_scroll.offset() - 2.0f) / m_rowHeight)));
    const std::size_t last =
        std::min(m_visible.size(), first + static_cast<std::size_t>(std::ceil(b.height / m_rowHeight)) + 2);

    for (std::size_t v = first; v < last; ++v) {
        const TreeItem& item = m_items[m_visible[v]];
        const Rect row{b.x, b.y + 2.0f + static_cast<float>(v) * m_rowHeight - m_scroll.offset(), b.width, m_rowHeight};
        const bool selected = item.id == m_selected;
        if (selected) {
            draw.fillRect(row, isFocused() ? t.selectionFocused : t.selection);
        } else if (hovered && *hovered == v) {
            draw.fillRect(row, t.hover);
        }
        const Rect chevron = chevronRect(row, item.depth, t);
        if (item.hasChildren) {
            drawIcon(draw, isExpanded(item) ? Icon::ChevronDown : Icon::ChevronRight, chevron, t.textDim);
        }
        const float textY = row.y + std::round((row.height - fm.lineHeight) * 0.5f);
        float x = chevron.right() + 4.0f;
        if (item.icon) {
            drawIcon(draw, *item.icon, Rect{x, row.center().y - 7.0f, 14.0f, 14.0f}, t.accent);
            x += 19.0f;
        }
        const float labelWidth = draw.textEllipsized(t.font, t.fontSize, {x, textY}, item.dimmed ? t.textFaint : t.text, item.label,
                                                     std::max(0.0f, row.right() - x - 8.0f));
        if (!item.detail.empty()) {
            const float dx = x + labelWidth + 8.0f;
            draw.textEllipsized(t.font, t.smallFontSize, {dx, textY + 1.0f}, t.textFaint, item.detail,
                                std::max(0.0f, row.right() - dx - 8.0f));
        }
    }
    draw.popClip();
    m_scroll.paint(draw, b, t, isHovered() || m_scroll.isDragging());
}

bool TreeView::onScroll(Context&, const ScrollEvent& event)
{
    m_scroll.scrollBy(-event.delta.y);
    return true;
}

void TreeView::select(Context& ctx, std::uint64_t id)
{
    if (id == m_selected) {
        return;
    }
    m_selected = id;
    if (m_onSelect) {
        ctx.post([this, id] {
            if (m_onSelect) {
                m_onSelect(id);
            }
        });
    }
}

bool TreeView::onPointerDown(Context& ctx, const PointerEvent& event)
{
    if (event.button != MouseButton::Left) {
        return false;
    }
    if (m_scroll.pointerDown(bounds(), ctx.theme(), event.position)) {
        return true;
    }
    const auto v = visibleRowAt(event.position);
    if (!v) {
        select(ctx, 0); // clicking empty space clears the selection
        return true;
    }
    const TreeItem& item = m_items[m_visible[*v]];
    const Rect row{bounds().x, 0.0f, bounds().width, m_rowHeight};
    const Rect chevron = chevronRect(row, item.depth, ctx.theme());
    const bool onChevron = event.position.x >= chevron.x - 2.0f && event.position.x <= chevron.right() + 2.0f;
    if (item.hasChildren && (onChevron || (event.clickCount == 2 && !m_onActivate))) {
        setExpanded(item.id, !isExpanded(item));
        if (onChevron) {
            return true;
        }
    }
    select(ctx, item.id);
    if (event.clickCount == 2 && m_onActivate) {
        ctx.post([onActivate = m_onActivate, id = item.id] { onActivate(id); });
    }
    return true;
}

void TreeView::onPointerMove(Context& ctx, const PointerEvent& event)
{
    if (m_scroll.isDragging()) {
        m_scroll.pointerMove(bounds(), ctx.theme(), event.position);
    }
}

bool TreeView::onPointerUp(Context&, const PointerEvent&)
{
    m_scroll.pointerUp();
    return true;
}

void TreeView::reveal(std::uint64_t id)
{
    // Walk up the parents, unfolding each.
    for (std::size_t guard = 0; guard < m_items.size(); ++guard) {
        const auto it = std::find_if(m_items.begin(), m_items.end(), [id](const TreeItem& i) { return i.id == id; });
        if (it == m_items.end() || it->parentId == 0) {
            break;
        }
        id = it->parentId;
        m_expanded.insert(id);
        m_collapsed.erase(id);
    }
    rebuildVisible();
    for (std::size_t v = 0; v < m_visible.size(); ++v) {
        if (m_items[m_visible[v]].id == m_selected) {
            const float top = 2.0f + static_cast<float>(v) * m_rowHeight;
            m_scroll.ensureVisible(top, top + m_rowHeight);
            return;
        }
    }
}

void TreeView::revealSelection(Context&)
{
    for (std::size_t v = 0; v < m_visible.size(); ++v) {
        if (m_items[m_visible[v]].id == m_selected) {
            const float top = 2.0f + static_cast<float>(v) * m_rowHeight;
            m_scroll.ensureVisible(top, top + m_rowHeight);
            return;
        }
    }
}

bool TreeView::onKeyDown(Context& ctx, const KeyEvent& event)
{
    if (m_visible.empty()) {
        return false;
    }
    auto it = std::find_if(m_visible.begin(), m_visible.end(),
                           [this](std::size_t i) { return m_items[i].id == m_selected; });
    const bool hasSelection = it != m_visible.end();
    const std::size_t v = hasSelection ? static_cast<std::size_t>(it - m_visible.begin()) : 0;

    switch (event.key) {
    case Key::Up:
        select(ctx, m_items[m_visible[hasSelection && v > 0 ? v - 1 : 0]].id);
        break;
    case Key::Down:
        select(ctx, m_items[m_visible[hasSelection ? std::min(v + 1, m_visible.size() - 1) : 0]].id);
        break;
    case Key::Home: select(ctx, m_items[m_visible.front()].id); break;
    case Key::Enter:
        if (!hasSelection || !m_onActivate) {
            return false;
        }
        ctx.post([onActivate = m_onActivate, id = m_selected] { onActivate(id); });
        return true;
    case Key::End: select(ctx, m_items[m_visible.back()].id); break;
    case Key::Left: {
        if (!hasSelection) {
            return false;
        }
        const TreeItem& item = m_items[m_visible[v]];
        if (item.hasChildren && isExpanded(item)) {
            setExpanded(item.id, false);
        } else if (item.parentId != 0) {
            select(ctx, item.parentId);
        }
        break;
    }
    case Key::Right: {
        if (!hasSelection) {
            return false;
        }
        const TreeItem& item = m_items[m_visible[v]];
        if (item.hasChildren && !isExpanded(item)) {
            setExpanded(item.id, true);
        } else if (item.hasChildren && v + 1 < m_visible.size()) {
            select(ctx, m_items[m_visible[v + 1]].id);
        }
        break;
    }
    default: return false;
    }
    revealSelection(ctx);
    return true;
}

// --- PropertyList ----------------------------------------------------------------------------

void PropertyList::setTitle(std::string title, std::string subtitle)
{
    m_title = std::move(title);
    m_subtitle = std::move(subtitle);
}

float PropertyList::contentHeight(Context& ctx, float width) const
{
    const Theme& t = ctx.theme();
    if (m_empty) {
        return 0.0f;
    }
    float h = t.padding + 22.0f + 8.0f; // title
    h += t.rowHeight * static_cast<float>(m_rows.size());
    if (!m_footnote.empty()) {
        Label note(m_footnote, Label::Style::Small);
        note.setWrap(true);
        h += 12.0f + note.measure(ctx, {width - 2.0f * t.padding, kUnbounded}).height;
    }
    return h + t.padding;
}

Size PropertyList::measure(Context&, Size available)
{
    return applyFixedSize(available);
}

void PropertyList::arrange(Context& ctx, Rect rect)
{
    Widget::arrange(ctx, rect);
    m_scroll.setExtent(contentHeight(ctx, rect.width), rect.height);
}

void PropertyList::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect b = bounds();
    draw.pushClip(b);

    if (m_empty) {
        Label empty(m_emptyText, Label::Style::Faint);
        empty.setWrap(true);
        const Rect area = b.inset(t.padding);
        const Size s = empty.measure(ctx, area.size());
        empty.arrange(ctx, Rect{area.x, area.y, area.width, s.height});
        empty.paint(ctx, draw);
        draw.popClip();
        return;
    }

    float y = b.y + t.padding - m_scroll.offset();
    const float x = b.x + t.padding;
    const float width = b.width - 2.0f * t.padding;
    const float titleWidth = draw.textEllipsized(t.fontStrong, t.fontSize + 1.0f, {x, y + 2.0f}, t.text, m_title, width);
    if (!m_subtitle.empty()) {
        draw.text(t.font, t.smallFontSize, {x + titleWidth + 8.0f, y + 4.0f}, t.textFaint, m_subtitle);
    }
    y += 22.0f;
    draw.fillRect(Rect{x, y + 3.0f, width, 1.0f}, t.border);
    y += 8.0f;

    const float labelWidth = std::clamp(width * 0.38f, 80.0f, 140.0f);
    const FontMetrics fm = ctx.atlas().metrics(t.font, t.fontSize);
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        const PropertyRow& row = m_rows[i];
        const Rect r{x - 4.0f, y, width + 8.0f, t.rowHeight};
        const float textY = y + std::round((t.rowHeight - fm.lineHeight) * 0.5f);
        if (row.isHeading) {
            draw.text(t.fontStrong, t.smallFontSize, {x, textY + 2.0f}, t.textDim, row.label);
        } else {
            if (i % 2 == 1) {
                draw.fillRoundedRect(r, 3.0f, Color{255, 255, 255, 6});
            }
            draw.textEllipsized(t.font, t.fontSize, {x, textY}, t.textDim, row.label, labelWidth - 8.0f);
            draw.textEllipsized(t.font, t.fontSize, {x + labelWidth, textY}, t.text, row.value, width - labelWidth);
        }
        y += t.rowHeight;
    }

    if (!m_footnote.empty()) {
        Label note(m_footnote, Label::Style::Small);
        note.setWrap(true);
        const Size s = note.measure(ctx, {width, kUnbounded});
        note.arrange(ctx, Rect{x, y + 12.0f, width, s.height});
        note.paint(ctx, draw);
    }
    draw.popClip();
    m_scroll.paint(draw, b, t, isHovered());
}

bool PropertyList::onScroll(Context&, const ScrollEvent& event)
{
    m_scroll.scrollBy(-event.delta.y);
    return true;
}

} // namespace rolo::ui
