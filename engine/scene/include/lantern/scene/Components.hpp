#pragma once

// Lantern's built-in components.

#include "lantern/scene/Component.hpp"

#include <optional>

namespace lantern {

class Entity;

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

/// An image (an image asset, chosen by ID) drawn centred on the entity,
/// stretched to `size`. Choosing an image in the editor sets the size to the
/// image's own size in pixels (1 pixel = 1 point).
struct Sprite : ComponentBase<Sprite> {
    AssetId image;
    Vec2 size{100.0f, 100.0f};
    Color color{255, 255, 255, 255};   // multiplies the image's colours; alpha = opacity
    bool flipX = false;
    bool flipY = false;

    static const ComponentType& componentType();
};

/// What the player sees: the game view is centred on this entity's position.
/// The first active Camera in the scene (in hierarchy order) is used; without
/// one, the view shows the world from (0, 0) to the game's resolution.
struct Camera : ComponentBase<Camera> {
    float zoom = 1.0f;   // 2 = everything twice as large

    static const ComponentType& componentType();
};

/// The area an entity's visible content covers, centred on the entity, in
/// its own (unrotated, unscaled) coordinates. Nothing for entities that draw
/// nothing (e.g. an empty object). Used for picking, outlines and tools.
struct LocalBounds {
    Vec2 size;
    bool ellipse = false;
};
std::optional<LocalBounds> localBounds(const Entity& entity);

} // namespace lantern
