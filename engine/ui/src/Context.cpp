#include "lantern/ui/Context.hpp"

#include "lantern/ui/EmbeddedFonts.hpp"
#include "lantern/ui/Utf8.hpp"

#include <algorithm>
#include <cmath>

namespace lantern::ui {

namespace {
constexpr double kDoubleClickSeconds = 0.4;
constexpr float kDoubleClickDistance = 4.0f;
constexpr double kTooltipDelaySeconds = 0.55;

std::size_t buttonIndex(MouseButton button)
{
    return static_cast<std::size_t>(button);
}

Widget* topAncestor(Widget* widget)
{
    while (widget != nullptr && widget->parent() != nullptr) {
        widget = widget->parent();
    }
    return widget;
}

void collectFocusable(Widget& widget, std::vector<Widget*>& out)
{
    if (!widget.isVisible()) {
        return;
    }
    if (widget.isFocusable() && widget.isEnabled()) {
        out.push_back(&widget);
    }
    for (auto& child : widget.children()) {
        collectFocusable(*child, out);
    }
}
} // namespace

Context::Context(Theme theme)
    : m_theme(theme)
    , m_atlas(std::make_unique<FontAtlas>())
    , m_draw(std::make_unique<DrawList>(*m_atlas))
{
    if (auto regular = m_atlas->addFont(fonts::interRegular())) {
        m_theme.font = *regular;
        m_theme.fontStrong = *regular;
    }
    if (auto strong = m_atlas->addFont(fonts::interSemiBold())) {
        m_theme.fontStrong = *strong;
    }
}

Context::~Context()
{
    // Destroy widgets while the context (which they notify) is still whole.
    m_overlays.clear();
    m_root.reset();
}

// --- Tree --------------------------------------------------------------------------

void Context::setRoot(std::unique_ptr<Widget> root)
{
    m_root = std::move(root);
    if (m_root) {
        m_root->attach(this);
    }
}

void Context::forget(Widget* widget) noexcept
{
    if (m_hovered == widget) {
        m_hovered = nullptr;
    }
    if (m_focused == widget) {
        m_focused = nullptr;
    }
    if (m_captured == widget) {
        m_captured = nullptr;
    }
    if (m_lastClickTarget == widget) {
        m_lastClickTarget = nullptr;
    }
}

bool Context::isAncestorOrSelf(const Widget* ancestor, const Widget* widget)
{
    for (const Widget* w = widget; w != nullptr; w = w->parent()) {
        if (w == ancestor) {
            return true;
        }
    }
    return false;
}

// --- Frame ---------------------------------------------------------------------------

void Context::setViewport(Size size, float pixelScale)
{
    m_viewport = {std::max(1.0f, size.width), std::max(1.0f, size.height)};
    m_atlas->setPixelScale(pixelScale);
}

void Context::advanceTime(double seconds)
{
    m_time += std::max(0.0, seconds);
}

void Context::post(std::function<void()> task)
{
    m_posted.push_back(std::move(task));
}

void Context::runPosted()
{
    // Tasks may post more tasks; run until none are left (bounded).
    for (int round = 0; round < 8 && !m_posted.empty(); ++round) {
        std::vector<std::function<void()>> tasks;
        tasks.swap(m_posted);
        for (auto& task : tasks) {
            task();
        }
    }
}

void Context::layoutOverlays()
{
    for (Overlay& overlay : m_overlays) {
        Widget& w = *overlay.widget;
        Size size = w.measure(*this, m_viewport);
        size.width = std::min(size.width, m_viewport.width);
        size.height = std::min(size.height, m_viewport.height);
        Vec2 pos = overlay.options.centered
                       ? Vec2{(m_viewport.width - size.width) * 0.5f, (m_viewport.height - size.height) * 0.4f}
                       : overlay.options.position;
        // Keep popups fully inside the window.
        pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, m_viewport.width - size.width));
        pos.y = std::clamp(pos.y, 0.0f, std::max(0.0f, m_viewport.height - size.height));
        w.arrange(*this, Rect{std::round(pos.x), std::round(pos.y), size.width, size.height});
    }
}

