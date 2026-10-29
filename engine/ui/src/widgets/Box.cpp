#include "lantern/ui/widgets/Box.hpp"

#include "lantern/ui/Context.hpp"

#include <algorithm>
#include <cmath>

namespace lantern::ui {

namespace {
// Read the main / cross component of a size for the box's axis.
float mainOf(Axis axis, Size s)
{
    return axis == Axis::Horizontal ? s.width : s.height;
}
float crossOf(Axis axis, Size s)
{
    return axis == Axis::Horizontal ? s.height : s.width;
}
Size makeSize(Axis axis, float main, float cross)
{
    return axis == Axis::Horizontal ? Size{main, cross} : Size{cross, main};
}
} // namespace

Box::Box(Axis axis, float spacing, Insets padding)
    : m_axis(axis)
    , m_spacing(spacing)
    , m_padding(padding)
{
}

Size Box::measure(Context& ctx, Size available)
{
    const Size inner{std::max(0.0f, available.width - m_padding.horizontal()),
                     std::max(0.0f, available.height - m_padding.vertical())};
    float main = 0.0f;
    float cross = 0.0f;
    int visible = 0;
    for (auto& child : children()) {
        if (!child->isVisible()) {
            continue;
        }
        const Size s = child->measure(ctx, inner);
        main += mainOf(m_axis, s);
        cross = std::max(cross, crossOf(m_axis, s));
        ++visible;
    }
    if (visible > 1) {
        main += m_spacing * static_cast<float>(visible - 1);
    }
    Size result = makeSize(m_axis, main, cross);
    result.width += m_padding.horizontal();
    result.height += m_padding.vertical();
    return applyFixedSize(result);
}

void Box::arrange(Context& ctx, Rect rect)
{
    setBounds(rect);
    const Rect inner = rect.inset(m_padding);
    const float available = mainOf(m_axis, inner.size());
    const float crossAvailable = crossOf(m_axis, inner.size());

    // First pass: natural sizes and total stretch.
    std::vector<Size> sizes;
    sizes.reserve(children().size());
    float used = 0.0f;
    float totalStretch = 0.0f;
    int visible = 0;
    for (auto& child : children()) {
        if (!child->isVisible()) {
            sizes.push_back({});
            continue;
        }
        const Size s = child->measure(ctx, inner.size());
        sizes.push_back(s);
        ++visible;
        if (child->stretch() > 0.0f) {
            totalStretch += child->stretch();
        } else {
            used += mainOf(m_axis, s);
        }
    }
    if (visible > 1) {
        used += m_spacing * static_cast<float>(visible - 1);
    }
    const float leftover = std::max(0.0f, available - used);

    // Second pass: place children.
    float cursor = m_axis == Axis::Horizontal ? inner.x : inner.y;
    for (std::size_t i = 0; i < children().size(); ++i) {
        Widget& child = *children()[i];
        if (!child.isVisible()) {
            continue;
        }
        float main = child.stretch() > 0.0f && totalStretch > 0.0f ? leftover * child.stretch() / totalStretch
                                                                   : mainOf(m_axis, sizes[i]);
        main = std::round(main);
        float cross = crossOf(m_axis, sizes[i]);
        float crossPos = 0.0f;
        switch (m_crossAlign) {
        case CrossAlign::Stretch: cross = crossAvailable; break;
        case CrossAlign::Start: break;
        case CrossAlign::Center: crossPos = std::round((crossAvailable - cross) * 0.5f); break;
        case CrossAlign::End: crossPos = crossAvailable - cross; break;
        }
        cross = std::min(cross, crossAvailable);
        const Rect childRect = m_axis == Axis::Horizontal ? Rect{cursor, inner.y + crossPos, main, cross}
                                                          : Rect{inner.x + crossPos, cursor, cross, main};
        child.arrange(ctx, childRect);
        cursor += main + m_spacing;
    }
}

void Box::paint(Context& ctx, DrawList& draw)
{
    if (m_background) {
        draw.fillRect(bounds(), *m_background);
    }
    paintChildren(ctx, draw);
}

// ---------------------------------------------------------------------------
// Flow
// ---------------------------------------------------------------------------
Flow::Flow(float spacing, float lineSpacing, Insets padding)
    : m_spacing(spacing)
    , m_lineSpacing(lineSpacing)
    , m_padding(padding)
{
}

std::vector<Flow::Line> Flow::breakLines(Context& ctx, float width, std::vector<Size>& sizes)
{
    sizes.assign(children().size(), Size{});
    std::vector<Line> lines;
    Line line;
    float cursor = 0.0f;
    bool lineEmpty = true;
    for (std::size_t i = 0; i < children().size(); ++i) {
        Widget& child = *children()[i];
        if (!child.isVisible()) {
            continue;
        }
        const Size s = child.measure(ctx, {width, kUnbounded});
        sizes[i] = s;
        const float needed = (lineEmpty ? 0.0f : m_spacing) + s.width;
        if (!lineEmpty && cursor + needed > width + 0.5f) {
            line.end = i;
            lines.push_back(line);
            line = Line{i, i, 0.0f};
            cursor = 0.0f;
            lineEmpty = true;
        }
        cursor += (lineEmpty ? 0.0f : m_spacing) + s.width;
        line.height = std::max(line.height, s.height);
        lineEmpty = false;
    }
    if (!lineEmpty) {
        line.end = children().size();
        lines.push_back(line);
    }
    return lines;
}

Size Flow::measure(Context& ctx, Size available)
{
    const float width = std::max(0.0f, available.width - m_padding.horizontal());
    std::vector<Size> sizes;
    const std::vector<Line> lines = breakLines(ctx, width, sizes);
    float height = 0.0f;
    float widest = 0.0f;
    for (std::size_t l = 0; l < lines.size(); ++l) {
        height += lines[l].height + (l > 0 ? m_lineSpacing : 0.0f);
        float w = 0.0f;
        bool first = true;
        for (std::size_t i = lines[l].first; i < lines[l].end; ++i) {
            if (children()[i]->isVisible()) {
                w += (first ? 0.0f : m_spacing) + sizes[i].width;
                first = false;
            }
        }
        widest = std::max(widest, w);
    }
    return applyFixedSize({widest + m_padding.horizontal(), height + m_padding.vertical()});
}

void Flow::arrange(Context& ctx, Rect rect)
{
    setBounds(rect);
    const Rect inner = rect.inset(m_padding);
    std::vector<Size> sizes;
    const std::vector<Line> lines = breakLines(ctx, inner.width, sizes);

    float y = inner.y;
    for (const Line& line : lines) {
        float used = 0.0f;
        float totalStretch = 0.0f;
        int count = 0;
        for (std::size_t i = line.first; i < line.end; ++i) {
            const Widget& child = *children()[i];
            if (!child.isVisible()) {
                continue;
            }
            used += sizes[i].width;
            totalStretch += child.stretch();
            ++count;
        }
        if (count > 1) {
            used += m_spacing * static_cast<float>(count - 1);
        }
        const float leftover = std::max(0.0f, inner.width - used);

        float x = inner.x;
        for (std::size_t i = line.first; i < line.end; ++i) {
            Widget& child = *children()[i];
            if (!child.isVisible()) {
                continue;
            }
            float w = sizes[i].width;
            if (child.stretch() > 0.0f && totalStretch > 0.0f) {
                w += leftover * child.stretch() / totalStretch;
            }
            w = std::min(std::round(w), std::max(0.0f, inner.right() - x));
            const float h = std::min(sizes[i].height, line.height);
            child.arrange(ctx, Rect{x, y + std::round((line.height - h) * 0.5f), w, h});
            x += w + m_spacing;
        }
        y += line.height + m_lineSpacing;
    }
}

Size Separator::measure(Context&, Size)
{
    return applyFixedSize(m_axis == Axis::Horizontal ? Size{1.0f, 1.0f} : Size{1.0f, 1.0f});
}

void Separator::paint(Context& ctx, DrawList& draw)
{
    const Rect b = bounds();
    const Rect line = m_axis == Axis::Horizontal ? Rect{b.x, std::round(b.center().y), b.width, 1.0f}
                                                 : Rect{std::round(b.center().x), b.y, 1.0f, b.height};
    draw.fillRect(line, ctx.theme().border);
}

} // namespace lantern::ui
