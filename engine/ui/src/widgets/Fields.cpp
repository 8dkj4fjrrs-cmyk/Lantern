#include "rolo/ui/widgets/Fields.hpp"

#include "rolo/core/Numbers.hpp"
#include "rolo/ui/Context.hpp"
#include "rolo/ui/Icons.hpp"
#include "rolo/ui/widgets/Basic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace rolo::ui {

namespace {

constexpr float kFieldHeight = 22.0f;
constexpr float kSuffixPad = 6.0f;
constexpr float kScrubPointsPerStep = 4.0f;   // drag distance for one step

float textTop(Context& ctx, float size, const Rect& rect)
{
    const FontMetrics fm = ctx.atlas().metrics(ctx.theme().font, size);
    return std::round(rect.y + (rect.height - fm.lineHeight) * 0.5f);
}

void paintChecker(DrawList& draw, Rect rect)
{
    draw.pushClip(rect);
    const float cell = 5.0f;
    draw.fillRect(rect, Color::hex(0xD0D0D0));
    for (float y = rect.y; y < rect.bottom(); y += cell) {
        for (float x = rect.x; x < rect.right(); x += cell) {
            const int ix = static_cast<int>((x - rect.x) / cell);
            const int iy = static_cast<int>((y - rect.y) / cell);
            if ((ix + iy) % 2 == 1) {
                draw.fillRect(Rect{x, y, cell, cell}, Color::hex(0x8A8A8A));
            }
        }
    }
    draw.popClip();
}

std::uint8_t toByte(float v)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

// ---------------------------------------------------------------------------
// HSV
// ---------------------------------------------------------------------------
void rgbToHsv(Color c, float& h, float& s, float& v)
{
    const float r = static_cast<float>(c.r) / 255.0f;
    const float g = static_cast<float>(c.g) / 255.0f;
    const float b = static_cast<float>(c.b) / 255.0f;
    const float maxC = std::max({r, g, b});
    const float minC = std::min({r, g, b});
    const float delta = maxC - minC;
    v = maxC;
    s = maxC > 0.0f ? delta / maxC : 0.0f;
    if (delta <= 0.0f) {
        return; // grey: keep the hue the caller had
    }
    float hue = 0.0f;
    if (maxC == r) {
        hue = (g - b) / delta;
    } else if (maxC == g) {
        hue = 2.0f + (b - r) / delta;
    } else {
        hue = 4.0f + (r - g) / delta;
    }
    hue /= 6.0f;
    h = hue < 0.0f ? hue + 1.0f : hue;
}

Color hsvToRgb(float h, float s, float v, std::uint8_t alpha)
{
    h = h - std::floor(h);
    const float scaled = h * 6.0f;
    const int sector = static_cast<int>(scaled) % 6;
    const float f = scaled - std::floor(scaled);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));
    float r = v, g = t, b = p;
    switch (sector) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return Color{toByte(r), toByte(g), toByte(b), alpha};
}

// ---------------------------------------------------------------------------
// NumberField
// ---------------------------------------------------------------------------
NumberField::NumberField()
    : TextField("")
{
    setSelectAllOnFocus(true);
    setFixedSize({0.0f, kFieldHeight});
    setStretch(1.0f);
    setOnCommit([this](const std::string& text) {
        Context* ctx = context();
        if (const auto parsed = parse(text); parsed && ctx != nullptr) {
            emit(*ctx, *parsed, true);
        } else {
            setText(format(m_value, m_integer)); // not a number: show the old value again
        }
    });
}

std::string NumberField::format(float value, bool integer)
{
    if (integer) {
        return std::to_string(static_cast<long long>(std::llround(value)));
    }
    return formatDecimals(static_cast<double>(value), 3);
}

std::optional<float> NumberField::parse(std::string_view text)
{
    std::string clean;
    for (char c : text) {
        if (c == ',') {
            clean += '.';
        } else if (c != ' ') {
            clean += c;
        }
    }
    if (!clean.empty() && clean.front() == '+') {
        clean.erase(0, 1);
    }
    if (clean.empty()) {
        return std::nullopt;
    }
    const auto value = parseNumber(clean);
    if (!value || std::abs(*value) > 1.0e30) {
        return std::nullopt;
    }
    return static_cast<float>(*value);
}

