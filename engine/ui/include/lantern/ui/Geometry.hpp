#pragma once

// Basic 2D types for Lantern UI.
//
// Units are *points*: on a Retina screen one point is two physical pixels.
// The origin is the window's top-left corner and y grows downwards (the usual
// convention for user interfaces; the game world uses y-up separately).

#include <algorithm>
#include <cstdint>

namespace lantern::ui {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr bool operator==(const Vec2&) const = default;
};

struct Size {
    float width = 0.0f;
    float height = 0.0f;
    constexpr bool operator==(const Size&) const = default;
};

struct Insets {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    static constexpr Insets all(float v) { return {v, v, v, v}; }
    static constexpr Insets symmetric(float horizontal, float vertical)
    {
        return {horizontal, vertical, horizontal, vertical};
    }
    constexpr float horizontal() const { return left + right; }
    constexpr float vertical() const { return top + bottom; }
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    static constexpr Rect fromCorners(Vec2 a, Vec2 b)
    {
        return {std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x) - std::min(a.x, b.x),
                std::max(a.y, b.y) - std::min(a.y, b.y)};
    }

    constexpr float left() const { return x; }
    constexpr float top() const { return y; }
    constexpr float right() const { return x + width; }
    constexpr float bottom() const { return y + height; }
    constexpr Vec2 topLeft() const { return {x, y}; }
    constexpr Vec2 bottomRight() const { return {x + width, y + height}; }
    constexpr Vec2 center() const { return {x + width * 0.5f, y + height * 0.5f}; }
    constexpr Size size() const { return {width, height}; }
    constexpr bool isEmpty() const { return width <= 0.0f || height <= 0.0f; }

    constexpr bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + width && p.y < y + height; }

    constexpr Rect inset(Insets i) const
    {
        return {x + i.left, y + i.top, std::max(0.0f, width - i.horizontal()), std::max(0.0f, height - i.vertical())};
    }
    constexpr Rect inset(float v) const { return inset(Insets::all(v)); }

    /// The overlapping part of two rectangles (empty if they do not overlap).
    constexpr Rect intersect(const Rect& o) const
    {
        const float l = std::max(x, o.x);
        const float t = std::max(y, o.y);
        const float r = std::min(right(), o.right());
        const float b = std::min(bottom(), o.bottom());
        return {l, t, std::max(0.0f, r - l), std::max(0.0f, b - t)};
    }

    constexpr bool operator==(const Rect&) const = default;
};

/// An sRGB colour with straight (not premultiplied) alpha.
struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    /// From 0xRRGGBB, e.g. Color::hex(0xF5A842).
    static constexpr Color hex(std::uint32_t rgb, std::uint8_t alpha = 255)
    {
        return {static_cast<std::uint8_t>((rgb >> 16) & 0xFF), static_cast<std::uint8_t>((rgb >> 8) & 0xFF),
                static_cast<std::uint8_t>(rgb & 0xFF), alpha};
    }

    /// Same colour with its alpha multiplied by `factor` (0..1).
    constexpr Color fade(float factor) const
    {
        const float f = std::clamp(factor, 0.0f, 1.0f);
        return {r, g, b, static_cast<std::uint8_t>(static_cast<float>(a) * f + 0.5f)};
    }

    /// Packed as the bytes R, G, B, A in memory (the vertex colour format).
    constexpr std::uint32_t packed() const
    {
        return static_cast<std::uint32_t>(r) | (static_cast<std::uint32_t>(g) << 8)
               | (static_cast<std::uint32_t>(b) << 16) | (static_cast<std::uint32_t>(a) << 24);
    }

    /// Linear blend from `from` (t = 0) to `to` (t = 1).
    static constexpr Color mix(Color from, Color to, float t)
    {
        const float k = std::clamp(t, 0.0f, 1.0f);
        auto ch = [k](std::uint8_t p, std::uint8_t q) {
            return static_cast<std::uint8_t>(static_cast<float>(p) + (static_cast<float>(q) - static_cast<float>(p)) * k
                                             + 0.5f);
        };
        return {ch(from.r, to.r), ch(from.g, to.g), ch(from.b, to.b), ch(from.a, to.a)};
    }

    constexpr bool operator==(const Color&) const = default;
};

} // namespace lantern::ui
