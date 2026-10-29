#include "lantern/ui/widgets/Basic.hpp"

#include "lantern/ui/Context.hpp"
#include "lantern/ui/Utf8.hpp"

#include <algorithm>
#include <cmath>

namespace lantern::ui {

IconView::IconView(Icon icon, float size, std::optional<Color> color)
    : m_icon(icon)
    , m_size(size)
    , m_color(color)
{
}

Size IconView::measure(Context&, Size)
{
    return applyFixedSize({m_size, m_size});
}

void IconView::paint(Context& ctx, DrawList& draw)
{
    const Rect b = bounds();
    const Rect box{b.center().x - m_size * 0.5f, b.center().y - m_size * 0.5f, m_size, m_size};
    drawIcon(draw, m_icon, box, m_color.value_or(ctx.theme().textDim));
}

void paintFocusRing(Context& ctx, DrawList& draw, Rect rect, float radius)
{
    draw.strokeRoundedRect(rect.inset(-2.0f), radius + 2.0f, ctx.theme().focusRing, 1.5f);
}

// --- Label ---------------------------------------------------------------------------------

Label::Label(std::string text, Style style)
    : m_text(std::move(text))
    , m_style(style)
{
}

Label::Resolved Label::resolve(Context& ctx) const
{
    const Theme& t = ctx.theme();
    Resolved r{t.font, t.fontSize, t.text};
    switch (m_style) {
    case Style::Normal: break;
    case Style::Strong: r.font = t.fontStrong; break;
    case Style::Dim: r.color = t.textDim; break;
    case Style::Faint: r.color = t.textFaint; break;
    case Style::Small:
        r.size = t.smallFontSize;
        r.color = t.textDim;
        break;
    }
    if (m_color) {
        r.color = *m_color;
    }
    if (!isEnabled()) {
        r.color = r.color.fade(0.45f);
    }
    return r;
}

std::vector<std::string> Label::wrapLines(Context& ctx, float width) const
{
    const Resolved r = resolve(ctx);
    std::vector<std::string> lines;
    std::string line;
    float lineWidth = 0.0f;
    const float space = ctx.atlas().measure(r.font, r.size, " ");

    std::size_t i = 0;
    while (i <= m_text.size()) {
        // Take the next word (or an explicit line break).
        const std::size_t end = m_text.find_first_of(" \n", i);
        const std::size_t wordEnd = end == std::string::npos ? m_text.size() : end;
        const std::string word = m_text.substr(i, wordEnd - i);
        const float w = ctx.atlas().measure(r.font, r.size, word);
        if (!line.empty() && lineWidth + space + w > width) {
            lines.push_back(line);
            line.clear();
            lineWidth = 0.0f;
        }
        if (!line.empty()) {
            line += ' ';
            lineWidth += space;
        }
        line += word;
        lineWidth += w;
        if (end != std::string::npos && m_text[end] == '\n') {
            lines.push_back(line);
            line.clear();
            lineWidth = 0.0f;
        }
        if (end == std::string::npos) {
            break;
        }
        i = end + 1;
    }
    if (!line.empty() || lines.empty()) {
        lines.push_back(line);
    }
    return lines;
}

Size Label::measure(Context& ctx, Size available)
{
    const Resolved r = resolve(ctx);
    const FontMetrics fm = ctx.atlas().metrics(r.font, r.size);
    const float lineHeight = std::ceil(fm.lineHeight);
    if (m_wrap) {
        const float width = std::max(1.0f, available.width);
        const auto lines = wrapLines(ctx, width);
        float widest = 0.0f;
        for (const auto& line : lines) {
            widest = std::max(widest, ctx.atlas().measure(r.font, r.size, line));
        }
        return applyFixedSize({std::ceil(std::min(widest, width)), lineHeight * static_cast<float>(lines.size())});
    }
    return applyFixedSize({std::ceil(ctx.atlas().measure(r.font, r.size, m_text)), lineHeight});
}

void Label::paint(Context& ctx, DrawList& draw)
{
    const Resolved r = resolve(ctx);
    const FontMetrics fm = ctx.atlas().metrics(r.font, r.size);
    const float lineHeight = std::ceil(fm.lineHeight);
    const Rect b = bounds();
    draw.pushClip(b);

    auto drawLine = [&](std::string_view line, float y) {
        const float w = ctx.atlas().measure(r.font, r.size, line);
        float x = b.x;
        if (m_align == TextAlign::Center) {
            x = b.x + std::round((b.width - w) * 0.5f);
        } else if (m_align == TextAlign::End) {
            x = b.right() - w;
        }
        draw.textEllipsized(r.font, r.size, {std::max(x, b.x), y}, r.color, line, b.width);
    };

    if (m_wrap) {
        float y = b.y;
        for (const auto& line : wrapLines(ctx, b.width)) {
            drawLine(line, y);
            y += lineHeight;
        }
    } else {
        drawLine(m_text, b.y + std::round((b.height - lineHeight) * 0.5f));
    }
    draw.popClip();
}

Widget* Label::hitTest(Vec2 point)
{
    return tooltip().empty() ? nullptr : Widget::hitTest(point);
}

// --- Button ----------------------------------------------------------------------------------

Button::Button(std::string text, std::function<void()> onClick)
    : m_text(std::move(text))
    , m_onClick(std::move(onClick))
{
    setFocusable(true);
}

Size Button::measure(Context& ctx, Size)
{
    const Theme& t = ctx.theme();
    float width = 0.0f;
    if (m_icon) {
        width += 16.0f;
    }
    if (!m_text.empty()) {
        width += (m_icon ? 6.0f : 0.0f) + ctx.atlas().measure(t.font, t.fontSize, m_text);
    }
    const float padding = m_text.empty() ? 5.0f : 12.0f;
    return applyFixedSize({std::ceil(width + padding * 2.0f), t.controlHeight});
}

void Button::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect b = bounds();
    const bool pressed = hasPointerCapture() && b.contains(ctx.pointerPosition());
    const bool hovered = isHovered() || hasPointerCapture();
    const bool enabled = isEnabled();

