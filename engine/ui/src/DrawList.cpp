#include "lantern/ui/DrawList.hpp"

#include "lantern/ui/Utf8.hpp"

#include <cmath>
#include <numbers>

namespace lantern::ui {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

Vec2 normalized(Vec2 v)
{
    const float len = std::sqrt(v.x * v.x + v.y * v.y);
    return len > 0.0f ? Vec2{v.x / len, v.y / len} : Vec2{};
}

// Number of segments for a quarter circle of this radius: smooth but cheap.
int arcSegments(float radius)
{
    return std::clamp(static_cast<int>(std::ceil(radius * 0.6f)), 2, 16);
}

} // namespace

DrawList::DrawList(FontAtlas& atlas)
    : m_atlas(atlas)
{
}

void DrawList::reset(Rect viewport)
{
    m_vertices.clear();
    m_indices.clear();
    m_commands.clear();
    m_clipStack.assign(1, viewport);
}

float DrawList::aaWidth() const
{
    return 1.0f / m_atlas.pixelScale(); // one physical pixel, in points
}

// --- Clipping -----------------------------------------------------------------

void DrawList::pushClip(Rect rect)
{
    m_clipStack.push_back(currentClip().intersect(rect));
}

void DrawList::popClip()
{
    if (m_clipStack.size() > 1) {
        m_clipStack.pop_back();
    }
}

Rect DrawList::currentClip() const
{
    return m_clipStack.empty() ? Rect{} : m_clipStack.back();
}

// --- Low level ------------------------------------------------------------------

void DrawList::ensureCommand(TextureId texture)
{
    const Rect clip = currentClip();
    if (m_commands.empty() || m_commands.back().texture != texture || !(m_commands.back().clip == clip)) {
        // Reuse an empty trailing command instead of leaving it behind.
        if (!m_commands.empty() && m_commands.back().indexCount == 0) {
            m_commands.back().texture = texture;
            m_commands.back().clip = clip;
        } else {
            m_commands.push_back(DrawCommand{texture, clip, static_cast<std::uint32_t>(m_indices.size()), 0});
        }
    }
}

std::uint32_t DrawList::addVertex(float x, float y, float u, float v, std::uint32_t color)
{
    m_vertices.push_back(Vertex{x, y, u, v, color});
    return static_cast<std::uint32_t>(m_vertices.size() - 1);
}

void DrawList::addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    m_indices.push_back(a);
    m_indices.push_back(b);
    m_indices.push_back(c);
    m_commands.back().indexCount += 3;
}

// --- Shapes -----------------------------------------------------------------------

void DrawList::fillRect(Rect rect, Color color)
{
    if (rect.isEmpty() || color.a == 0 || currentClip().intersect(rect).isEmpty()) {
        return;
    }
    ensureCommand(kAtlasTexture);
    const Vec2 uv = m_atlas.whiteUv();
    const std::uint32_t c = color.packed();
    const std::uint32_t a = addVertex(rect.left(), rect.top(), uv.x, uv.y, c);
    const std::uint32_t b = addVertex(rect.right(), rect.top(), uv.x, uv.y, c);
    const std::uint32_t d = addVertex(rect.right(), rect.bottom(), uv.x, uv.y, c);
    const std::uint32_t e = addVertex(rect.left(), rect.bottom(), uv.x, uv.y, c);
    addTriangle(a, b, d);
    addTriangle(a, d, e);
}

void DrawList::roundedRectPath(Rect rect, float radius, std::vector<Vec2>& out) const
{
    out.clear();
    const float r = std::min({radius, rect.width * 0.5f, rect.height * 0.5f});
    if (r <= 0.0f) {
        out = {rect.topLeft(), {rect.right(), rect.top()}, rect.bottomRight(), {rect.left(), rect.bottom()}};
        return;
    }
    const int segments = arcSegments(r);
    // Corner centres, visited clockwise on screen: top-right, bottom-right, bottom-left, top-left.
    const Vec2 centres[4] = {{rect.right() - r, rect.top() + r},
                             {rect.right() - r, rect.bottom() - r},
                             {rect.left() + r, rect.bottom() - r},
                             {rect.left() + r, rect.top() + r}};
    const float startAngles[4] = {-kPi * 0.5f, 0.0f, kPi * 0.5f, kPi};
    for (int corner = 0; corner < 4; ++corner) {
        for (int i = 0; i <= segments; ++i) {
            const float a = startAngles[corner] + (kPi * 0.5f) * static_cast<float>(i) / static_cast<float>(segments);
            out.push_back({centres[corner].x + std::cos(a) * r, centres[corner].y + std::sin(a) * r});
        }
    }
}

