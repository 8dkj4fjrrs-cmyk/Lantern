#pragma once

// Lantern's built-in components.

#include "lantern/scene/Component.hpp"

namespace lantern {

/// Where an entity is, relative to its parent: position (points), rotation
/// (degrees, clockwise) and scale (1 = normal size). Every entity has one.
struct Transform : ComponentBase<Transform> {
    Vec2 position;
    float rotation = 0.0f;
    Vec2 scale{1.0f, 1.0f};

    Affine2 local() const { return Affine2::fromParts(position, rotation, scale); }

    static const ComponentType& componentType();
};

enum class ShapeKind { Rectangle, Ellipse };

/// A filled or outlined rectangle or ellipse, centred on the entity.
/// (Images arrive with sprites in Milestone 4.)
struct Shape : ComponentBase<Shape> {
    ShapeKind kind = ShapeKind::Rectangle;
    Vec2 size{100.0f, 100.0f};
    Color color{255, 255, 255, 255};
    bool filled = true;
    float lineWidth = 2.0f;   // outline thickness when not filled

    static const ComponentType& componentType();
};

} // namespace lantern