const DrawList& Context::render()
{
    runPosted();
    const Rect full{0.0f, 0.0f, m_viewport.width, m_viewport.height};

    // If the glyph atlas grows while we draw, texture coordinates already
    // emitted are stale: simply draw the frame again (rare).
    for (int attempt = 0; attempt < 3; ++attempt) {
        m_atlas->consumeResetFlag();
        m_draw->reset(full);
        m_draw->fillRect(full, m_theme.window);

        if (m_root) {
            m_root->measure(*this, m_viewport);
            m_root->arrange(*this, full);
            m_root->paint(*this, *m_draw);
        }
        layoutOverlays();
        for (Overlay& overlay : m_overlays) {
            if (overlay.options.modal) {
                m_draw->fillRect(full, Color{0, 0, 0, 110});
            }
            overlay.widget->paint(*this, *m_draw);
        }
        paintTooltip(*m_draw);

        if (!m_atlas->consumeResetFlag()) {
            break;
        }
    }

    // Widgets may have moved: refresh what is under the pointer.
    if (m_captured == nullptr) {
        updateHover();
    }
    return *m_draw;
}

void Context::paintTooltip(DrawList& draw)
{
    if (m_hovered == nullptr || m_captured != nullptr || m_tooltipSuppressed || !m_pointerInside
        || m_time - m_hoverStart < kTooltipDelaySeconds) {
        return;
    }
    const std::string text = m_hovered->tooltip();
    if (text.empty()) {
        return;
    }
    const float size = m_theme.smallFontSize + 0.5f;
    const float width = m_atlas->measure(m_theme.font, size, text);
    const FontMetrics fm = m_atlas->metrics(m_theme.font, size);
    const Size box{width + 16.0f, fm.lineHeight + 10.0f};
    Vec2 pos{m_pointer.x + 12.0f, m_pointer.y + 20.0f};
    pos.x = std::clamp(pos.x, 4.0f, std::max(4.0f, m_viewport.width - box.width - 4.0f));
    if (pos.y + box.height > m_viewport.height - 4.0f) {
        pos.y = m_pointer.y - box.height - 8.0f;
    }
    const Rect rect{std::round(pos.x), std::round(pos.y), box.width, box.height};
    draw.fillRoundedRect(Rect{rect.x, rect.y + 2.0f, rect.width, rect.height}, 6.0f, Color{0, 0, 0, 70});
    draw.fillRoundedRect(rect, 6.0f, m_theme.popup);
    draw.strokeRoundedRect(rect, 6.0f, m_theme.border);
    draw.text(m_theme.font, size, {rect.x + 8.0f, rect.y + 5.0f}, m_theme.text, text);
}

// --- Hit testing and hover ------------------------------------------------------------

Widget* Context::hitTestAll(Vec2 point) const
{
    for (auto it = m_overlays.rbegin(); it != m_overlays.rend(); ++it) {
        if (it->closing) {
            continue;
        }
        if (Widget* hit = it->widget->hitTest(point)) {
            return hit;
        }
        if (it->options.modal) {
            return nullptr; // nothing below a modal dialog can be reached
        }
    }
    return m_root ? m_root->hitTest(point) : nullptr;
}

void Context::setHovered(Widget* widget)
{
    if (widget == m_hovered) {
        return;
    }
    Widget* old = m_hovered;
    m_hovered = widget;
    m_hoverStart = m_time;
    m_tooltipSuppressed = false;
    if (old != nullptr) {
        old->onPointerLeave(*this);
    }
    if (widget != nullptr) {
        widget->onPointerEnter(*this);
    }
}

void Context::updateHover()
{
    setHovered(m_pointerInside ? hitTestAll(m_pointer) : nullptr);
    const Widget* source = m_captured != nullptr ? m_captured : m_hovered;
    m_cursor = source != nullptr ? source->cursor(m_pointer) : CursorShape::Arrow;
}

// --- Pointer input ----------------------------------------------------------------------

bool Context::isPointerDown(MouseButton button) const noexcept
{
    return m_buttons[buttonIndex(button)];
}

void Context::pointerMove(Vec2 position)
{
    m_pointer = position;
    m_pointerInside = true;
    const PointerEvent event{position, MouseButton::Left, m_modifiers, 1};
    if (m_captured != nullptr) {
        m_captured->onPointerMove(*this, event);
        m_cursor = m_captured != nullptr ? m_captured->cursor(m_pointer) : CursorShape::Arrow;
    } else {
        updateHover();
        if (m_hovered != nullptr) {
            m_hovered->onPointerMove(*this, event);
        }
    }
    runPosted();
}

