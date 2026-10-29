#pragma once

// DrawList: everything Lantern UI wants drawn in one frame, as plain data.
//
// Widgets call fillRect(), text(), image() and so on; the result is a list of
// triangles (vertices + indices) grouped into commands that share a texture
// and a clipping rectangle. The host (lantern_runtime) hands this to the GPU.
// Nothing here knows about Axmol, OpenGL or Metal.
//
// Curved and diagonal shapes get a one-pixel soft edge (anti-aliasing) so they
// look smooth on every screen.

#include "lantern/ui/Font.hpp"
#include "lantern/ui/Geometry.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace lantern::ui {

struct Vertex {
    float x = 0.0f;  // points
    float y = 0.0f;
    float u = 0.0f;  // texture coordinates
    float v = 0.0f;
    std::uint32_t color = 0; // Color::packed()
};

struct DrawCommand {
    TextureId texture = kAtlasTexture;
    Rect clip;                     // in points; pixels outside are not drawn
    std::uint32_t indexOffset = 0; // first index in indices()
    std::uint32_t indexCount = 0;
};

class DrawList {
public:
    /// The atlas provides the white pixel for solid shapes and glyphs for text.
    explicit DrawList(FontAtlas& atlas);

    /// Starts a new frame covering `viewport` (points).
    void reset(Rect viewport);

    // --- Clipping -------------------------------------------------------
    /// Restricts drawing to `rect` (intersected with the current clip).
    void pushClip(Rect rect);
    void popClip();
    Rect currentClip() const;

    // --- Shapes ---------------------------------------------------------
    void fillRect(Rect rect, Color color);
    /// A rectangle whose colour blends between its four corners
    /// (top-left, top-right, bottom-right, bottom-left), e.g. for colour pickers.
    void fillRectGradient(Rect rect, Color topLeft, Color topRight, Color bottomRight, Color bottomLeft);
    void fillRoundedRect(Rect rect, float radius, Color color);
    void strokeRoundedRect(Rect rect, float radius, Color color, float thickness = 1.0f);
    void fillCircle(Vec2 center, float radius, Color color);
    /// Points must go clockwise on screen (y down).
    void fillConvexPolygon(std::span<const Vec2> points, Color color);
    void line(Vec2 from, Vec2 to, Color color, float thickness = 1.0f);
    void polyline(std::span<const Vec2> points, Color color, float thickness = 1.0f, bool closed = false);

    // --- Images and text --------------------------------------------------
    void image(TextureId texture, Rect rect, Vec2 uv0 = {0.0f, 0.0f}, Vec2 uv1 = {1.0f, 1.0f},
               Color tint = Color{255, 255, 255, 255});

    /// Draws one line of text with its top-left corner at `topLeft`.
    /// Returns the width drawn, in points.
    float text(FontId font, float size, Vec2 topLeft, Color color, std::string_view text);

    /// Like text(), but if the text is wider than `maxWidth` it is cut short
    /// and ends with an ellipsis (…). Returns the width drawn.
    float textEllipsized(FontId font, float size, Vec2 topLeft, Color color, std::string_view text, float maxWidth);

    // --- Results ----------------------------------------------------------
    const std::vector<Vertex>& vertices() const noexcept { return m_vertices; }
    const std::vector<std::uint32_t>& indices() const noexcept { return m_indices; }
    const std::vector<DrawCommand>& commands() const noexcept { return m_commands; }
    FontAtlas& atlas() noexcept { return m_atlas; }

private:
    void ensureCommand(TextureId texture);
    std::uint32_t addVertex(float x, float y, float u, float v, std::uint32_t color);
    void addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c);
    void roundedRectPath(Rect rect, float radius, std::vector<Vec2>& out) const;
    float aaWidth() const;

    FontAtlas& m_atlas;
    std::vector<Vertex> m_vertices;
    std::vector<std::uint32_t> m_indices;
    std::vector<DrawCommand> m_commands;
    std::vector<Rect> m_clipStack;
    std::vector<Vec2> m_scratch;
};

} // namespace lantern::ui
