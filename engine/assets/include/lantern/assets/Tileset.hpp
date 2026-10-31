#pragma once

// Tileset: an image cut into equal tiles, plus what each tile means for
// collision. Saved as its own small asset file (.ltileset):
//
//     { "format": "lantern.tileset", "version": 1,
//       "image": "8c1f02e94b7d3a60",          <- the image asset, by ID
//       "tileSize": [16, 16], "margin": 0, "spacing": 0,
//       "solid": [3, 4, 17] }                 <- tiles that block movement
//
// Tiles are numbered from 0, left to right, then top to bottom.

#include "lantern/core/Engine.hpp" // Status
#include "lantern/scene/Components.hpp"
#include "lantern/scene/EntityId.hpp"
#include "lantern/scene/Types.hpp"

#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace lantern {

struct TileRect {
    int x = 0;       // pixels from the image's left edge
    int y = 0;       // pixels from the image's top edge
    int width = 0;
    int height = 0;
};

struct Tileset {
    AssetId image;
    int tileWidth = 16;
    int tileHeight = 16;
    int margin = 0;    // pixels around the outside of the image
    int spacing = 0;   // pixels between neighbouring tiles
    std::set<int> solid;   // tiles that block movement (collision)

    /// How many whole tiles fit across and down an image of this size.
    int columns(int imageWidth) const;
    int rows(int imageHeight) const;
    /// Where tile `index` is in the image.
    TileRect tileRect(int index, int imageWidth) const;
    bool isSolid(int index) const { return solid.count(index) > 0; }

    friend bool operator==(const Tileset&, const Tileset&) = default;
};

namespace TilesetFormat {
inline constexpr std::string_view kName = "lantern.tileset";
inline constexpr int kVersion = 1;
inline constexpr std::string_view kExtension = ".ltileset";
} // namespace TilesetFormat

/// Whether a tilemap cell blocks movement: it is painted solid on the
/// tilemap's collision layer, or a visible layer has a tile there that the
/// tileset marks solid. `tileset` may be null (then only painted collision counts).
bool isTileSolid(const Tilemap& map, const Tileset* tileset, TileCell cell);

/// Reads .ltileset text. Nothing (and `error` set) if it cannot be used.
std::optional<Tileset> parseTileset(std::string_view json, std::string* error = nullptr);
std::string serializeTileset(const Tileset& tileset);

} // namespace lantern
