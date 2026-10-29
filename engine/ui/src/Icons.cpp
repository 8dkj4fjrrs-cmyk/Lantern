#include "lantern/ui/Icons.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace lantern::ui {

void drawIcon(DrawList& draw, Icon icon, Rect area, Color color)
{
    const Vec2 c = area.center();
    const float s = std::min(area.width, area.height); // icon box size
    const float t = std::max(1.25f, s * 0.11f);         // stroke width

    switch (icon) {
    case Icon::ChevronRight: {
        const float h = s * 0.22f;
        const std::array<Vec2, 3> pts{Vec2{c.x - h * 0.5f, c.y - h}, Vec2{c.x + h * 0.5f, c.y},
                                      Vec2{c.x - h * 0.5f, c.y + h}};
        draw.polyline(pts, color, t);
        break;
    }
    case Icon::ChevronDown: {
        const float h = s * 0.22f;
        const std::array<Vec2, 3> pts{Vec2{c.x - h, c.y - h * 0.5f}, Vec2{c.x, c.y + h * 0.5f},
                                      Vec2{c.x + h, c.y - h * 0.5f}};
        draw.polyline(pts, color, t);
        break;
    }
    case Icon::Close: {
        const float h = s * 0.2f;
        draw.line({c.x - h, c.y - h}, {c.x + h, c.y + h}, color, t);
        draw.line({c.x - h, c.y + h}, {c.x + h, c.y - h}, color, t);
        break;
    }
    case Icon::Check: {
        const float h = s * 0.24f;
        const std::array<Vec2, 3> pts{Vec2{c.x - h, c.y}, Vec2{c.x - h * 0.3f, c.y + h * 0.7f},
                                      Vec2{c.x + h, c.y - h * 0.65f}};
        draw.polyline(pts, color, t * 1.15f);
        break;
    }
    case Icon::Search: {
        const float r = s * 0.2f;
        const Vec2 lens{c.x - s * 0.06f, c.y - s * 0.06f};
        std::array<Vec2, 20> ring{};
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(ring.size());
            ring[i] = {lens.x + std::cos(a) * r, lens.y + std::sin(a) * r};
        }
        draw.polyline(ring, color, t, true);
        draw.line({lens.x + r * 0.72f, lens.y + r * 0.72f}, {c.x + s * 0.28f, c.y + s * 0.28f}, color, t);
        break;
    }
    case Icon::Dot:
        draw.fillCircle(c, s * 0.16f, color);
        break;
    }
}

} // namespace lantern::ui
