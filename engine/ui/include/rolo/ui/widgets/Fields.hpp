#pragma once

// Editable value fields for property inspectors.
//
//   NumberField  a number you can type, nudge with ↑/↓ (Shift: ×10), or
//                "scrub" by dragging its axis label (e.g. the "X") sideways
//   ColorField   a colour swatch plus its hex code; clicking the swatch opens
//                a colour picker (saturation/brightness square, hue and
//                opacity bars)
//   ChoiceField  one option from a list, chosen from a drop-down menu
//
// Each reports changes through two callbacks: onValue for every change
// (several per second while dragging) and onEditEnd once the gesture is over,
// so an undo system can merge a drag into one step.

#include "rolo/ui/widgets/Menu.hpp"
#include "rolo/ui/widgets/TextField.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rolo::ui {

class NumberField : public TextField {
public:
    NumberField();

    float value() const noexcept { return m_value; }
    /// Shows `value` (unless the user is typing in the field).
    void setValue(float value);
    void setStep(float step) { m_step = step; }
    void setRange(std::optional<float> min, std::optional<float> max);
    /// Whole numbers only.
    void setInteger(bool integer) { m_integer = integer; }
    /// A short label at the right end ("X", "°"); dragging it changes the value.
    void setSuffix(std::string suffix);

    void setOnValue(std::function<void(float)> onValue) { m_onValue = std::move(onValue); }
    void setOnEditEnd(std::function<void()> onEditEnd) { m_onEditEnd = std::move(onEditEnd); }

    /// Text for a value: up to 3 decimals, no trailing zeros ("1.5", "640").
    static std::string format(float value, bool integer);
    /// Reads typed text ("1,5" is accepted as 1.5); nothing if it is not a number.
    static std::optional<float> parse(std::string_view text);

    void paint(Context& ctx, DrawList& draw) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    void onPointerMove(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;
    bool onKeyDown(Context& ctx, const KeyEvent& event) override;
    CursorShape cursor(Vec2 point) const override;

    /// The drag-to-change area (the suffix label).
    Rect scrubArea() const;

private:
    float clamp(float value) const;
    void emit(Context& ctx, float value, bool final);

    float m_value = 0.0f;
    float m_step = 1.0f;
    std::optional<float> m_min;
    std::optional<float> m_max;
    bool m_integer = false;
    std::string m_suffix;
    std::function<void(float)> m_onValue;
    std::function<void()> m_onEditEnd;

    bool m_scrubbing = false;
    float m_scrubStartX = 0.0f;
    float m_scrubStartValue = 0.0f;
};

class ColorField : public Widget {
public:
    ColorField();
    ~ColorField() override;

    Color value() const noexcept { return m_value; }
    void setValue(Color value);

    void setOnValue(std::function<void(Color)> onValue) { m_onValue = std::move(onValue); }
    void setOnEditEnd(std::function<void()> onEditEnd) { m_onEditEnd = std::move(onEditEnd); }

    /// "#rrggbbaa" — alpha omitted when fully opaque ("#rrggbb").
    static std::string toHex(Color color);
    static std::optional<Color> fromHex(std::string_view text);

    /// Opens the colour picker under the swatch.
    void openPicker(Context& ctx);
    Rect swatchRect() const;

    Size measure(Context& ctx, Size available) override;
    void arrange(Context& ctx, Rect rect) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;

private:
    friend class ColorPicker;
    void pickerChanged(Color color);
    void pickerClosed();

    Color m_value;
    TextField* m_hex = nullptr;
    Widget* m_picker = nullptr;   // the open picker (owned by the context)
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true); // seen by the picker's callbacks
    std::function<void(Color)> m_onValue;
    std::function<void()> m_onEditEnd;
};

/// The colour picker popup used by ColorField.
class ColorPicker : public Widget {
public:
    explicit ColorPicker(Color color);

    Color color() const;
    void setOnChange(std::function<void(Color)> onChange) { m_onChange = std::move(onChange); }

    Size measure(Context& ctx, Size available) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    void onPointerMove(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;
    bool onKeyDown(Context& ctx, const KeyEvent& event) override;

    Rect squareRect() const;
    Rect hueRect() const;
    Rect alphaRect() const;

private:
    enum class Part { None, Square, Hue, Alpha };
    void dragTo(Vec2 point);

    float m_hue = 0.0f;         // 0..1
    float m_saturation = 0.0f;  // 0..1
    float m_brightness = 1.0f;  // 0..1
    float m_alpha = 1.0f;       // 0..1
    Part m_dragging = Part::None;
    std::function<void(Color)> m_onChange;
};

/// HSV <-> RGB helpers (all 0..1).
void rgbToHsv(Color c, float& h, float& s, float& v);
Color hsvToRgb(float h, float s, float v, std::uint8_t alpha = 255);

class ChoiceField : public Widget {
public:
    explicit ChoiceField(std::vector<std::string> options = {});

    void setOptions(std::vector<std::string> options) { m_options = std::move(options); }
    int selected() const noexcept { return m_selected; }
    void setSelected(int index) { m_selected = index; }
    void setOnChoose(std::function<void(int)> onChoose) { m_onChoose = std::move(onChoose); }

    void open(Context& ctx);

    Size measure(Context& ctx, Size available) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;
    bool onKeyDown(Context& ctx, const KeyEvent& event) override;

private:
    std::vector<std::string> m_options;
    int m_selected = 0;
    std::function<void(int)> m_onChoose;
};

} // namespace rolo::ui
