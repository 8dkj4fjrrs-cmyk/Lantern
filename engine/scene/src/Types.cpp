#include "lantern/scene/Types.hpp"
#include "lantern/scene/EntityId.hpp"

#include <cmath>
#include <cstdio>

namespace lantern {

namespace {

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

// --- Color -------------------------------------------------------------------

std::string Color::toHex() const
{
    char text[10];
    std::snprintf(text, sizeof text, "#%02x%02x%02x%02x", r, g, b, a);
    return text;
}

std::optional<Color> Color::fromHex(std::string_view text)
{
    if (!text.empty() && text.front() == '#') {
        text.remove_prefix(1);
    }
    if (text.size() != 6 && text.size() != 8) {
        return std::nullopt;
    }
    std::uint8_t channels[4] = {0, 0, 0, 255};
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int hi = hexDigit(text[i]);
        const int lo = hexDigit(text[i + 1]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        channels[i / 2] = static_cast<std::uint8_t>(hi * 16 + lo);
    }
    return Color{channels[0], channels[1], channels[2], channels[3]};
}

// --- Affine2 -------------------------------------------------------------------

Affine2 Affine2::fromParts(Vec2 position, float rotationDegrees, Vec2 scale)
{
    // Clockwise rotation with y pointing up: angle -θ in maths convention.
    const float radians = -rotationDegrees * 3.14159265358979f / 180.0f;
    const float cs = std::cos(radians);
    const float sn = std::sin(radians);
    Affine2 t;
    t.a = cs * scale.x;
    t.b = sn * scale.x;
    t.c = -sn * scale.y;
    t.d = cs * scale.y;
    t.tx = position.x;
    t.ty = position.y;
    return t;
}

Affine2 Affine2::operator*(const Affine2& m) const
{
    Affine2 r;
    r.a = a * m.a + c * m.b;
    r.b = b * m.a + d * m.b;
    r.c = a * m.c + c * m.d;
    r.d = b * m.c + d * m.d;
    r.tx = a * m.tx + c * m.ty + tx;
    r.ty = b * m.tx + d * m.ty + ty;
    return r;
}

std::optional<Affine2> Affine2::inverse() const
{
    const float det = a * d - b * c;
    if (std::abs(det) < 1e-12f) {
        return std::nullopt;
    }
    Affine2 r;
    r.a = d / det;
    r.b = -b / det;
    r.c = -c / det;
    r.d = a / det;
    r.tx = -(r.a * tx + r.c * ty);
    r.ty = -(r.b * tx + r.d * ty);
    return r;
}

// --- EntityId --------------------------------------------------------------------

std::string EntityId::toString() const
{
    if (isNull()) {
        return {};
    }
    char text[17];
    std::snprintf(text, sizeof text, "%016llx", static_cast<unsigned long long>(m_value));
    return text;
}

std::optional<EntityId> EntityId::parse(std::string_view text)
{
    if (text.size() != 16) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (char c : text) {
        const int digit = hexDigit(c);
        if (digit < 0) {
            return std::nullopt;
        }
        value = (value << 4) | static_cast<std::uint64_t>(digit);
    }
    if (value == 0) {
        return std::nullopt;
    }
    return EntityId(value);
}

EntityIdGenerator::EntityIdGenerator()
{
    std::random_device device;
    std::seed_seq seed{device(), device(), device(), device()};
    m_engine.seed(seed);
}

EntityIdGenerator::EntityIdGenerator(std::uint64_t seed)
    : m_engine(seed)
{
}

EntityId EntityIdGenerator::next()
{
    std::uint64_t value = 0;
    while (value == 0) {
        value = m_engine();
    }
    return EntityId(value);
}

} // namespace lantern