void NumberField::setValue(float value)
{
    m_value = value;
    if (!isEditing() && !m_scrubbing) {
        const std::string text = format(value, m_integer);
        if (text != this->text()) {
            setText(text);
        }
    }
}

void NumberField::setRange(std::optional<float> min, std::optional<float> max)
{
    m_min = min;
    m_max = max;
}

void NumberField::setSuffix(std::string suffix)
{
    m_suffix = std::move(suffix);
    setRightInset(m_suffix.empty() ? 0.0f : 16.0f);
}

float NumberField::clamp(float value) const
{
    if (m_min) {
        value = std::max(value, *m_min);
    }
    if (m_max) {
        value = std::min(value, *m_max);
    }
    if (m_integer) {
        value = std::round(value);
    }
    return value;
}

void NumberField::emit(Context&, float value, bool final)
{
    value = clamp(value);
    const bool changed = value != m_value;
    m_value = value;
    if (!isEditing()) {
        setText(format(value, m_integer));
    }
    if (changed && m_onValue) {
        m_onValue(value);
    }
    if (final && m_onEditEnd) {
        m_onEditEnd();
    }
}

Rect NumberField::scrubArea() const
{
    if (m_suffix.empty()) {
        return {};
    }
    const Rect b = bounds();
    return Rect{b.right() - 18.0f, b.y, 18.0f, b.height};
}

void NumberField::paint(Context& ctx, DrawList& draw)
{
    TextField::paint(ctx, draw);
    if (m_suffix.empty()) {
        return;
    }
    const Theme& t = ctx.theme();
    const Rect area = scrubArea();
    const bool hot = (isHovered() && area.contains(ctx.pointerPosition())) || m_scrubbing;
    const float w = ctx.atlas().measure(t.font, t.smallFontSize, m_suffix);
    draw.text(t.font, t.smallFontSize, {area.right() - kSuffixPad - w, textTop(ctx, t.smallFontSize, area)},
              hot ? t.accent : t.textFaint, m_suffix);
}

bool NumberField::onPointerDown(Context& ctx, const PointerEvent& event)
{
    if (event.button == MouseButton::Left && scrubArea().contains(event.position) && !isEditing()) {
        m_scrubbing = true;
        m_scrubStartX = event.position.x;
        m_scrubStartValue = m_value;
        return true;
    }
    return TextField::onPointerDown(ctx, event);
}

void NumberField::onPointerMove(Context& ctx, const PointerEvent& event)
{
    if (!m_scrubbing) {
        TextField::onPointerMove(ctx, event);
        return;
    }
    const float fine = event.modifiers.shift ? 0.1f : 1.0f;
    const float steps = std::round((event.position.x - m_scrubStartX) / kScrubPointsPerStep);
    emit(ctx, m_scrubStartValue + steps * m_step * fine, false);
}

bool NumberField::onPointerUp(Context& ctx, const PointerEvent& event)
{
    if (m_scrubbing) {
        m_scrubbing = false;
        if (m_onEditEnd) {
            m_onEditEnd();
        }
        return true;
    }
    return TextField::onPointerUp(ctx, event);
}

bool NumberField::onKeyDown(Context& ctx, const KeyEvent& event)
{
    if (event.key == Key::Up || event.key == Key::Down) {
        const float base = parse(text()).value_or(m_value);
        const float delta = m_step * (event.modifiers.shift ? 10.0f : 1.0f) * (event.key == Key::Up ? 1.0f : -1.0f);
        const float next = clamp(base + delta);
        setText(format(next, m_integer));
        state().selectAll();
        emit(ctx, next, true);
        return true;
    }
    return TextField::onKeyDown(ctx, event);
}

CursorShape NumberField::cursor(Vec2 point) const
{
    if (m_scrubbing || (scrubArea().contains(point) && !isEditing())) {
        return CursorShape::ResizeHorizontal;
    }
    return TextField::cursor(point);
}

