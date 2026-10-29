#include "lantern/ui/widgets/Menu.hpp"

#include "lantern/ui/Context.hpp"
#include "lantern/ui/Icons.hpp"

#include <algorithm>
#include <cmath>

namespace lantern::ui {

namespace {
constexpr float kItemHeight = 26.0f;
constexpr float kSeparatorHeight = 9.0f;
constexpr float kPopupPadding = 5.0f;
constexpr float kCheckColumn = 22.0f;
constexpr float kTitlePadding = 10.0f;
} // namespace

// --- MenuPopup --------------------------------------------------------------------------------

MenuPopup::MenuPopup(std::vector<MenuItem> items)
    : m_items(std::move(items))
{
}

float MenuPopup::itemTop(std::size_t index) const
{
    float y = bounds().y + kPopupPadding;
    for (std::size_t i = 0; i < index; ++i) {
        y += m_items[i].separator ? kSeparatorHeight : kItemHeight;
    }
    return y;
}

bool MenuPopup::isEnabled(std::size_t index) const
{
    const MenuItem& item = m_items[index];
    return !item.separator && (!item.isEnabled || item.isEnabled());
}

Size MenuPopup::measure(Context& ctx, Size)
{
    const Theme& t = ctx.theme();
    float labelWidth = 0.0f;
    float shortcutWidth = 0.0f;
    float height = 2.0f * kPopupPadding;
    for (const MenuItem& item : m_items) {
        height += item.separator ? kSeparatorHeight : kItemHeight;
        labelWidth = std::max(labelWidth, ctx.atlas().measure(t.font, t.fontSize, item.label));
        shortcutWidth = std::max(shortcutWidth, ctx.atlas().measure(t.font, t.fontSize, item.shortcut));
    }
    const float width = kPopupPadding * 2.0f + kCheckColumn + labelWidth + (shortcutWidth > 0.0f ? 32.0f + shortcutWidth : 0.0f) + 16.0f;
    return {std::ceil(std::max(width, 180.0f)), height};
}

void MenuPopup::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect b = bounds();
    // Soft shadow, then the rounded panel.
    draw.fillRoundedRect(Rect{b.x - 1.0f, b.y + 3.0f, b.width + 2.0f, b.height + 2.0f}, t.popupRadius + 1.0f, Color{0, 0, 0, 60});
    draw.fillRoundedRect(b, t.popupRadius, t.popup);
    draw.strokeRoundedRect(b, t.popupRadius, t.border);

    const FontMetrics fm = ctx.atlas().metrics(t.font, t.fontSize);
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        const MenuItem& item = m_items[i];
        const float y = itemTop(i);
        if (item.separator) {
            draw.fillRect(Rect{b.x + 10.0f, std::round(y + kSeparatorHeight * 0.5f), b.width - 20.0f, 1.0f}, t.border);
            continue;
        }
        const Rect row{b.x + kPopupPadding, y, b.width - 2.0f * kPopupPadding, kItemHeight};
        const bool enabled = isEnabled(i);
        const bool highlighted = enabled && m_highlight && *m_highlight == i;
        if (highlighted) {
            draw.fillRoundedRect(row, 4.0f, t.accent);
        }
        const Color ink = highlighted ? t.textOnAccent : (enabled ? t.text : t.textFaint);
        if (item.isChecked && item.isChecked()) {
            drawIcon(draw, Icon::Check, Rect{row.x + 3.0f, row.y + 5.0f, 16.0f, 16.0f}, ink);
        }
        const float textY = row.y + std::round((kItemHeight - fm.lineHeight) * 0.5f);
        draw.text(t.font, t.fontSize, {row.x + kCheckColumn, textY}, ink, item.label);
        if (!item.shortcut.empty()) {
            const float w = ctx.atlas().measure(t.font, t.fontSize, item.shortcut);
            draw.text(t.font, t.fontSize, {row.right() - 10.0f - w, textY},
                      highlighted ? t.textOnAccent : t.textFaint, item.shortcut);
        }
    }
}

std::optional<std::size_t> MenuPopup::itemAt(Vec2 point) const
{
    if (!bounds().contains(point)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        const float top = itemTop(i);
        const float h = m_items[i].separator ? kSeparatorHeight : kItemHeight;
        if (point.y >= top && point.y < top + h) {
            return m_items[i].separator ? std::nullopt : std::optional<std::size_t>(i);
        }
    }
    return std::nullopt;
}

void MenuPopup::onPointerMove(Context&, const PointerEvent& event)
{
    if (const auto i = itemAt(event.position); i && isEnabled(*i)) {
        m_highlight = i;
    } else {
        m_highlight.reset();
    }
}

void MenuPopup::onPointerLeave(Context&)
{
    m_highlight.reset();
}

bool MenuPopup::onPointerDown(Context&, const PointerEvent&)
{
    return true; // take the press; the choice happens on release
}

