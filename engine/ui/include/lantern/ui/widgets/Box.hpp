#pragma once

// Layout containers.
//
//   Box (Row or Column)  places children one after another, with spacing and
//                        padding; left-over space goes to children with stretch.
//   Spacer               an empty, stretchy gap.
//   Separator            a thin dividing line.

#include "lantern/ui/Widget.hpp"

#include <optional>

namespace lantern::ui {

enum class Axis { Horizontal, Vertical };

/// How children are placed across the box (e.g. vertically inside a Row).
enum class CrossAlign { Start, Center, End, Stretch };

class Box : public Widget {
public:
    explicit Box(Axis axis, float spacing = 0.0f, Insets padding = {});

    void setSpacing(float spacing) { m_spacing = spacing; }
    void setPadding(Insets padding) { m_padding = padding; }
    void setCrossAlign(CrossAlign align) { m_crossAlign = align; }
    /// Fills the box's area before drawing children.
    void setBackground(std::optional<Color> color) { m_background = color; }

    Size measure(Context& ctx, Size available) override;
    void arrange(Context& ctx, Rect rect) override;
    void paint(Context& ctx, DrawList& draw) override;

private:
    Axis m_axis;
    float m_spacing;
    Insets m_padding;
    CrossAlign m_crossAlign = CrossAlign::Stretch;
    std::optional<Color> m_background;
};

class Row : public Box {
public:
    explicit Row(float spacing = 0.0f, Insets padding = {}) : Box(Axis::Horizontal, spacing, padding) {}
};

class Column : public Box {
public:
    explicit Column(float spacing = 0.0f, Insets padding = {}) : Box(Axis::Vertical, spacing, padding) {}
};

/// Lays children out left to right like words in a paragraph: when the next
/// child does not fit, it starts a new line. Within a line, children with a
/// stretch share the leftover space (so a Spacer pushes the rest of its line
/// to the right edge). Children are centred vertically in their line.
/// Used for toolbars that must still work in a narrow panel.
class Flow : public Widget {
public:
    explicit Flow(float spacing = 0.0f, float lineSpacing = 0.0f, Insets padding = {});

    Size measure(Context& ctx, Size available) override;
    void arrange(Context& ctx, Rect rect) override;

private:
    struct Line {
        std::size_t first = 0;  // index into children()
        std::size_t end = 0;    // one past the last
        float height = 0.0f;
    };
    /// Splits the visible children into lines for a given inner width.
    std::vector<Line> breakLines(Context& ctx, float width, std::vector<Size>& sizes);

    float m_spacing;
    float m_lineSpacing;
    Insets m_padding;
};

class Spacer : public Widget {
public:
    explicit Spacer(float stretch = 1.0f) { setStretch(stretch); }
    Widget* hitTest(Vec2) override { return nullptr; }
};

class Separator : public Widget {
public:
    explicit Separator(Axis axis = Axis::Horizontal) : m_axis(axis) {}
    Size measure(Context& ctx, Size available) override;
    void paint(Context& ctx, DrawList& draw) override;

private:
    Axis m_axis;
};

} // namespace lantern::ui