// ---------------------------------------------------------------------------
// ColorField
// ---------------------------------------------------------------------------
std::string ColorField::toHex(Color c)
{
    char text[10];
    if (c.a == 255) {
        std::snprintf(text, sizeof text, "#%02x%02x%02x", c.r, c.g, c.b);
    } else {
        std::snprintf(text, sizeof text, "#%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
    }
    return text;
}

std::optional<Color> ColorField::fromHex(std::string_view text)
{
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    if (!text.empty() && text.front() == '#') {
        text.remove_prefix(1);
    }
    if (text.size() != 6 && text.size() != 8) {
        return std::nullopt;
    }
    std::uint8_t ch[4] = {0, 0, 0, 255};
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int hi = hexDigit(text[i]);
        const int lo = hexDigit(text[i + 1]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        ch[i / 2] = static_cast<std::uint8_t>(hi * 16 + lo);
    }
    return Color{ch[0], ch[1], ch[2], ch[3]};
}

ColorField::ColorField()
{
    setStretch(1.0f);
    auto hex = std::make_unique<TextField>("");
    hex->setSelectAllOnFocus(true);
    hex->setOnCommit([this](const std::string& text) {
        if (const auto c = fromHex(text)) {
            setValue(*c);
            if (m_onValue) {
                m_onValue(*c);
            }
            if (m_onEditEnd) {
                m_onEditEnd();
            }
        } else {
            m_hex->setText(toHex(m_value));
        }
    });
    m_hex = &addChild(std::move(hex));
}

ColorField::~ColorField()
{
    *m_alive = false;
    if (m_picker != nullptr && context() != nullptr) {
        context()->closeOverlay(m_picker);
    }
}

void ColorField::setValue(Color value)
{
    m_value = value;
    if (!m_hex->isEditing()) {
        const std::string text = toHex(value);
        if (text != m_hex->text()) {
            m_hex->setText(text);
        }
    }
}

Rect ColorField::swatchRect() const
{
    const Rect b = bounds();
    return Rect{b.x, b.y, 34.0f, b.height};
}

Size ColorField::measure(Context& ctx, Size available)
{
    static_cast<void>(ctx);
    return applyFixedSize({std::min(available.width, 160.0f), kFieldHeight});
}

void ColorField::arrange(Context& ctx, Rect rect)
{
    setBounds(rect);
    const Rect swatch = swatchRect();
    m_hex->arrange(ctx, Rect{swatch.right() + 4.0f, rect.y, std::max(0.0f, rect.right() - swatch.right() - 4.0f),
                             rect.height});
}

void ColorField::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect swatch = swatchRect();
    const bool hot = isHovered() && swatch.contains(ctx.pointerPosition());
    const Rect inner = swatch.inset(2.0f);
    draw.fillRoundedRect(swatch, t.radius, hot || m_picker != nullptr ? t.controlHover : t.field);
    if (m_value.a < 255) {
        paintChecker(draw, inner);
    }
    draw.fillRect(inner, m_value);
    paintChildren(ctx, draw);
}

bool ColorField::onPointerDown(Context&, const PointerEvent& event)
{
    return event.button == MouseButton::Left && swatchRect().contains(event.position);
}

bool ColorField::onPointerUp(Context& ctx, const PointerEvent& event)
{
    if (swatchRect().contains(event.position) && m_picker == nullptr) {
        openPicker(ctx);
    }
    return true;
}

void ColorField::openPicker(Context& ctx)
{
    auto picker = std::make_unique<ColorPicker>(m_value);
    // The picker may outlive this field (e.g. the panel is rebuilt): it only
    // calls back while the field still exists.
    std::shared_ptr<bool> alive = m_alive;
    picker->setOnChange([this, alive](Color c) {
        if (*alive) {
            pickerChanged(c);
        }
    });
    m_picker = picker.get();
    const Rect swatch = swatchRect();
    ctx.openOverlay(std::move(picker), OverlayOptions{.position = {swatch.x, swatch.bottom() + 4.0f},
                                                      .onClosed = [this, alive] {
                                                          if (*alive) {
                                                              pickerClosed();
                                                          }
                                                      }});
}

void ColorField::pickerChanged(Color color)
{
    setValue(color);
    if (m_onValue) {
        m_onValue(color);
    }
}

void ColorField::pickerClosed()
{
    m_picker = nullptr;
    if (m_onEditEnd) {
        m_onEditEnd();
    }
}

// ---------------------------------------------------------------------------
// ColorPicker
// ---------------------------------------------------------------------------
namespace {
constexpr float kPickerPad = 10.0f;
constexpr float kSquareW = 200.0f;
constexpr float kSquareH = 140.0f;
constexpr float kBarH = 12.0f;
constexpr float kGap = 10.0f;
} // namespace