void Context::pointerLeftWindow()
{
    m_pointerInside = false;
    if (m_captured == nullptr) {
        setHovered(nullptr);
        m_cursor = CursorShape::Arrow;
    }
}

void Context::pointerButton(MouseButton button, bool pressed, Modifiers modifiers)
{
    m_modifiers = modifiers;
    m_buttons[buttonIndex(button)] = pressed;
    m_tooltipSuppressed = true;
    PointerEvent event{m_pointer, button, modifiers, 1};

    if (pressed) {
        Widget* target = hitTestAll(m_pointer);

        // A click outside open popups closes them (and does nothing else).
        if (!m_overlays.empty() && !isInOverlay(target)) {
            bool closedAny = false;
            for (auto it = m_overlays.rbegin(); it != m_overlays.rend(); ++it) {
                if (it->options.modal) {
                    break;
                }
                if (it->options.closeOnOutsideClick && !it->closing) {
                    closeOverlay(it->widget.get());
                    closedAny = true;
                }
            }
            if (closedAny) {
                runPosted();
                updateHover();
                return;
            }
        }

        // Double / triple clicks.
        const Vec2 d = m_pointer - m_lastClickPosition;
        const bool near = std::abs(d.x) <= kDoubleClickDistance && std::abs(d.y) <= kDoubleClickDistance;
        if (target != nullptr && target == m_lastClickTarget && near && m_time - m_lastClickTime < kDoubleClickSeconds
            && button == MouseButton::Left) {
            ++m_clickCount;
        } else {
            m_clickCount = 1;
        }
        m_lastClickTarget = target;
        m_lastClickTime = m_time;
        m_lastClickPosition = m_pointer;
        event.clickCount = m_clickCount;

        // Clicking moves keyboard focus to the nearest focusable widget (or clears it).
        Widget* focusTarget = target;
        while (focusTarget != nullptr && !(focusTarget->isFocusable() && focusTarget->isEnabled())) {
            focusTarget = focusTarget->parent();
        }
        setFocus(focusTarget);

        for (Widget* w = target; w != nullptr; w = w->parent()) {
            if (w->isEnabled() && w->onPointerDown(*this, event)) {
                capturePointer(w);
                break;
            }
        }
    } else {
        if (m_captured != nullptr) {
            Widget* captured = m_captured;
            captured->onPointerUp(*this, event);
            releasePointer();
        } else {
            for (Widget* w = hitTestAll(m_pointer); w != nullptr; w = w->parent()) {
                if (w->isEnabled() && w->onPointerUp(*this, event)) {
                    break;
                }
            }
        }
        updateHover();
    }
    runPosted();
}

void Context::scroll(Vec2 delta)
{
    const ScrollEvent event{m_pointer, delta};
    for (Widget* w = hitTestAll(m_pointer); w != nullptr; w = w->parent()) {
        if (w->isEnabled() && w->onScroll(*this, event)) {
            break;
        }
    }
    runPosted();
}

void Context::capturePointer(Widget* widget)
{
    m_captured = widget;
}

void Context::releasePointer()
{
    m_captured = nullptr;
}

// --- Keyboard ---------------------------------------------------------------------------

void Context::setFocus(Widget* widget)
{
    if (widget == m_focused) {
        return;
    }
    Widget* old = m_focused;
    m_focused = widget;
    if (old != nullptr) {
        old->onFocusChanged(*this, false);
    }
    if (widget != nullptr && m_focused == widget) {
        widget->onFocusChanged(*this, true);
    }
}

void Context::focusNext(bool backwards)
{
    // Tab moves within the top modal dialog if there is one, else the main tree.
    Widget* layer = m_root.get();
    for (auto it = m_overlays.rbegin(); it != m_overlays.rend(); ++it) {
        if (it->options.modal && !it->closing) {
            layer = it->widget.get();
            break;
        }
    }
    if (layer == nullptr) {
        return;
    }
    std::vector<Widget*> order;
    collectFocusable(*layer, order);
    if (order.empty()) {
        return;
    }
    auto it = std::find(order.begin(), order.end(), m_focused);
    std::size_t index = 0;
    if (it != order.end()) {
        const auto current = static_cast<std::size_t>(it - order.begin());
        index = backwards ? (current + order.size() - 1) % order.size() : (current + 1) % order.size();
    } else if (backwards) {
        index = order.size() - 1;
    }
    setFocus(order[index]);
}

