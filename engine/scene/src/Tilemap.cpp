// The Tilemap component: tiles in layers, painted collision, and how they
// are saved.
//
// Saved form (the "data" of a lantern.Tilemap component), one line per row
// of tiles so that a painted change shows up as a small diff:
//
//   "data": {
//     "layers": [
//       { "name": "Ground", "visible": true,
//         "rows": [
//           [0, -2, 3, 3, 3, -1, 5],     <- row y = 0: cells x = -2, -1, 0, 1, 2
//           [1, 0, 7, 7]                 <- row y = 1: cells x = 0, 1
//         ] }
//     ],
//     "solid": [[0, 4, 1, 1, 0, 1]]      <- painted collision, same layout (1 = solid)
//   }
//
// Each row is [y, first x, then one value per cell]; -1 (or 0 for "solid")
// marks a gap inside the row.

#include "lantern/scene/Components.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <map>

namespace lantern {

namespace {

using Json = nlohmann::ordered_json;

// Cells grouped by row (y), each row sorted by x.
template <class Cells, class ValueOf>
Json encodeRows(const Cells& cells, ValueOf valueOf, int gap)
{
    std::map<int, std::map<int, int>> rows;
    for (const auto& item : cells) {
        const auto [cell, value] = valueOf(item);
        rows[cell.y][cell.x] = value;
    }
    Json out = Json::array();
    for (const auto& [y, row] : rows) {
        Json line = Json::array({y, row.begin()->first});
        int x = row.begin()->first;
        for (const auto& [cx, value] : row) {
            for (; x < cx; ++x) {
                line.push_back(gap);
            }
            line.push_back(value);
            x = cx + 1;
        }
        out.push_back(std::move(line));
    }
    return out;
}

// Reads rows written by encodeRows; calls put(cell, value) for each non-gap value.
template <class Put>
bool decodeRows(const Json& rows, int gap, Put put, std::string& problem)
{
    if (!rows.is_array()) {
        problem = "rows should be a list";
        return false;
    }
    std::size_t count = 0;
    for (const Json& line : rows) {
        if (!line.is_array() || line.size() < 2 || !line[0].is_number_integer() || !line[1].is_number_integer()) {
            problem = "a row should start with its y and its first x";
            continue;
        }
        const int y = line[0].get<int>();
        int x = line[1].get<int>();
        for (std::size_t i = 2; i < line.size(); ++i, ++x) {
            if (!line[i].is_number_integer()) {
                problem = "tile numbers should be whole numbers";
                continue;
            }
            const int value = line[i].get<int>();
            if (value != gap) {
                if (++count > 4'000'000) {
                    problem = "the map is too large (more than 4 million cells); the rest was skipped";
                    return false;
                }
                put(TileCell{x, y}, value);
            }
        }
    }
    return true;
}

} // namespace

int Tilemap::tileAt(std::size_t layer, TileCell cell) const
{
    if (layer >= layers.size()) {
        return -1;
    }
    const auto it = layers[layer].tiles.find(cell);
    return it == layers[layer].tiles.end() ? -1 : it->second;
}

bool Tilemap::setTile(std::size_t layer, TileCell cell, int tile)
{
    if (layer >= layers.size()) {
        return false;
    }
    auto& tiles = layers[layer].tiles;
    if (tile < 0) {
        return tiles.erase(cell) > 0;
    }
    const auto it = tiles.find(cell);
    if (it != tiles.end() && it->second == tile) {
        return false;
    }
    tiles[cell] = tile;
    return true;
}

bool Tilemap::setSolid(TileCell cell, bool isSolid)
{
    return isSolid ? solid.insert(cell).second : solid.erase(cell) > 0;
}

std::optional<Tilemap::CellRange> Tilemap::usedCells() const
{
    std::optional<CellRange> range;
    const auto add = [&](TileCell c) {
        if (!range) {
            range = CellRange{c, c};
            return;
        }
        range->min = {std::min(range->min.x, c.x), std::min(range->min.y, c.y)};
        range->max = {std::max(range->max.x, c.x), std::max(range->max.y, c.y)};
    };
    for (const TileLayer& layer : layers) {
        for (const auto& [cell, tile] : layer.tiles) {
            add(cell);
        }
    }
    for (const TileCell& cell : solid) {
        add(cell);
    }
    return range;
}

TileCell Tilemap::cellAt(Vec2 local) const
{
    const float w = cellSize.x > 0.0f ? cellSize.x : 1.0f;
    const float h = cellSize.y > 0.0f ? cellSize.y : 1.0f;
    return {static_cast<int>(std::floor(local.x / w)), static_cast<int>(std::floor(local.y / h))};
}

std::string Tilemap::saveData() const
{
    Json data;
    Json jsonLayers = Json::array();
    for (const TileLayer& layer : layers) {
        Json l;
        l["name"] = layer.name;
        l["visible"] = layer.visible;
        l["rows"] = encodeRows(
            layer.tiles, [](const auto& item) { return std::pair<TileCell, int>{item.first, item.second}; }, -1);
        jsonLayers.push_back(std::move(l));
    }
    data["layers"] = std::move(jsonLayers);
    data["solid"] = encodeRows(solid, [](TileCell c) { return std::pair<TileCell, int>{c, 1}; }, 0);
    return data.dump();
}

std::string Tilemap::loadData(std::string_view text)
{
    Json data;
    try {
        data = Json::parse(text.begin(), text.end());
    } catch (const nlohmann::json::parse_error&) {
        return "The tilemap's data is not valid JSON; the map is empty.";
    }
    if (!data.is_object()) {
        return "The tilemap's data should be an object; the map is empty.";
    }
    std::string problem;
    layers.clear();
    solid.clear();
    if (data.contains("layers") && data["layers"].is_array()) {
        for (const Json& l : data["layers"]) {
            TileLayer layer;
            if (l.contains("name") && l["name"].is_string()) {
                layer.name = l["name"].get<std::string>();
            }
            if (l.contains("visible") && l["visible"].is_boolean()) {
                layer.visible = l["visible"].get<bool>();
            }
            if (l.contains("rows")) {
                decodeRows(l["rows"], -1, [&](TileCell c, int tile) {
                    if (tile >= 0) {
                        layer.tiles[c] = tile;
                    } else {
                        problem = "tile numbers cannot be negative (except -1 for no tile)";
                    }
                }, problem);
            }
            layers.push_back(std::move(layer));
        }
    }
    if (layers.empty()) {
        layers.push_back(TileLayer{"Ground", true, {}});
    }
    if (data.contains("solid")) {
        decodeRows(data["solid"], 0, [&](TileCell c, int) { solid.insert(c); }, problem);
    }
    return problem.empty() ? std::string() : "Some tilemap data could not be read (" + problem + "); it was skipped.";
}

const ComponentType& Tilemap::componentType()
{
    static const ComponentType type =
        ComponentTypeBuilder<Tilemap>("lantern.Tilemap", "Tilemap")
            .description("A grid of tiles from a tileset, painted in layers, plus collision: cells that block "
                         "movement. Select the tilemap and use the Brush, Eraser and Fill tools in the Scene view; "
                         "pick tiles in the Tile Palette.")
            .property("tileset", "Tileset", &Tilemap::tileset,
                      {.help = "The tileset asset the tiles come from.", .assetType = "tileset"})
            .property("cellSize", "Cell Size", &Tilemap::cellSize,
                      {.min = 1.0f, .step = 1.0f, .help = "How large each tile is drawn, in points."})
            .build();
    return type;
}

} // namespace lantern