    Color fill = t.control;
    Color ink = t.text;
    switch (m_style) {
    case Style::Normal: fill = pressed ? t.controlPressed : hovered ? t.controlHover : t.control; break;
    case Style::Primary:
        fill = pressed ? t.accent : hovered ? t.accentHover : t.accent;
        ink = t.textOnAccent;
        break;
    case Style::Subtle:
        fill = pressed ? t.controlPressed : hovered ? t.controlHover : Color{0, 0, 0, 0};
        ink = hovered ? t.text : t.textDim;
        break;
    }
    if (m_toggled) {
        fill = hovered ? t.controlHover : t.control;
        ink = t.accent;
    }
    if (!enabled) {
        fill = fill.fade(0.5f);
        ink = ink.fade(0.45f);
    }
    draw.fillRoundedRect(b, t.radius, fill);

    float contentWidth = 0.0f;
    const float textWidth = m_text.empty() ? 0.0f : ctx.atlas().measure(t.font, t.fontSize, m_text);
    if (m_icon) {
        contentWidth += 16.0f;
    }
    if (!m_text.empty()) {
        contentWidth += (m_icon ? 6.0f : 0.0f) + textWidth;
    }
    float x = b.x + std::round((b.width - contentWidth) * 0.5f);
    if (m_icon) {
        drawIcon(draw, *m_icon, Rect{x, b.center().y - 8.0f, 16.0f, 16.0f}, ink);
        x += 22.0f;
    }
    if (!m_text.empty()) {
        const FontMetrics fm = ctx.atlas().metrics(t.font, t.fontSize);
        draw.text(t.font, t.fontSize, {x, b.y + std::round((b.height - fm.lineHeight) * 0.5f)}, ink, m_text);
    }
    if (isFocused() && enabled) {
        paintFocusRing(ctx, draw, b, t.radius);
    }
}

bool Button::onPointerDown(Context&, const PointerEvent& event)
{
    return event.button == MouseButton::Left;
}

bool Button::onPointerUp(Context& ctx, const PointerEvent& event)
{
    if (bounds().contains(event.position)) {
        activate(ctx);
    }
    return true;
}

bool Button::onKeyDown(Context& ctx, const KeyEvent& event)
{
    if ((event.key == Key::Space || event.key == Key::Enter) && event.modifiers.none()) {
        activate(ctx);
        return true;
    }
    return false;
}

void Button::activate(Context& ctx)
{
    if (m_onClick && isEnabled()) {
        // Run after the event, so the action may safely change the widget tree.
        ctx.post(m_onClick);
    }
}

// --- Checkbox --------------------------------------------------------------------------------

Checkbox::Checkbox(std::string text, bool checked, std::function<void(bool)> onChange)
    : m_text(std::move(text))
    , m_checked(checked)
    , m_onChange(std::move(onChange))
{
    setFocusable(true);
}