bool Context::dispatchKey(const KeyEvent& event)
{
    Widget* target = m_focused;
    // While a popup is open, keys go to it unless focus is already inside it.
    if (!m_overlays.empty()) {
        Overlay& top = m_overlays.back();
        if (!top.closing && !isAncestorOrSelf(top.widget.get(), target)) {
            target = top.widget.get();
        }
    }
    for (Widget* w = target; w != nullptr; w = w->parent()) {
        if (w->isEnabled() && w->onKeyDown(*this, event)) {
            return true;
        }
    }
    return false;
}

void Context::keyDown(Key key, Modifiers modifiers, bool repeat)
{
    m_modifiers = modifiers;
    m_tooltipSuppressed = true;
    const KeyEvent event{key, modifiers, repeat};

    if (!dispatchKey(event)) {
        if (key == Key::Tab && !modifiers.ctrl && !modifiers.super && !modifiers.alt) {
            focusNext(modifiers.shift);
        } else if (key == Key::Escape && !m_overlays.empty()) {
            closeOverlay(m_overlays.back().widget.get());
        } else if (!repeat) {
            for (const Shortcut& shortcut : m_shortcuts) {
                if (shortcut.key == key && shortcut.modifiers == modifiers && shortcut.action) {
                    shortcut.action();
                    break;
                }
            }
        }
    }
    runPosted();
}

void Context::keyUp(Key key, Modifiers modifiers)
{
    m_modifiers = modifiers;
    const KeyEvent event{key, modifiers, false};
    for (Widget* w = m_focused; w != nullptr; w = w->parent()) {
        if (w->isEnabled() && w->onKeyUp(*this, event)) {
            break;
        }
    }
    runPosted();
}

void Context::textInput(char32_t codepoint)
{
    if (codepoint < 32 || codepoint == 127) {
        return; // control characters arrive as key events instead
    }
    std::string text;
    utf8::append(text, codepoint);
    for (Widget* w = m_focused; w != nullptr; w = w->parent()) {
        if (w->isEnabled() && w->onText(*this, text)) {
            break;
        }
    }
    runPosted();
}

void Context::addShortcut(Key key, Modifiers modifiers, std::function<void()> action)
{
    m_shortcuts.push_back(Shortcut{key, modifiers, std::move(action)});
}

// --- Overlays ----------------------------------------------------------------------------

Widget& Context::openOverlay(std::unique_ptr<Widget> widget, OverlayOptions options)
{
    widget->attach(this);
    Widget& ref = *widget;
    m_overlays.push_back(Overlay{std::move(widget), std::move(options), false});
    return ref;
}

void Context::closeOverlay(Widget* widget)
{
    Widget* top = topAncestor(widget);
    for (Overlay& overlay : m_overlays) {
        if (overlay.widget.get() == top && !overlay.closing) {
            overlay.closing = true;
            post([this, top] {
                auto it = std::find_if(m_overlays.begin(), m_overlays.end(),
                                       [top](const Overlay& o) { return o.widget.get() == top; });
                if (it == m_overlays.end()) {
                    return;
                }
                std::function<void()> onClosed = std::move(it->options.onClosed);
                m_overlays.erase(it); // destroys the overlay's widgets
                if (onClosed) {
                    onClosed();
                }
            });
            return;
        }
    }
}

void Context::closeAllOverlays()
{
    for (Overlay& overlay : m_overlays) {
        if (!overlay.closing) {
            closeOverlay(overlay.widget.get());
        }
    }
}

bool Context::hasModal() const noexcept
{
    return std::any_of(m_overlays.begin(), m_overlays.end(),
                       [](const Overlay& o) { return o.options.modal && !o.closing; });
}

bool Context::isInOverlay(const Widget* widget) const
{
    if (widget == nullptr) {
        return false;
    }
    const Widget* top = widget;
    while (top->parent() != nullptr) {
        top = top->parent();
    }
    return std::any_of(m_overlays.begin(), m_overlays.end(),
                       [top](const Overlay& o) { return o.widget.get() == top; });
}

} // namespace lantern::ui