ColorPicker::ColorPicker(Color color)
{
    rgbToHsv(color, m_hue, m_saturation, m_brightness);
    m_alpha = static_cast<float>(color.a) / 255.0f;
    setFocusable(true);
}

Color ColorPicker::color() const
{
    return hsvToRgb(m_hue, m_saturation, m_brightness, toByte(m_alpha));
}

Size ColorPicker::measure(Context&, Size)
{
    return {kSquareW + 2.0f * kPickerPad, kPickerPad + kSquareH + kGap + kBarH + kGap + kBarH + kPickerPad};
}

Rect ColorPicker::squareRect() const
{
    const Rect b = bounds();
    return {b.x + kPickerPad, b.y + kPickerPad, kSquareW, kSquareH};
}

Rect ColorPicker::hueRect() const
{
    const Rect s = squareRect();
    return {s.x, s.bottom() + kGap, kSquareW, kBarH};
}

Rect ColorPicker::alphaRect() const
{
    const Rect h = hueRect();
    return {h.x, h.bottom() + kGap, kSquareW, kBarH};
}

void ColorPicker::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect b = bounds();
    draw.fillRoundedRect(Rect{b.x, b.y + 3.0f, b.width, b.height}, t.popupRadius, Color{0, 0, 0, 80});
    draw.fillRoundedRect(b, t.popupRadius, t.popup);
    draw.strokeRoundedRect(b, t.popupRadius, t.border);

    // Saturation (left to right) and brightness (top to bottom) for the current hue.
    const Rect sq = squareRect();
    const Color pure = hsvToRgb(m_hue, 1.0f, 1.0f);
    const Color white{255, 255, 255, 255};
    draw.fillRectGradient(sq, white, pure, pure, white);
    draw.fillRectGradient(sq, Color{0, 0, 0, 0}, Color{0, 0, 0, 0}, Color{0, 0, 0, 255}, Color{0, 0, 0, 255});
    const Vec2 marker{sq.x + m_saturation * sq.width, sq.y + (1.0f - m_brightness) * sq.height};
    draw.fillCircle(marker, 6.0f, Color{0, 0, 0, 120});
    draw.fillCircle(marker, 5.0f, white);
    draw.fillCircle(marker, 3.5f, color().fade(1.0f));

    // Hue bar: red → yellow → green → cyan → blue → magenta → red.
    const Rect hue = hueRect();
    for (int i = 0; i < 6; ++i) {
        const float x0 = hue.x + hue.width * static_cast<float>(i) / 6.0f;
        const float x1 = hue.x + hue.width * static_cast<float>(i + 1) / 6.0f;
        const Color c0 = hsvToRgb(static_cast<float>(i) / 6.0f, 1.0f, 1.0f);
        const Color c1 = hsvToRgb(static_cast<float>(i + 1) / 6.0f, 1.0f, 1.0f);
        draw.fillRectGradient(Rect{x0, hue.y, x1 - x0 + 0.5f, hue.height}, c0, c1, c1, c0);
    }
    const float hx = hue.x + m_hue * hue.width;
    draw.fillRoundedRect(Rect{hx - 3.0f, hue.y - 2.0f, 6.0f, hue.height + 4.0f}, 2.0f, white);

    // Opacity bar over a checkerboard.
    const Rect alpha = alphaRect();
    paintChecker(draw, alpha);
    const Color opaque = hsvToRgb(m_hue, m_saturation, m_brightness);
    const Color clear{opaque.r, opaque.g, opaque.b, 0};
    draw.fillRectGradient(alpha, clear, opaque, opaque, clear);
    const float ax = alpha.x + m_alpha * alpha.width;
    draw.fillRoundedRect(Rect{ax - 3.0f, alpha.y - 2.0f, 6.0f, alpha.height + 4.0f}, 2.0f, white);
}

void ColorPicker::dragTo(Vec2 p)
{
    switch (m_dragging) {
    case Part::Square: {
        const Rect sq = squareRect();
        m_saturation = std::clamp((p.x - sq.x) / sq.width, 0.0f, 1.0f);
        m_brightness = 1.0f - std::clamp((p.y - sq.y) / sq.height, 0.0f, 1.0f);
        break;
    }
    case Part::Hue: {
        const Rect h = hueRect();
        m_hue = std::clamp((p.x - h.x) / h.width, 0.0f, 0.9999f);
        break;
    }
    case Part::Alpha: {
        const Rect a = alphaRect();
        m_alpha = std::clamp((p.x - a.x) / a.width, 0.0f, 1.0f);
        break;
    }
    case Part::None: return;
    }
    if (m_onChange) {
        m_onChange(color());
    }
}