Size Checkbox::measure(Context& ctx, Size)
{
    const Theme& t = ctx.theme();
    const float textWidth = m_text.empty() ? 0.0f : 6.0f + ctx.atlas().measure(t.font, t.fontSize, m_text);
    return applyFixedSize({std::ceil(16.0f + textWidth), t.controlHeight});
}

void Checkbox::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect b = bounds();
    const Rect box{b.x + 1.0f, b.center().y - 7.0f, 14.0f, 14.0f};
    const bool hovered = isHovered() || hasPointerCapture();
    const float alpha = isEnabled() ? 1.0f : 0.45f;

    if (m_checked) {
        draw.fillRoundedRect(box, 2.5f, (hovered ? t.accentHover : t.accent).fade(alpha));
        drawIcon(draw, Icon::Check, box.inset(-1.0f), t.textOnAccent.fade(alpha));
    } else {
        draw.fillRoundedRect(box, 2.5f, (hovered ? t.fieldHover : t.field).fade(alpha));
        draw.strokeRoundedRect(box, 2.5f, t.border.fade(alpha));
    }
    if (!m_text.empty()) {
        const FontMetrics fm = ctx.atlas().metrics(t.font, t.fontSize);
        draw.text(t.font, t.fontSize, {box.right() + 6.0f, b.y + std::round((b.height - fm.lineHeight) * 0.5f)},
                  m_textColor.value_or(t.text).fade(alpha), m_text);
    }
    if (isFocused()) {
        paintFocusRing(ctx, draw, box, 4.0f);
    }
}

bool Checkbox::onPointerDown(Context&, const PointerEvent& event)
{
    return event.button == MouseButton::Left;
}

bool Checkbox::onPointerUp(Context& ctx, const PointerEvent& event)
{
    if (bounds().contains(event.position)) {
        toggle(ctx);
    }
    return true;
}

bool Checkbox::onKeyDown(Context& ctx, const KeyEvent& event)
{
    if (event.key == Key::Space && event.modifiers.none()) {
        toggle(ctx);
        return true;
    }
    return false;
}

void Checkbox::toggle(Context& ctx)
{
    m_checked = !m_checked;
    if (m_onChange) {
        ctx.post([this, value = m_checked] {
            if (m_onChange) {
                m_onChange(value);
            }
        });
    }
}

// --- ImageView -------------------------------------------------------------------------------

ImageView::ImageView(TextureId texture, Size imageSize)
    : m_texture(texture)
    , m_imageSize(imageSize)
{
}

void ImageView::setTexture(TextureId texture, Size imageSize)
{
    m_texture = texture;
    m_imageSize = imageSize;
}

void ImageView::setUv(Vec2 uv0, Vec2 uv1)
{
    m_uv0 = uv0;
    m_uv1 = uv1;
}

void ImageView::arrange(Context& ctx, Rect rect)
{
    Widget::arrange(ctx, rect);
    if (m_imageSize.width <= 0.0f || m_imageSize.height <= 0.0f || rect.isEmpty()) {
        m_imageRect = {};
        m_scale = 0.0f;
        return;
    }
    // Fit inside without stretching ("letterbox"), centred, on whole points.
    m_scale = std::min(rect.width / m_imageSize.width, rect.height / m_imageSize.height);
    const float w = std::round(m_imageSize.width * m_scale);
    const float h = std::round(m_imageSize.height * m_scale);
    m_imageRect = {std::round(rect.x + (rect.width - w) * 0.5f), std::round(rect.y + (rect.height - h) * 0.5f), w, h};
}

void ImageView::paint(Context& ctx, DrawList& draw)
{
    if (m_background) {
        draw.fillRect(bounds(), *m_background);
    }
    if (m_texture != 0 && !m_imageRect.isEmpty()) {
        draw.image(m_texture, m_imageRect, m_uv0, m_uv1);
    }
    if (!m_caption.empty() && !m_imageRect.isEmpty()) {
        const Theme& t = ctx.theme();
        const float size = t.smallFontSize;
        const float width = ctx.atlas().measure(t.font, size, m_caption);
        const FontMetrics fm = ctx.atlas().metrics(t.font, size);
        const Rect chip{m_imageRect.x + 8.0f, m_imageRect.y + 8.0f, std::ceil(width) + 12.0f, std::ceil(fm.lineHeight) + 6.0f};
        draw.fillRoundedRect(chip, 4.0f, Color{0, 0, 0, 150});
        draw.text(t.font, size, {chip.x + 6.0f, chip.y + 3.0f}, Color::hex(0xE8E8E8), m_caption);
    }
    paintChildren(ctx, draw);
}

} // namespace lantern::ui