bool MenuPopup::onPointerUp(Context& ctx, const PointerEvent& event)
{
    if (const auto i = itemAt(event.position); i && isEnabled(*i)) {
        choose(ctx, *i);
    }
    return true;
}

void MenuPopup::choose(Context& ctx, std::size_t index)
{
    std::function<void()> action = m_items[index].action;
    ctx.closeOverlay(this);
    if (action) {
        ctx.post(std::move(action));
    }
}

void MenuPopup::moveHighlight(int direction)
{
    if (m_items.empty()) {
        return;
    }
    const auto n = static_cast<int>(m_items.size());
    int i = m_highlight ? static_cast<int>(*m_highlight) : (direction > 0 ? -1 : n);
    for (int step = 0; step < n; ++step) {
        i = (i + direction + n) % n;
        if (isEnabled(static_cast<std::size_t>(i))) {
            m_highlight = static_cast<std::size_t>(i);
            return;
        }
    }
}

bool MenuPopup::onKeyDown(Context& ctx, const KeyEvent& event)
{
    switch (event.key) {
    case Key::Down: moveHighlight(1); return true;
    case Key::Up: moveHighlight(-1); return true;
    case Key::Enter:
    case Key::Space:
        if (m_highlight) {
            choose(ctx, *m_highlight);
        }
        return true;
    case Key::Escape: ctx.closeOverlay(this); return true;
    default: return false;
    }
}

// --- MenuBar -----------------------------------------------------------------------------------

Size MenuBar::measure(Context& ctx, Size available)
{
    return applyFixedSize({available.width, ctx.theme().menuBarHeight});
}

void MenuBar::arrange(Context& ctx, Rect rect)
{
    Widget::arrange(ctx, rect);
    const Theme& t = ctx.theme();
    m_titleRects.clear();
    float x = rect.x + 6.0f;
    for (const Menu& menu : m_menus) {
        const float w = std::ceil(ctx.atlas().measure(t.font, t.fontSize, menu.title)) + 2.0f * kTitlePadding;
        m_titleRects.push_back(Rect{x, rect.y + 3.0f, w, rect.height - 6.0f});
        x += w + 2.0f;
    }
}

void MenuBar::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect b = bounds();
    draw.fillRect(b, t.panelHeader);
    draw.fillRect(Rect{b.x, b.bottom() - 1.0f, b.width, 1.0f}, t.divider);

    const FontMetrics fm = ctx.atlas().metrics(t.font, t.fontSize);
    const auto hovered = isHovered() ? titleAt(ctx.pointerPosition()) : std::nullopt;
    for (std::size_t i = 0; i < m_menus.size(); ++i) {
        const Rect r = m_titleRects[i];
        const bool open = m_open && *m_open == i;
        if (open) {
            draw.fillRoundedRect(r, 4.0f, t.controlPressed);
        } else if (hovered && *hovered == i) {
            draw.fillRoundedRect(r, 4.0f, t.controlHover);
        }
        draw.text(t.font, t.fontSize, {r.x + kTitlePadding, r.y + std::round((r.height - fm.lineHeight) * 0.5f)},
                  open ? t.text : t.textDim, m_menus[i].title);
    }
    paintChildren(ctx, draw);
}

std::optional<std::size_t> MenuBar::titleAt(Vec2 point) const
{
    for (std::size_t i = 0; i < m_titleRects.size(); ++i) {
        if (m_titleRects[i].contains(point)) {
            return i;
        }
    }
    return std::nullopt;
}

void MenuBar::open(Context& ctx, std::size_t index)
{
    if (index >= m_menus.size()) {
        return;
    }
    if (m_popup != nullptr) {
        ctx.closeOverlay(m_popup);
    }
    m_open = index;
    const Rect title = index < m_titleRects.size() ? m_titleRects[index] : Rect{};
    auto popup = std::make_unique<MenuPopup>(m_menus[index].items);
    Widget* raw = popup.get();
    m_popup = raw;
    ctx.openOverlay(std::move(popup), OverlayOptions{.position = {title.x, bounds().bottom() + 2.0f},
                                                     .onClosed = [this, raw] {
                                                         if (m_popup == raw) {
                                                             m_popup = nullptr;
                                                             m_open.reset();
                                                         }
                                                     }});
}

bool MenuBar::onPointerDown(Context& ctx, const PointerEvent& event)
{
    if (const auto i = titleAt(event.position)) {
        open(ctx, *i);
        return true;
    }
    return false;
}

void MenuBar::onPointerMove(Context& ctx, const PointerEvent& event)
{
    // While a menu is open, sliding along the bar switches menus (like macOS).
    if (m_open) {
        if (const auto i = titleAt(event.position); i && *i != *m_open) {
            open(ctx, *i);
        }
    }
}

} // namespace lantern::ui
