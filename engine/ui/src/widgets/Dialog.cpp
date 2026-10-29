#include "lantern/ui/widgets/Dialog.hpp"

#include "lantern/ui/Context.hpp"

#include <cmath>

namespace lantern::ui {

namespace {
constexpr float kTitleHeight = 44.0f;
constexpr float kPadding = 18.0f;
} // namespace

Dialog::Dialog(std::string title, float width)
    : m_title(std::move(title))
    , m_width(width)
{
    m_body = &emplace<Column>(6.0f);
    m_buttons = &emplace<Row>(8.0f);
    m_buttons->emplace<Spacer>();
}

Button& Dialog::addButton(std::string text, Button::Style style, std::function<void()> action)
{
    Button& button = m_buttons->emplace<Button>(std::move(text));
    button.setStyle(style);
    button.setFixedSize({0.0f, 0.0f});
    button.setOnClick([this, action = std::move(action)] {
        if (Context* ctx = context()) {
            ctx->closeOverlay(this);
        }
        if (action) {
            action();
        }
    });
    if (style == Button::Style::Primary) {
        m_primary = &button;
    }
    return button;
}

Size Dialog::measure(Context& ctx, Size available)
{
    const float inner = m_width - 2.0f * kPadding;
    const float bodyHeight = m_body->measure(ctx, {inner, available.height}).height;
    const float buttonsHeight = m_buttons->measure(ctx, {inner, available.height}).height;
    return {m_width, kTitleHeight + bodyHeight + kPadding + buttonsHeight + kPadding};
}

void Dialog::arrange(Context& ctx, Rect rect)
{
    setBounds(rect);
    const float inner = rect.width - 2.0f * kPadding;
    const float bodyHeight = m_body->measure(ctx, {inner, rect.height}).height;
    m_body->arrange(ctx, Rect{rect.x + kPadding, rect.y + kTitleHeight, inner, bodyHeight});
    const float buttonsHeight = m_buttons->measure(ctx, {inner, rect.height}).height;
    m_buttons->arrange(ctx, Rect{rect.x + kPadding, rect.bottom() - kPadding - buttonsHeight, inner, buttonsHeight});
}

void Dialog::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect b = bounds();
    draw.fillRoundedRect(Rect{b.x - 2.0f, b.y + 6.0f, b.width + 4.0f, b.height + 4.0f}, 12.0f, Color{0, 0, 0, 90});
    draw.fillRoundedRect(b, 10.0f, t.popup);
    draw.strokeRoundedRect(b, 10.0f, t.border);
    draw.text(t.fontStrong, t.fontSize + 2.0f, {b.x + kPadding, b.y + 15.0f}, t.text, m_title);
    paintChildren(ctx, draw);
}

bool Dialog::onKeyDown(Context& ctx, const KeyEvent& event)
{
    if (event.key == Key::Enter && m_primary != nullptr) {
        m_primary->onKeyDown(ctx, KeyEvent{Key::Enter, {}, false});
        return true;
    }
    if (event.key == Key::Escape) {
        ctx.closeOverlay(this);
        return true;
    }
    return false;
}

Dialog& showDialog(Context& ctx, std::unique_ptr<Dialog> dialog)
{
    Widget& w = ctx.openOverlay(std::move(dialog),
                                OverlayOptions{.centered = true, .modal = true, .closeOnOutsideClick = false});
    ctx.focusNext(); // put keyboard focus inside the dialog
    return static_cast<Dialog&>(w);
}

} // namespace lantern::ui
