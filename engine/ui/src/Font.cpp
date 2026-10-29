#include "lantern/ui/Font.hpp"

#include "lantern/ui/Utf8.hpp"

#include <stb_truetype.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace lantern::ui {

namespace {

constexpr int kPadding = 1;          // empty pixels around each glyph (avoids bleeding)
constexpr int kMaxAtlasSize = 4096;  // never grow beyond this; clear instead
constexpr int kTabSpaces = 4;

struct GlyphKey {
    FontId font;
    std::uint16_t pixelSize;
    char32_t codepoint;
    bool operator==(const GlyphKey&) const = default;
};

struct GlyphKeyHash {
    std::size_t operator()(const GlyphKey& k) const noexcept
    {
        return (static_cast<std::size_t>(k.codepoint) * 1315423911u) ^ (static_cast<std::size_t>(k.pixelSize) << 21)
               ^ (static_cast<std::size_t>(k.font) << 45);
    }
};

} // namespace

struct FontAtlas::Impl {
    std::vector<stbtt_fontinfo> fonts;
    std::unordered_map<GlyphKey, GlyphInfo, GlyphKeyHash> glyphs;

    // Shelf packer: glyphs are placed left to right in rows ("shelves").
    int cursorX = kPadding;
    int cursorY = kPadding;
    int shelfHeight = 0;
};

FontAtlas::FontAtlas(int width, int height)
    : m_impl(std::make_unique<Impl>())
    , m_width(std::clamp(width, 64, kMaxAtlasSize))
    , m_height(std::clamp(height, 64, kMaxAtlasSize))
    , m_pixels(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height) * 4, 0)
{
    // A 2x2 white block in the top-left corner for solid shapes.
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            std::uint8_t* p = &m_pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width)
                                         + static_cast<std::size_t>(x)) * 4];
            p[0] = p[1] = p[2] = p[3] = 255;
        }
    }
    m_impl->cursorX = 2 + kPadding;
}

FontAtlas::~FontAtlas() = default;

Vec2 FontAtlas::whiteUv() const noexcept
{
    return {1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height)};
}

std::optional<FontId> FontAtlas::addFont(std::span<const unsigned char> data)
{
    if (data.empty()) {
        return std::nullopt;
    }
    stbtt_fontinfo info{};
    const int offset = stbtt_GetFontOffsetForIndex(data.data(), 0);
    if (offset < 0 || stbtt_InitFont(&info, data.data(), offset) == 0) {
        return std::nullopt;
    }
    m_impl->fonts.push_back(info);
    return static_cast<FontId>(m_impl->fonts.size() - 1);
}

void FontAtlas::setPixelScale(float scale)
{
    m_pixelScale = std::clamp(scale, 0.5f, 4.0f);
}

bool FontAtlas::consumeResetFlag()
{
    const bool was = m_wasReset;
    m_wasReset = false;
    return was;
}

FontMetrics FontAtlas::metrics(FontId font, float size) const
{
    if (font >= m_impl->fonts.size()) {
        return {};
    }
    const stbtt_fontinfo& info = m_impl->fonts[font];
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    const float scale = stbtt_ScaleForMappingEmToPixels(&info, size);
    return FontMetrics{static_cast<float>(ascent) * scale, static_cast<float>(-descent) * scale,
                       static_cast<float>(ascent - descent + lineGap) * scale};
}