void DrawList::fillRoundedRect(Rect rect, float radius, Color color)
{
    if (radius <= 0.0f) {
        fillRect(rect, color);
        return;
    }
    if (rect.isEmpty() || color.a == 0 || currentClip().intersect(rect).isEmpty()) {
        return;
    }
    std::vector<Vec2> path;
    roundedRectPath(rect, radius, path);
    fillConvexPolygon(path, color);
}

void DrawList::strokeRoundedRect(Rect rect, float radius, Color color, float thickness)
{
    if (rect.isEmpty() || color.a == 0) {
        return;
    }
    // Stroke along the centre line, half a line width inside the rectangle.
    std::vector<Vec2> path;
    roundedRectPath(rect.inset(thickness * 0.5f), std::max(0.0f, radius - thickness * 0.5f), path);
    polyline(path, color, thickness, true);
}

void DrawList::fillCircle(Vec2 center, float radius, Color color)
{
    if (radius <= 0.0f || color.a == 0) {
        return;
    }
    const int segments = std::clamp(static_cast<int>(radius * 2.0f), 12, 64);
    std::vector<Vec2> path;
    path.reserve(static_cast<std::size_t>(segments));
    for (int i = 0; i < segments; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        path.push_back({center.x + std::cos(a) * radius, center.y + std::sin(a) * radius});
    }
    fillConvexPolygon(path, color);
}

// Anti-aliased convex fill: an opaque inner polygon plus a thin "fringe"
// band around it that fades to transparent over one pixel.
void DrawList::fillConvexPolygon(std::span<const Vec2> points, Color color)
{
    const std::size_t n = points.size();
    if (n < 3 || color.a == 0) {
        return;
    }
    ensureCommand(kAtlasTexture);
    const Vec2 uv = m_atlas.whiteUv();
    const std::uint32_t solid = color.packed();
    const std::uint32_t clear = color.fade(0.0f).packed();
    const float aa = aaWidth();

    // Outward normal of each edge i -> i+1.
    std::vector<Vec2> edgeNormals(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2 d = normalized(points[(i + 1) % n] - points[i]);
        edgeNormals[i] = {d.y, -d.x};
    }

    const auto first = static_cast<std::uint32_t>(m_vertices.size());
    for (std::size_t i = 0; i < n; ++i) {
        // Average the normals of the two edges meeting here (miter), limited so
        // sharp corners do not spike.
        const Vec2 n0 = edgeNormals[(i + n - 1) % n];
        const Vec2 n1 = edgeNormals[i];
        Vec2 dm{(n0.x + n1.x) * 0.5f, (n0.y + n1.y) * 0.5f};
        const float len2 = dm.x * dm.x + dm.y * dm.y;
        if (len2 > 0.000001f) {
            const float inv = std::min(1.0f / len2, 100.0f);
            dm = dm * inv;
        }
        const Vec2 inner = points[i] - dm * (aa * 0.5f);
        const Vec2 outer = points[i] + dm * (aa * 0.5f);
        addVertex(inner.x, inner.y, uv.x, uv.y, solid);  // even: inner
        addVertex(outer.x, outer.y, uv.x, uv.y, clear);  // odd: outer
    }

    for (std::size_t i = 2; i < n; ++i) { // inner fan
        addTriangle(first, first + static_cast<std::uint32_t>((i - 1) * 2), first + static_cast<std::uint32_t>(i * 2));
    }
    for (std::size_t i = 0; i < n; ++i) { // fringe band
        const std::size_t j = (i + 1) % n;
        const std::uint32_t ii = first + static_cast<std::uint32_t>(i * 2);
        const std::uint32_t jj = first + static_cast<std::uint32_t>(j * 2);
        addTriangle(ii, ii + 1, jj + 1);
        addTriangle(ii, jj + 1, jj);
    }
}

void DrawList::line(Vec2 from, Vec2 to, Color color, float thickness)
{
    if (color.a == 0) {
        return;
    }
    const Vec2 d = normalized(to - from);
    if (d.x == 0.0f && d.y == 0.0f) {
        return;
    }
    ensureCommand(kAtlasTexture);
    const Vec2 uv = m_atlas.whiteUv();
    const float aa = aaWidth();
    // Lines thinner than a pixel are drawn one pixel wide but fainter.
    const float core = std::max(thickness - aa, 0.0f) * 0.5f;
    const float alpha = std::min(1.0f, thickness / aa);
    const std::uint32_t solid = color.fade(alpha).packed();
    const std::uint32_t clear = color.fade(0.0f).packed();
    const Vec2 n{-d.y, d.x};

    const auto base = static_cast<std::uint32_t>(m_vertices.size());
    const float offsets[4] = {-(core + aa), -core, core, core + aa};
    const std::uint32_t colours[4] = {clear, solid, solid, clear};
    for (const Vec2 p : {from, to}) {
        for (int k = 0; k < 4; ++k) {
            const Vec2 q = p + n * offsets[k];
            addVertex(q.x, q.y, uv.x, uv.y, colours[k]);
        }
    }
    for (std::uint32_t k = 0; k < 3; ++k) {
        addTriangle(base + k, base + k + 1, base + 4 + k + 1);
        addTriangle(base + k, base + 4 + k + 1, base + 4 + k);
    }
}

