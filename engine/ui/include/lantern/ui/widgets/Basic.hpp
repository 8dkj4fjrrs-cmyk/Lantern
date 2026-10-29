#pragma once

// Basic widgets: Label, Button, Checkbox, ImageView.

#include "lantern/ui/Icons.hpp"
#include "lantern/ui/Widget.hpp"

#include <functional>
#include <optional>
#include <string>

namespace lantern::ui {

enum class TextAlign { Start, Center, End };

/// Text. Single line (cut short with "…" if too narrow) or word-wrapped.
class Label : public Widget {
public:
    enum class Style { Normal, Strong, Dim, Faint, Small };

    explicit Label(std::string text = {}, Style style = Style::Normal);

    void setText(std::string text) { m_text = std::move(text); }
    const std::string& text() const noexcept { return m_text; }
    void setStyle(Style style) { m_style = style; }
    void setAlign(TextAlign align) { m_align = align; }
    void setWrap(bool wrap) { m_wrap = wrap; }
    /// Overrides the style's colour.
    void setColor(std::optional<Color> color) { m_color = color; }

    Size measure(Context& ctx, Size available) override;
    void paint(Context& ctx, DrawList& draw) override;
    /// Labels let clicks through to whatever they sit on, unless they have a tooltip.
    Widget* hitTest(Vec2 point) override;

private:
    struct Resolved {
        FontId font;
        float size;
        Color color;
    };
    Resolved resolve(Context& ctx) const;
    std::vector<std::string> wrapLines(Context& ctx, float width) const;

    std::string m_text;
    Style m_style;
    TextAlign m_align = TextAlign::Start;
    bool m_wrap = false;
    std::optional<Color> m_color;
};

/// A clickable button. Activated by clicking, or Space/Enter when focused.
class Button : public Widget {
public:
    enum class Style {
        Normal,   // filled
        Primary,  // accent colour, for the main action
        Subtle,   // no fill until hovered (toolbars)
    };

    explicit Button(std::string text = {}, std::function<void()> onClick = {});

    void setText(std::string text) { m_text = std::move(text); }
    void setIcon(std::optional<Icon> icon) { m_icon = icon; }
    void setStyle(Style style) { m_style = style; }
    void setOnClick(std::function<void()> onClick) { m_onClick = std::move(onClick); }
    /// Shows the button as switched on (e.g. the active tool in a toolbar).
    void setToggled(bool toggled) { m_toggled = toggled; }
    bool isToggled() const noexcept { return m_toggled; }

    Size measure(Context& ctx, Size available) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;
    bool onKeyDown(Context& ctx, const KeyEvent& event) override;
    CursorShape cursor(Vec2) const override { return CursorShape::Arrow; }

private:
    void activate(Context& ctx);

    std::string m_text;
    std::optional<Icon> m_icon;
    Style m_style = Style::Normal;
    std::function<void()> m_onClick;
    bool m_toggled = false;
};

/// A box that can be ticked, with a label.
class Checkbox : public Widget {
public:
    explicit Checkbox(std::string text = {}, bool checked = false, std::function<void(bool)> onChange = {});

    bool isChecked() const noexcept { return m_checked; }
    void setChecked(bool checked) { m_checked = checked; }
    void setText(std::string text) { m_text = std::move(text); }
    /// Colour for the label text (e.g. to match a log level).
    void setTextColor(std::optional<Color> color) { m_textColor = color; }
    void setOnChange(std::function<void(bool)> onChange) { m_onChange = std::move(onChange); }

    Size measure(Context& ctx, Size available) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;
    bool onKeyDown(Context& ctx, const KeyEvent& event) override;

private:
    void toggle(Context& ctx);

    std::string m_text;
    bool m_checked;
    std::optional<Color> m_textColor;
    std::function<void(bool)> m_onChange;
};

/// Shows an image (e.g. the game world) scaled to fit without distortion.
class ImageView : public Widget {
public:
    ImageView(TextureId texture, Size imageSize);

    void setTexture(TextureId texture, Size imageSize);
    /// Texture coordinates of the image's top-left and bottom-right corners
    /// (swap the y values for images stored upside down).
    void setUv(Vec2 uv0, Vec2 uv1);
    void setCaption(std::string caption) { m_caption = std::move(caption); }
    void setBackground(Color color) { m_background = color; }

    /// Where the image was drawn last frame, and at what scale.
    Rect imageRect() const noexcept { return m_imageRect; }
    float imageScale() const noexcept { return m_scale; }

    void arrange(Context& ctx, Rect rect) override;
    void paint(Context& ctx, DrawList& draw) override;

private:
    TextureId m_texture;
    Size m_imageSize;
    Vec2 m_uv0{0.0f, 0.0f};
    Vec2 m_uv1{1.0f, 1.0f};
    std::string m_caption;
    std::optional<Color> m_background;
    Rect m_imageRect;
    float m_scale = 0.0f;
};

/// Draws a keyboard-focus ring around `rect` (shared by focusable widgets).
/// A single icon, e.g. beside a count in a status bar.
class IconView : public Widget {
public:
    explicit IconView(Icon icon, float size = 14.0f, std::optional<Color> color = {});
    void setColor(std::optional<Color> color) { m_color = color; }
    Size measure(Context& ctx, Size available) override;
    void paint(Context& ctx, DrawList& draw) override;
    Widget* hitTest(Vec2) override { return nullptr; }

private:
    Icon m_icon;
    float m_size;
    std::optional<Color> m_color;
};

void paintFocusRing(Context& ctx, DrawList& draw, Rect rect, float radius);

} // namespace lantern::ui
