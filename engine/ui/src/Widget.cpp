#include "lantern/ui/Widget.hpp"

#include "lantern/ui/Context.hpp"

#include <algorithm>

namespace lantern::ui {

Widget::~Widget()
{
    if (m_context != nullptr) {
        m_context->forget(this);
    }
    // Children are destroyed after this body runs; each forgets itself too.
}

void Widget::adopt(std::unique_ptr<Widget> child)
{
    child->m_parent = this;
    child->attach(m_context);
    m_children.push_back(std::move(child));
}

void Widget::attach(Context* context)
{
    if (m_context != nullptr && m_context != context) {
        m_context->forget(this);
    }
    m_context = context;
    for (auto& child : m_children) {
        child->attach(context);
    }
}

std::unique_ptr<Widget> Widget::removeChild(Widget& child)
{
    auto it = std::find_if(m_children.begin(), m_children.end(),
                           [&child](const std::unique_ptr<Widget>& c) { return c.get() == &child; });
    if (it == m_children.end()) {
        return nullptr;
    }
    std::unique_ptr<Widget> owned = std::move(*it);
    m_children.erase(it);
    owned->attach(nullptr);
    owned->m_parent = nullptr;
    return owned;
}

void Widget::clearChildren()
{
    m_children.clear();
}

Widget* Widget::findByName(std::string_view name)
{
    if (m_name == name) {
        return this;
    }
    for (auto& child : m_children) {
        if (Widget* found = child->findByName(name)) {
            return found;
        }
    }
    return nullptr;
}

Size Widget::applyFixedSize(Size measured) const
{
    return {m_fixedSize.width > 0.0f ? m_fixedSize.width : measured.width,
            m_fixedSize.height > 0.0f ? m_fixedSize.height : measured.height};
}

Size Widget::measure(Context& ctx, Size available)
{
    Size result;
    for (auto& child : m_children) {
        if (child->isVisible()) {
            const Size s = child->measure(ctx, available);
            result.width = std::max(result.width, s.width);
            result.height = std::max(result.height, s.height);
        }
    }
    return applyFixedSize(result);
}

void Widget::arrange(Context& ctx, Rect rect)
{
    setBounds(rect);
    for (auto& child : m_children) {
        if (child->isVisible()) {
            child->arrange(ctx, rect);
        }
    }
}

void Widget::paint(Context& ctx, DrawList& draw)
{
    paintChildren(ctx, draw);
}

void Widget::paintChildren(Context& ctx, DrawList& draw)
{
    for (auto& child : m_children) {
        if (child->isVisible()) {
            child->paint(ctx, draw);
        }
    }
}

Widget* Widget::hitTest(Vec2 point)
{
    if (!m_visible || !m_bounds.contains(point)) {
        return nullptr;
    }
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        if (Widget* hit = (*it)->hitTest(point)) {
            return hit;
        }
    }
    return this;
}

bool Widget::onPointerDown(Context&, const PointerEvent&)
{
    return false;
}
bool Widget::onPointerUp(Context&, const PointerEvent&)
{
    return false;
}
void Widget::onPointerMove(Context&, const PointerEvent&) {}
void Widget::onPointerEnter(Context&) {}
void Widget::onPointerLeave(Context&) {}
bool Widget::onScroll(Context&, const ScrollEvent&)
{
    return false;
}
bool Widget::onKeyDown(Context&, const KeyEvent&)
{
    return false;
}
bool Widget::onKeyUp(Context&, const KeyEvent&)
{
    return false;
}
bool Widget::onText(Context&, std::string_view)
{
    return false;
}
void Widget::onFocusChanged(Context&, bool) {}

CursorShape Widget::cursor(Vec2) const
{
    return CursorShape::Arrow;
}

bool Widget::isEnabled() const noexcept
{
    for (const Widget* w = this; w != nullptr; w = w->m_parent) {
        if (!w->m_enabled) {
            return false;
        }
    }
    return true;
}

bool Widget::isHovered() const noexcept
{
    return m_context != nullptr && m_context->hovered() == this;
}

bool Widget::isFocused() const noexcept
{
    return m_context != nullptr && m_context->focused() == this;
}

bool Widget::hasPointerCapture() const noexcept
{
    return m_context != nullptr && m_context->captured() == this;
}

} // namespace lantern::ui