bool ColorPicker::onPointerDown(Context&, const PointerEvent& event)
{
    if (event.button != MouseButton::Left) {
        return true;
    }
    const Vec2 p = event.position;
    if (squareRect().inset(-4.0f).contains(p)) {
        m_dragging = Part::Square;
    } else if (hueRect().inset(-4.0f).contains(p)) {
        m_dragging = Part::Hue;
    } else if (alphaRect().inset(-4.0f).contains(p)) {
        m_dragging = Part::Alpha;
    }
    dragTo(p);
    return true;
}

void ColorPicker::onPointerMove(Context&, const PointerEvent& event)
{
    if (m_dragging != Part::None) {
        dragTo(event.position);
    }
}

bool ColorPicker::onPointerUp(Context&, const PointerEvent&)
{
    m_dragging = Part::None;
    return true;
}

bool ColorPicker::onKeyDown(Context& ctx, const KeyEvent& event)
{
    if (event.key == Key::Escape || event.key == Key::Enter) {
        ctx.closeOverlay(this);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ChoiceField
// ---------------------------------------------------------------------------
ChoiceField::ChoiceField(std::vector<std::string> options)
    : m_options(std::move(options))
{
    setFocusable(true);
    setStretch(1.0f);
}

Size ChoiceField::measure(Context& ctx, Size available)
{
    const Theme& t = ctx.theme();
    float widest = 0.0f;
    for (const std::string& o : m_options) {
        widest = std::max(widest, ctx.atlas().measure(t.font, t.fontSize, o));
    }
    return applyFixedSize({std::min(available.width, std::ceil(widest) + 36.0f), kFieldHeight});
}

void ChoiceField::paint(Context& ctx, DrawList& draw)
{
    const Theme& t = ctx.theme();
    const Rect b = bounds();
    const bool hot = isHovered() || hasPointerCapture();
    draw.fillRoundedRect(b, t.radius, hot ? t.fieldHover : t.field);
    if (hot) {
        draw.strokeRoundedRect(b, t.radius, t.border);
    }
    const std::string label =
        m_selected >= 0 && static_cast<std::size_t>(m_selected) < m_options.size() ? m_options[static_cast<std::size_t>(m_selected)] : "";
    draw.textEllipsized(t.font, t.fontSize, {b.x + 6.0f, textTop(ctx, t.fontSize, b)}, t.text, label,
                        std::max(0.0f, b.width - 30.0f));
    drawIcon(draw, Icon::ChevronDown, Rect{b.right() - 20.0f, b.center().y - 7.0f, 14.0f, 14.0f}, t.textDim);
    if (isFocused()) {
        paintFocusRing(ctx, draw, b, t.radius);
    }
}

void ChoiceField::open(Context& ctx)
{
    std::vector<MenuItem> items;
    for (std::size_t i = 0; i < m_options.size(); ++i) {
        MenuItem item;
        item.label = m_options[i];
        const int index = static_cast<int>(i);
        item.isChecked = [this, index] { return m_selected == index; };
        item.action = [this, index] {
            if (index != m_selected) {
                m_selected = index;
                if (m_onChoose) {
                    m_onChoose(index);
                }
            }
        };
        items.push_back(std::move(item));
    }
    auto popup = std::make_unique<MenuPopup>(std::move(items));
    const Rect b = bounds();
    ctx.openOverlay(std::move(popup), OverlayOptions{.position = {b.x, b.bottom() + 2.0f}});
}

bool ChoiceField::onPointerDown(Context&, const PointerEvent& event)
{
    return event.button == MouseButton::Left;
}

bool ChoiceField::onPointerUp(Context& ctx, const PointerEvent& event)
{
    if (bounds().contains(event.position)) {
        open(ctx);
    }
    return true;
}

bool ChoiceField::onKeyDown(Context& ctx, const KeyEvent& event)
{
    if (event.key == Key::Space || event.key == Key::Enter || event.key == Key::Down) {
        open(ctx);
        return true;
    }
    return false;
}

} // namespace rolo::ui
