#include "lantern/ui/widgets/DockSpace.hpp"

#include "lantern/ui/Context.hpp"
#include "lantern/ui/Icons.hpp"

#include <algorithm>
#include <cmath>

namespace lantern::ui {

namespace {
constexpr float kGap = 4.0f;            // space between groups (also the splitter)
constexpr float kGroupRadius = 6.0f;
constexpr float kTabPadding = 12.0f;
constexpr float kCloseSize = 16.0f;
constexpr float kDragThreshold = 6.0f;
constexpr float kEdgeZone = 0.25f;      // outer quarter of a group = dock to that side
} // namespace

void DockSpace::setDefaultLayout(DockLayout layout)
{
    m_default = std::move(layout);
    if (m_layout.isEmpty()) {
        m_layout = m_default;
    }
}

void DockSpace::setLayout(DockLayout layout)
{
    m_layout = std::move(layout);
    m_groups.clear();
    m_splitters.clear();
    m_tabs.clear();
    m_contentRects.clear();
}

void DockSpace::resetLayout()
{
    m_layout = m_default;
    changed();
}

void DockSpace::setOpen(const std::string& id, bool open)
{
    if (findPanel(id) == nullptr || open == isOpen(id)) {
        if (open) {
            m_layout.activate(id);
        }
        return;
    }
    if (open) {
        m_layout.reopen(id, m_default);
    } else {
        m_layout.remove(id);
    }
    changed();
}

std::vector<std::string> DockSpace::panelIds() const
{
    std::vector<std::string> ids;
    for (const Panel& p : m_panels) {
        ids.push_back(p.id);
    }
    return ids;
}

std::string DockSpace::titleOf(std::string_view id) const
{
    const Panel* p = findPanel(id);
    return p != nullptr ? p->title : std::string(id);
}

const DockSpace::Panel* DockSpace::findPanel(std::string_view id) const
{
    for (const Panel& p : m_panels) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

void DockSpace::changed()
{
    // The layout tree may have been reshaped: geometry from the last arrange()
    // points at old nodes, so drop it until the next frame recomputes it.
    m_groups.clear();
    m_splitters.clear();
    m_tabs.clear();
    m_contentRects.clear();
    if (m_onChanged) {
        if (Context* ctx = context()) {
            ctx->post(m_onChanged);
        } else {
            m_onChanged();
        }
    }
}

// --- Layout --------------------------------------------------------------------------------

void DockSpace::arrange(Context& ctx, Rect rect)
{
    setBounds(rect);
    const Theme& t = ctx.theme();
    m_layout.computeRects(rect.inset(kGap), kGap, m_groups, m_splitters);
    m_tabs.clear();
    m_contentRects.clear();

    for (const Panel& p : m_panels) {
        p.content->setVisible(false);
    }

    for (const auto& group : m_groups) {
        const Rect g = group.rect;
        const Rect bar{g.x, g.y, g.width, t.tabBarHeight};
        const Rect content{g.x, g.y + t.tabBarHeight, g.width, std::max(0.0f, g.height - t.tabBarHeight)};
        m_contentRects.push_back(content);

        float x = bar.x + 4.0f;
        for (std::size_t i = 0; i < group.node->tabs.size(); ++i) {
            const std::string& id = group.node->tabs[i];
            const float textWidth = std::ceil(ctx.atlas().measure(t.font, t.fontSize, titleOf(id)));
            const float w = kTabPadding + textWidth + 6.0f + kCloseSize + 6.0f;
            const Rect tab{x, bar.y + 4.0f, w, bar.height - 4.0f};
            const Rect close{tab.right() - kCloseSize - 6.0f, tab.center().y - kCloseSize * 0.5f, kCloseSize, kCloseSize};
            const bool active = i == group.node->active;
            m_tabs.push_back(Tab{id, tab, close, active});
            x += w + 2.0f;

            if (active) {
                if (const Panel* p = findPanel(id)) {
                    p->content->setVisible(true);
                    p->content->arrange(ctx, content);
                }
            }
        }
    }
}

// --- Painting -------------------------------------------------------------------------------

void DockSpace::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Vec2 pointer = ctx.pointerPosition();
    const bool interactive = isHovered() || hasPointerCapture();

    for (std::size_t gi = 0; gi < m_groups.size(); ++gi) {
        const Rect g = m_groups[gi].rect;
        draw.fillRoundedRect(g, kGroupRadius, t.panel);
        // Tab strip: darker band across the top (square bottom edge).
        draw.pushClip(Rect{g.x, g.y, g.width, t.tabBarHeight});
        draw.fillRoundedRect(Rect{g.x, g.y, g.width, t.tabBarHeight + kGroupRadius}, kGroupRadius, t.panelHeader);
        draw.popClip();
        draw.fillRect(Rect{g.x, g.y + t.tabBarHeight - 1.0f, g.width, 1.0f}, t.divider);

        // Panel content, clipped to its area.
        const Rect content = m_contentRects[gi];
        const auto& tabs = m_groups[gi].node->tabs;
        const std::size_t active = m_groups[gi].node->active;
        if (active < tabs.size()) {
            if (const Panel* p = findPanel(tabs[active]); p != nullptr && p->content->isVisible()) {
                draw.pushClip(content);
                p->content->paint(ctx, draw);
                draw.popClip();
            }
        }
    }

    // Tabs.
    const FontMetrics fm = ctx.atlas().metrics(t.font, t.fontSize);
    for (const Tab& tab : m_tabs) {
        const bool hovered = interactive && m_drag == Drag::None && tab.rect.contains(pointer);
        draw.pushClip(bounds());
        if (tab.active) {
            draw.pushClip(Rect{tab.rect.x, tab.rect.y, tab.rect.width, tab.rect.height - 1.0f});
            draw.fillRoundedRect(Rect{tab.rect.x, tab.rect.y, tab.rect.width, tab.rect.height + 6.0f}, 5.0f, t.panel);
            draw.popClip();
            draw.fillRoundedRect(Rect{tab.rect.x + 6.0f, tab.rect.y, tab.rect.width - 12.0f, 2.0f}, 1.0f, t.accent);
        } else if (hovered) {
            draw.fillRoundedRect(tab.rect.inset(Insets{0.0f, 2.0f, 0.0f, 3.0f}), 4.0f, t.hover);
        }
        const float textY = tab.rect.y + std::round((tab.rect.height - fm.lineHeight) * 0.5f);
        draw.text(tab.active ? t.fontStrong : t.font, t.fontSize, {tab.rect.x + kTabPadding, textY},
                  tab.active ? t.text : t.textDim, titleOf(tab.id));
        if (tab.active || hovered) {
            const bool closeHot = interactive && tab.close.contains(pointer);
            if (closeHot) {
                draw.fillRoundedRect(tab.close, 4.0f, t.controlHover);
            }
            drawIcon(draw, Icon::Close, tab.close, closeHot ? t.text : t.textFaint);
        }
        draw.popClip();
    }

    // Splitter highlight while hovered or dragged.
    if (m_drag == Drag::Splitter) {
        for (const auto& s : m_splitters) {
            if (s.node == m_dragSplit) {
                draw.fillRect(s.rect, t.accent.fade(0.6f));
            }
        }
    } else if (interactive && m_drag == Drag::None) {
        if (const auto* s = splitterAt(pointer)) {
            draw.fillRect(s->rect, t.accent.fade(0.35f));
        }
    }

    // Drag-to-dock preview and a ghost of the dragged tab.
    if (m_drag == Drag::Tab && m_tabDragging) {
        if (m_drop) {
            draw.fillRoundedRect(m_drop->preview, kGroupRadius, t.accent.fade(0.18f));
            draw.strokeRoundedRect(m_drop->preview, kGroupRadius, t.accent.fade(0.8f), 2.0f);
        }
        const std::string title = titleOf(m_dragPanel);
        const float w = ctx.atlas().measure(t.fontStrong, t.fontSize, title) + 2.0f * kTabPadding;
        const Rect ghost{pointer.x + 10.0f, pointer.y + 8.0f, w, t.tabBarHeight - 4.0f};
        draw.fillRoundedRect(ghost, 5.0f, t.popup.fade(0.95f));
        draw.strokeRoundedRect(ghost, 5.0f, t.accent.fade(0.7f));
        draw.text(t.fontStrong, t.fontSize, {ghost.x + kTabPadding, ghost.y + std::round((ghost.height - fm.lineHeight) * 0.5f)},
                  t.text, title);
    }
}

// --- Hit testing ------------------------------------------------------------------------------

const DockLayout::SplitterRect* DockSpace::splitterAt(Vec2 point) const
{
    for (const auto& s : m_splitters) {
        if (s.rect.inset(-3.0f).contains(point)) {
            return &s;
        }
    }
    return nullptr;
}

const DockSpace::Tab* DockSpace::tabAt(Vec2 point) const
{
    for (const Tab& tab : m_tabs) {
        if (tab.rect.contains(point)) {
            return &tab;
        }
    }
    return nullptr;
}

Rect DockSpace::tabRect(std::string_view id) const
{
    for (const Tab& tab : m_tabs) {
        if (tab.id == id) {
            return tab.rect;
        }
    }
    return {};
}

Widget* DockSpace::hitTest(Vec2 point)
{
    if (!isVisible() || !bounds().contains(point)) {
        return nullptr;
    }
    if (splitterAt(point) != nullptr || tabAt(point) != nullptr) {
        return this;
    }
    for (std::size_t gi = 0; gi < m_groups.size(); ++gi) {
        if (m_contentRects[gi].contains(point)) {
            return Widget::hitTest(point); // the visible panel content
        }
    }
    return this; // tab strips and gaps
}

CursorShape DockSpace::cursor(Vec2 point) const
{
    const DockLayout::Node* split = m_drag == Drag::Splitter ? m_dragSplit : nullptr;
    if (split == nullptr) {
        if (const auto* s = splitterAt(point)) {
            split = s->node;
        }
    }
    if (split != nullptr) {
        return split->axis == Axis::Horizontal ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical;
    }
    return CursorShape::Arrow;
}

std::optional<DockSpace::DropTarget> DockSpace::dropTargetAt(Vec2 point) const
{
    for (std::size_t gi = 0; gi < m_groups.size(); ++gi) {
        const Rect g = m_groups[gi].rect;
        if (!g.contains(point)) {
            continue;
        }
        const DockLayout::Node& node = *m_groups[gi].node;
        // A lone tab cannot be docked onto its own group.
        const bool own = std::find(node.tabs.begin(), node.tabs.end(), m_dragPanel) != node.tabs.end();
        if (own && node.tabs.size() == 1) {
            return std::nullopt;
        }
        std::string anchor;
        for (const std::string& id : node.tabs) {
            if (id != m_dragPanel) {
                anchor = id;
                break;
            }
        }
        const Rect content = m_contentRects[gi];
        // Over the tab strip, or in the middle: join as a tab.
        DockSide side = DockSide::Center;
        if (content.contains(point)) {
            const float fx = (point.x - content.x) / std::max(1.0f, content.width);
            const float fy = (point.y - content.y) / std::max(1.0f, content.height);
            const float distances[4] = {fx, 1.0f - fx, fy, 1.0f - fy}; // left, right, top, bottom
            const auto nearest = static_cast<std::size_t>(std::min_element(distances, distances + 4) - distances);
            if (distances[nearest] < kEdgeZone) {
                side = static_cast<DockSide>(nearest); // DockSide order matches: Left, Right, Top, Bottom
            }
        }
        if (own && side == DockSide::Center) {
            return std::nullopt; // already a tab here
        }
        Rect preview = g;
        constexpr float kShare = 0.35f;
        switch (side) {
        case DockSide::Left: preview.width *= kShare; break;
        case DockSide::Right: preview.x += preview.width * (1.0f - kShare); preview.width *= kShare; break;
        case DockSide::Top: preview.height *= kShare; break;
        case DockSide::Bottom: preview.y += preview.height * (1.0f - kShare); preview.height *= kShare; break;
        case DockSide::Center: break;
        }
        return DropTarget{anchor, side, preview.inset(2.0f)};
    }
    return std::nullopt;
}

// --- Input -------------------------------------------------------------------------------------

bool DockSpace::onPointerDown(Context& ctx, const PointerEvent& event)
{
    if (event.button != MouseButton::Left) {
        return false;
    }
    m_pressPosition = event.position;
    if (const auto* s = splitterAt(event.position)) {
        m_drag = Drag::Splitter;
        m_dragSplit = s->node;
        m_dragArea = s->area;
        return true;
    }
    if (const Tab* tab = tabAt(event.position)) {
        m_drag = Drag::Tab;
        m_dragPanel = tab->id;
        m_tabDragging = false;
        m_pressedClose = tab->close.contains(event.position);
        if (!m_pressedClose && !tab->active) {
            m_layout.activate(tab->id);
            if (m_onChanged) {
                ctx.post(m_onChanged); // same tree shape: geometry stays valid
            }
        }
        return true;
    }
    return false;
}

void DockSpace::onPointerMove(Context&, const PointerEvent& event)
{
    if (m_drag == Drag::Splitter && m_dragSplit != nullptr) {
        const bool horizontal = m_dragSplit->axis == Axis::Horizontal;
        const float total = (horizontal ? m_dragArea.width : m_dragArea.height) - kGap;
        if (total > 0.0f) {
            const float pos = horizontal ? event.position.x - m_dragArea.x : event.position.y - m_dragArea.y;
            DockLayout::setRatio(*m_dragSplit, (pos - kGap * 0.5f) / total);
        }
    } else if (m_drag == Drag::Tab && !m_pressedClose) {
        const Vec2 d = event.position - m_pressPosition;
        if (!m_tabDragging && std::abs(d.x) + std::abs(d.y) > kDragThreshold) {
            m_tabDragging = true;
        }
        if (m_tabDragging) {
            m_drop = dropTargetAt(event.position);
        }
    }
}

bool DockSpace::onPointerUp(Context&, const PointerEvent& event)
{
    if (m_drag == Drag::Splitter) {
        changed();
    } else if (m_drag == Drag::Tab) {
        if (m_pressedClose) {
            const Tab* tab = tabAt(event.position);
            if (tab != nullptr && tab->id == m_dragPanel && tab->close.contains(event.position)) {
                m_layout.remove(m_dragPanel);
                changed();
            }
        } else if (m_tabDragging && m_drop) {
            if (m_layout.dock(m_dragPanel, m_drop->anchor, m_drop->side, 0.35f)) {
                m_layout.activate(m_dragPanel);
                changed();
            }
        }
    }
    m_drag = Drag::None;
    m_dragSplit = nullptr;
    m_tabDragging = false;
    m_pressedClose = false;
    m_drop.reset();
    return true;
}

} // namespace lantern::ui
