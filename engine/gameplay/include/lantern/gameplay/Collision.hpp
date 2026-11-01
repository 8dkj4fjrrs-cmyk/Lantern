#pragma once

// Collision queries: what is where, and how far something can move.
//
// Two things take part in collision:
//   - Collider components (lantern::Collider): a box on an object. Solid ones
//     block movement; every collider is found by overlap queries.
//   - Tilemaps: a cell blocks movement if it is painted on the Collision
//     layer, or a visible layer has a tile its tileset marks solid.
//
// Everything works with axis-aligned boxes in world coordinates, which is
// what top-down and platform games need and keeps the rules easy to predict.
// A rotated collider counts as the box enclosing it; tilemaps are assumed
// not to be rotated (their position and scale are used).
//
// These are plain functions over a Scene, so they are easy to test and can
// be used outside a running game too. Game (Game.hpp) offers the same
// queries for the scene being played.

#include "lantern/assets/AssetDatabase.hpp"
#include "lantern/scene/Scene.hpp"

#include <optional>
#include <vector>

namespace lantern {

/// An axis-aligned box in world coordinates.
struct Box {
    Vec2 min;   // bottom-left
    Vec2 max;   // top-right

    static Box around(Vec2 center, Vec2 size)
    {
        return {{center.x - size.x * 0.5f, center.y - size.y * 0.5f}, {center.x + size.x * 0.5f, center.y + size.y * 0.5f}};
    }
    Vec2 center() const { return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f}; }
    Vec2 size() const { return max - min; }
    Box moved(Vec2 by) const { return {min + by, max + by}; }
    /// True if the boxes share some area (touching edges do not count, so
    /// something standing right next to a wall is not "inside" it).
    bool overlaps(const Box& other) const
    {
        return min.x < other.max.x && other.min.x < max.x && min.y < other.max.y && other.min.y < max.y;
    }
    bool contains(Vec2 p) const { return p.x >= min.x && p.x < max.x && p.y >= min.y && p.y < max.y; }
    friend bool operator==(const Box&, const Box&) = default;
};

/// The world box of an entity's Collider, if it has one and is active
/// (the entity and all its parents).
std::optional<Box> colliderBox(const Scene& scene, EntityId id);

/// Entities whose colliders overlap `box` (solid or not), in hierarchy order.
/// `ignore` and everything inside it are left out (e.g. the one asking).
std::vector<EntityId> overlappingColliders(const Scene& scene, const Box& box, EntityId ignore = {});

/// True if something solid (a solid collider or a solid tile) overlaps `box`.
/// `assets` finds tilesets (their solid tiles); without it only painted
/// collision counts.
bool isBlocked(const Scene& scene, const AssetDatabase* assets, const Box& box, EntityId ignore = {});

/// The world boxes of solid tilemap cells overlapping `area`.
std::vector<Box> solidTiles(const Scene& scene, const AssetDatabase* assets, const Box& area);

/// What happened when moving.
struct MoveResult {
    Vec2 moved;            // how far it actually went (world points)
    bool blockedX = false; // stopped by something left or right
    bool blockedY = false; // stopped by something below or above
    EntityId hit;          // the collider it ran into, if it was one (not a tile)
};

/// How far a box can move by `delta` before running into something solid.
/// It moves along x first, then y, so it slides along walls instead of
/// sticking to them. Anything it already overlaps does not stop it (so it
/// can always get out of a wall it was placed in).
MoveResult sweepBox(const Scene& scene, const AssetDatabase* assets, const Box& box, Vec2 delta, EntityId ignore = {});

} // namespace lantern
