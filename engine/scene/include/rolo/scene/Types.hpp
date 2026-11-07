#pragma once

// Small value types used by scenes: 2D vectors, colours, and the 2D affine
// transform that places an entity in the world.
//
// Scene coordinates follow the convention of Cocos2d-x/Axmol, which many 2D
// developers know: the origin is the bottom-left corner of the game world, x
// grows to the right, y grows upwards, and positive rotation turns an object
// clockwise (in degrees).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rolo {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    friend bool operator==(const Vec2&, const Vec2&) = default;
    friend Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
    friend Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
    friend Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
};

/// An 8-bit-per-channel colour with alpha (opacity: 0 = invisible, 255 = solid).
struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;

    friend bool operator==(const Color&, const Color&) = default;

    /// "#RRGGBBAA" (alpha always written), the form used in scene files.
    std::string toHex() const;
    /// Reads "#RRGGBB" or "#RRGGBBAA" (the "#" is optional, case-insensitive).
    static std::optional<Color> fromHex(std::string_view text);
};

/// A 2D affine transform (rotation, scale and translation), as the matrix
///   | a  c  tx |
///   | b  d  ty |
struct Affine2 {
    float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, tx = 0.0f, ty = 0.0f;

    /// Scale, then rotate clockwise by `rotationDegrees`, then move to `position`.
    static Affine2 fromParts(Vec2 position, float rotationDegrees, Vec2 scale);

    Vec2 apply(Vec2 p) const { return {a * p.x + c * p.y + tx, b * p.x + d * p.y + ty}; }
    /// This transform applied after `inner` (i.e. parent * child).
    Affine2 operator*(const Affine2& inner) const;
    /// The reverse mapping; nothing if the transform squashes space flat (a zero scale).
    std::optional<Affine2> inverse() const;
};

} // namespace rolo
