#pragma once

// Lantern's built-in components.

#include "lantern/scene/Component.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

enum class TextAlign { Left, Center, Right };

/// What a Text shows while the game runs. "Text" is its own text, with
/// {variable} filled in; the others are filled by the game (see
/// docs/gameplay.md): the dialogue being shown, the prompt for what the
/// player can interact with, and the active quests.
enum class TextSource { Text, DialogueSpeaker, DialogueLine, DialogueChoices, InteractionPrompt, QuestLog };

/// Text drawn in the world (or, as a child of the camera, on screen). The
/// entity's position is the top of the text: its left end, middle or right
/// end, by `align`; lines go downwards. Long text wraps at `width` points
/// (0: no wrapping).
struct Text : ComponentBase<Text> {
    std::string text = "Text";
    TextSource source = TextSource::Text;
    AssetId font;                       // a font asset; none: Lantern's built-in font (Inter)
    float size = 16.0f;                 // in points
    Color color{255, 255, 255, 255};
    TextAlign align = TextAlign::Left;
    float width = 0.0f;

    static const ComponentType& componentType();
};

/// A box used for collision, centred on the entity (moved by `offset`), in
/// the entity's own coordinates. Gameplay code asks about colliders through
/// lantern::Game: solid colliders block movement (Game::move), and every
/// collider is found by overlap queries (e.g. a door or a pickup area).
/// Rotated objects use the box that encloses their rotated collider.
struct Collider : ComponentBase<Collider> {
    Vec2 size{32.0f, 32.0f};
    Vec2 offset;
    bool solid = true;

    static const ComponentType& componentType();
};

/// A cell of a tilemap: column x (to the right) and row y (upwards), from
/// the cell whose bottom-left corner is at the entity's position.
struct TileCell {
    int x = 0;
    int y = 0;
    friend bool operator==(const TileCell&, const TileCell&) = default;
    friend auto operator<=>(const TileCell&, const TileCell&) = default;
};

struct TileCellHash {
    std::size_t operator()(TileCell c) const noexcept
    {
        return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) << 32)
                                          | static_cast<std::uint32_t>(c.y));
    }
};

/// One layer of tiles. Layers are drawn in order (the last on top).
struct TileLayer {
    std::string name = "Layer";
    bool visible = true;
    std::unordered_map<TileCell, int, TileCellHash> tiles;   // cell -> tile number in the tileset

    friend bool operator==(const TileLayer&, const TileLayer&) = default;
};

/// A grid of tiles from a tileset, in layers, plus painted collision.
/// Tiles are drawn `cellSize` points large; cell (0, 0) has its bottom-left
/// corner at the entity's position. Whether a cell blocks movement: it is
/// painted solid on the collision layer, or a visible layer has a tile the
/// tileset marks solid (see lantern::isTileSolid in lantern_assets).
struct Tilemap : ComponentBase<Tilemap> {
    AssetId tileset;
    Vec2 cellSize{16.0f, 16.0f};
    std::vector<TileLayer> layers{TileLayer{"Ground", true, {}}};
    std::unordered_set<TileCell, TileCellHash> solid;   // painted collision

    /// The tile at a cell of a layer, or -1 for none.
    int tileAt(std::size_t layer, TileCell cell) const;
    /// Puts (or with -1 removes) a tile. False if nothing changed.
    bool setTile(std::size_t layer, TileCell cell, int tile);
    bool setSolid(TileCell cell, bool solid);
    /// The smallest rectangle of cells holding every tile and solid cell.
    struct CellRange {
        TileCell min;
        TileCell max;
    };
    std::optional<CellRange> usedCells() const;
    /// The cell containing a point in the entity's own coordinates.
    TileCell cellAt(Vec2 local) const;

    std::string saveData() const override;
    std::string loadData(std::string_view json) override;

    static const ComponentType& componentType();
};

/// The area an entity's visible content covers, in its own (unrotated,
/// unscaled) coordinates. Nothing for entities that draw nothing (e.g. an
/// empty object). Used for picking, outlines and tools.
struct LocalBounds {
    Vec2 center;   // (0, 0) for shapes and sprites, which are centred on the entity
    Vec2 size;
    bool ellipse = false;
};
std::optional<LocalBounds> localBounds(const Entity& entity);

} // namespace lantern
