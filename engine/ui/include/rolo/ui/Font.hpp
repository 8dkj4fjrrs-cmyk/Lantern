#pragma once

// Fonts and the glyph atlas.
//
// A font file (TrueType/OpenType) describes letters as outlines. To draw text
// quickly, each letter ("glyph") is rasterised once, at the size needed, into
// one shared image — the atlas — and text is then drawn as small textured
// rectangles cut from it. The host uploads the atlas to the GPU whenever its
// version() changes.
//
// Sizes are in points. Glyphs are rasterised at size × pixelScale so text is
// crisp on Retina screens.

#include "rolo/ui/Geometry.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace rolo::ui {

using FontId = std::uint16_t;
using TextureId = std::uint32_t;

/// The atlas is always texture 1; hosts number their own images from 2.
inline constexpr TextureId kAtlasTexture = 1;

struct FontMetrics {
    float ascent = 0.0f;      // points above the baseline
    float descent = 0.0f;     // points below the baseline (positive)
    float lineHeight = 0.0f;  // distance between baselines of consecutive lines
};

struct GlyphInfo {
    float advance = 0.0f;  // how far the pen moves, in points
    bool visible = false;  // false for spaces and unknown glyphs with no shape
    Rect box;              // quad relative to (pen x, baseline), in points
    Vec2 uv0;              // atlas coordinates (0..1)
    Vec2 uv1;
};

class FontAtlas {
public:
    explicit FontAtlas(int width = 1024, int height = 1024);
    ~FontAtlas();

    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    /// Registers a font. The bytes are not copied: they must stay alive as
    /// long as the atlas (embedded fonts always do). Returns nothing if the
    /// data is not a usable font.
    std::optional<FontId> addFont(std::span<const unsigned char> data);

    /// Physical pixels per point (2 on Retina). Changing it re-rasterises
    /// glyphs at the new density as they are next used.
    void setPixelScale(float scale);
    float pixelScale() const noexcept { return m_pixelScale; }

    FontMetrics metrics(FontId font, float size) const;

    /// Rasterises on first use. The reference stays valid until the next call
    /// that adds a glyph.
    const GlyphInfo& glyph(FontId font, float size, char32_t codepoint);

    /// Width of a single line of text, in points.
    float measure(FontId font, float size, std::string_view text);

    /// Pen x position before each character and after the last one
    /// (size = number of characters + 1). Used for caret placement.
    std::vector<float> caretPositions(FontId font, float size, std::string_view text);

    /// Byte index in `text` of the caret position closest to `x`.
    std::size_t hitTest(FontId font, float size, std::string_view text, float x);

    // --- Texture for the host -------------------------------------------
    int width() const noexcept { return m_width; }
    int height() const noexcept { return m_height; }
    const std::vector<std::uint8_t>& pixels() const noexcept { return m_pixels; } // RGBA8
    /// Increases whenever pixels() changes.
    std::uint64_t version() const noexcept { return m_version; }
    /// True if the atlas was resized or cleared since the last call: texture
    /// coordinates already handed out are stale, so the frame being built
    /// must be drawn again (Context::render() does this automatically).
    bool consumeResetFlag();
    /// Atlas coordinate of a fully white pixel (used to draw solid shapes).
    Vec2 whiteUv() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    int m_width;
    int m_height;
    std::vector<std::uint8_t> m_pixels;
    std::uint64_t m_version = 1;
    float m_pixelScale = 1.0f;
    bool m_wasReset = false;
};

} // namespace rolo::ui