void DrawList::polyline(std::span<const Vec2> points, Color color, float thickness, bool closed)
{
    if (points.size() < 2) {
        return;
    }
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        line(points[i], points[i + 1], color, thickness);
    }
    if (closed) {
        line(points.back(), points.front(), color, thickness);
    }
}

// --- Images and text -----------------------------------------------------------------

void DrawList::image(TextureId texture, Rect rect, Vec2 uv0, Vec2 uv1, Color tint)
{
    if (rect.isEmpty() || currentClip().intersect(rect).isEmpty()) {
        return;
    }
    ensureCommand(texture);
    const std::uint32_t c = tint.packed();
    const std::uint32_t a = addVertex(rect.left(), rect.top(), uv0.x, uv0.y, c);
    const std::uint32_t b = addVertex(rect.right(), rect.top(), uv1.x, uv0.y, c);
    const std::uint32_t d = addVertex(rect.right(), rect.bottom(), uv1.x, uv1.y, c);
    const std::uint32_t e = addVertex(rect.left(), rect.bottom(), uv0.x, uv1.y, c);
    addTriangle(a, b, d);
    addTriangle(a, d, e);
}

float DrawList::text(FontId font, float size, Vec2 topLeft, Color color, std::string_view text)
{
    if (text.empty() || color.a == 0) {
        return 0.0f;
    }
    const float scale = m_atlas.pixelScale();
    const FontMetrics fm = m_atlas.metrics(font, size);
    // Snap the baseline to a whole pixel so glyphs stay sharp.
    const float baseline = std::round((topLeft.y + fm.ascent) * scale) / scale;
    const std::uint32_t c = color.packed();
    const Rect clip = currentClip();

    float pen = topLeft.x;
    std::size_t i = 0;
    while (i < text.size()) {
        const GlyphInfo g = m_atlas.glyph(font, size, utf8::decode(text, i)); // copy: atlas may grow
        if (g.visible) {
            const float x = std::round((pen + g.box.x) * scale) / scale;
            const float y = baseline + g.box.y;
            const Rect quad{x, y, g.box.width, g.box.height};
            if (!clip.intersect(quad).isEmpty()) {
                ensureCommand(kAtlasTexture);
                const std::uint32_t a = addVertex(quad.left(), quad.top(), g.uv0.x, g.uv0.y, c);
                const std::uint32_t b = addVertex(quad.right(), quad.top(), g.uv1.x, g.uv0.y, c);
                const std::uint32_t d = addVertex(quad.right(), quad.bottom(), g.uv1.x, g.uv1.y, c);
                const std::uint32_t e = addVertex(quad.left(), quad.bottom(), g.uv0.x, g.uv1.y, c);
                addTriangle(a, b, d);
                addTriangle(a, d, e);
            }
        }
        pen += g.advance;
    }
    return pen - topLeft.x;
}

float DrawList::textEllipsized(FontId font, float size, Vec2 topLeft, Color color, std::string_view str,
                               float maxWidth)
{
    const float full = m_atlas.measure(font, size, str);
    if (full <= maxWidth) {
        return text(font, size, topLeft, color, str);
    }
    constexpr std::string_view kEllipsis = "\u2026"; // "…"
    const float ellipsis = m_atlas.measure(font, size, kEllipsis);
    if (maxWidth <= ellipsis) {
        return 0.0f;
    }
    // Keep as many characters as fit before the "...".
    float pen = 0.0f;
    std::size_t i = 0;
    std::size_t cut = 0;
    while (i < str.size()) {
        const std::size_t start = i;
        const float advance = m_atlas.glyph(font, size, utf8::decode(str, i)).advance;
        if (pen + advance + ellipsis > maxWidth) {
            cut = start;
            break;
        }
        pen += advance;
        cut = i;
    }
    const float drawn = text(font, size, topLeft, color, str.substr(0, cut));
    return drawn + text(font, size, {topLeft.x + drawn, topLeft.y}, color, kEllipsis);
}

} // namespace lantern::ui