const GlyphInfo& FontAtlas::glyph(FontId font, float size, char32_t codepoint)
{
    static const GlyphInfo kNone{};
    if (font >= m_impl->fonts.size()) {
        return kNone;
    }

    const float pixelSizeF = std::max(1.0f, std::round(size * m_pixelScale));
    const auto pixelSize = static_cast<std::uint16_t>(std::min(pixelSizeF, 512.0f));
    const GlyphKey key{font, pixelSize, codepoint};
    if (auto it = m_impl->glyphs.find(key); it != m_impl->glyphs.end()) {
        return it->second;
    }

    const stbtt_fontinfo& info = m_impl->fonts[font];
    const float scale = stbtt_ScaleForMappingEmToPixels(&info, static_cast<float>(pixelSize));
    const float pxToPt = size / static_cast<float>(pixelSize); // pixels → points at this size

    GlyphInfo g;
    const bool isTab = codepoint == '\t';
    const int glyphIndex = stbtt_FindGlyphIndex(&info, static_cast<int>(isTab ? ' ' : codepoint));
    int advance = 0;
    int leftBearing = 0;
    stbtt_GetGlyphHMetrics(&info, glyphIndex, &advance, &leftBearing);
    g.advance = static_cast<float>(advance) * scale * pxToPt * (isTab ? static_cast<float>(kTabSpaces) : 1.0f);

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetGlyphBitmapBox(&info, glyphIndex, scale, scale, &x0, &y0, &x1, &y1);
    const int w = x1 - x0;
    const int h = y1 - y0;

    if (!isTab && w > 0 && h > 0 && !stbtt_IsGlyphEmpty(&info, glyphIndex)) {
        Impl& im = *m_impl;
        // Next shelf if this row is full.
        if (im.cursorX + w + kPadding > m_width) {
            im.cursorX = kPadding;
            im.cursorY += im.shelfHeight + kPadding;
            im.shelfHeight = 0;
        }
        // Grow (or, at the maximum size, start over) if the atlas is full.
        if (im.cursorY + h + kPadding > m_height) {
            if (m_height < kMaxAtlasSize) {
                m_height = std::min(m_height * 2, kMaxAtlasSize);
                m_pixels.resize(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height) * 4, 0);
                // UVs of existing glyphs were computed for the old height.
                for (auto& [k, existing] : im.glyphs) {
                    (void)k;
                    existing.uv0.y *= 0.5f;
                    existing.uv1.y *= 0.5f;
                }
                m_wasReset = true; // anything already drawn this frame used the old UVs
            } else {
                im.glyphs.clear();
                std::fill(m_pixels.begin(), m_pixels.end(), std::uint8_t{0});
                for (int y = 0; y < 2; ++y) {
                    for (int x = 0; x < 2; ++x) {
                        std::uint8_t* p = &m_pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width)
                                                     + static_cast<std::size_t>(x)) * 4];
                        p[0] = p[1] = p[2] = p[3] = 255;
                    }
                }
                im.cursorX = 2 + kPadding;
                im.cursorY = kPadding;
                im.shelfHeight = 0;
                m_wasReset = true;
            }
            return glyph(font, size, codepoint); // retry with room
        }

        // Rasterise into a temporary 8-bit buffer, then copy as white + alpha.
        std::vector<unsigned char> bitmap(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
        stbtt_MakeGlyphBitmap(&info, bitmap.data(), w, h, w, scale, scale, glyphIndex);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const std::size_t dst = (static_cast<std::size_t>(im.cursorY + y) * static_cast<std::size_t>(m_width)
                                         + static_cast<std::size_t>(im.cursorX + x)) * 4;
                m_pixels[dst + 0] = 255;
                m_pixels[dst + 1] = 255;
                m_pixels[dst + 2] = 255;
                m_pixels[dst + 3] = bitmap[static_cast<std::size_t>(y) * static_cast<std::size_t>(w)
                                           + static_cast<std::size_t>(x)];
            }
        }

        g.visible = true;
        g.box = Rect{static_cast<float>(x0) * pxToPt, static_cast<float>(y0) * pxToPt, static_cast<float>(w) * pxToPt,
                     static_cast<float>(h) * pxToPt};
        g.uv0 = {static_cast<float>(im.cursorX) / static_cast<float>(m_width),
                 static_cast<float>(im.cursorY) / static_cast<float>(m_height)};
        g.uv1 = {static_cast<float>(im.cursorX + w) / static_cast<float>(m_width),
                 static_cast<float>(im.cursorY + h) / static_cast<float>(m_height)};

        im.cursorX += w + kPadding;
        im.shelfHeight = std::max(im.shelfHeight, h);
        ++m_version;
    }

    return m_impl->glyphs.emplace(key, g).first->second;
}

float FontAtlas::measure(FontId font, float size, std::string_view text)
{
    float width = 0.0f;
    std::size_t i = 0;
    while (i < text.size()) {
        width += glyph(font, size, utf8::decode(text, i)).advance;
    }
    return width;
}

std::vector<float> FontAtlas::caretPositions(FontId font, float size, std::string_view text)
{
    std::vector<float> positions{0.0f};
    float pen = 0.0f;
    std::size_t i = 0;
    while (i < text.size()) {
        pen += glyph(font, size, utf8::decode(text, i)).advance;
        positions.push_back(pen);
    }
    return positions;
}

std::size_t FontAtlas::hitTest(FontId font, float size, std::string_view text, float x)
{
    float pen = 0.0f;
    std::size_t i = 0;
    while (i < text.size()) {
        const std::size_t start = i;
        const float advance = glyph(font, size, utf8::decode(text, i)).advance;
        if (x < pen + advance * 0.5f) {
            return start;
        }
        pen += advance;
    }
    return text.size();
}

} // namespace lantern::ui
