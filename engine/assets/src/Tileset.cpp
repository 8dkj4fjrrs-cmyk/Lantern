#include "lantern/assets/Tileset.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace lantern {

namespace {
using Json = nlohmann::ordered_json;

void setError(std::string* error, std::string message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

std::optional<int> readInt(const Json& j, const char* key, int fallback, int min, int max)
{
    if (!j.contains(key)) {
        return fallback;
    }
    if (!j[key].is_number_integer()) {
        return std::nullopt;
    }
    const auto v = j[key].get<long long>();
    if (v < min || v > max) {
        return std::nullopt;
    }
    return static_cast<int>(v);
}
} // namespace

int Tileset::columns(int imageWidth) const
{
    const int usable = imageWidth - 2 * margin + spacing;
    const int step = tileWidth + spacing;
    return step > 0 && usable > 0 ? usable / step : 0;
}

int Tileset::rows(int imageHeight) const
{
    const int usable = imageHeight - 2 * margin + spacing;
    const int step = tileHeight + spacing;
    return step > 0 && usable > 0 ? usable / step : 0;
}

TileRect Tileset::tileRect(int index, int imageWidth) const
{
    const int cols = std::max(1, columns(imageWidth));
    const int col = index % cols;
    const int row = index / cols;
    return {margin + col * (tileWidth + spacing), margin + row * (tileHeight + spacing), tileWidth, tileHeight};
}

bool isTileSolid(const Tilemap& map, const Tileset* tileset, TileCell cell)
{
    if (map.solid.count(cell) > 0) {
        return true;
    }
    if (tileset == nullptr || tileset->solid.empty()) {
        return false;
    }
    for (const TileLayer& layer : map.layers) {
        if (!layer.visible) {
            continue;
        }
        const auto it = layer.tiles.find(cell);
        if (it != layer.tiles.end() && tileset->isSolid(it->second)) {
            return true;
        }
    }
    return false;
}

std::optional<Tileset> parseTileset(std::string_view text, std::string* error)
{
    Json j;
    try {
        j = Json::parse(text.begin(), text.end());
    } catch (const nlohmann::json::parse_error&) {
        setError(error, "The tileset file is not valid JSON.");
        return std::nullopt;
    }
    if (!j.is_object() || j.value("format", "") != TilesetFormat::kName) {
        setError(error, "This is not a Lantern tileset file (\"format\" should be \"lantern.tileset\").");
        return std::nullopt;
    }
    if (!j.contains("version") || !j["version"].is_number_integer() || j["version"].get<int>() < 1) {
        setError(error, "The tileset file has no valid format \"version\".");
        return std::nullopt;
    }
    if (j["version"].get<int>() > TilesetFormat::kVersion) {
        setError(error, "This tileset was saved by a newer version of Lantern. Update Lantern to use it.");
        return std::nullopt;
    }

    Tileset t;
    if (j.contains("image") && !j["image"].is_null()) {
        const auto id = j["image"].is_string() ? AssetId::parse(j["image"].get<std::string>()) : std::nullopt;
        if (!id) {
            setError(error, "\"image\" should be the ID of an image asset (16 hex digits).");
            return std::nullopt;
        }
        t.image = *id;
    }
    if (!j.contains("tileSize") || !j["tileSize"].is_array() || j["tileSize"].size() != 2
        || !j["tileSize"][0].is_number_integer() || !j["tileSize"][1].is_number_integer()) {
        setError(error, "\"tileSize\" should be two whole numbers, like [16, 16].");
        return std::nullopt;
    }
    t.tileWidth = j["tileSize"][0].get<int>();
    t.tileHeight = j["tileSize"][1].get<int>();
    if (t.tileWidth < 1 || t.tileHeight < 1 || t.tileWidth > 4096 || t.tileHeight > 4096) {
        setError(error, "Tiles must be between 1 and 4096 pixels wide and high.");
        return std::nullopt;
    }
    const auto margin = readInt(j, "margin", 0, 0, 4096);
    const auto spacing = readInt(j, "spacing", 0, 0, 4096);
    if (!margin || !spacing) {
        setError(error, "\"margin\" and \"spacing\" should be whole numbers of pixels (0 or more).");
        return std::nullopt;
    }
    t.margin = *margin;
    t.spacing = *spacing;
    if (j.contains("solid")) {
        if (!j["solid"].is_array()) {
            setError(error, "\"solid\" should be a list of tile numbers.");
            return std::nullopt;
        }
        for (const Json& n : j["solid"]) {
            if (!n.is_number_integer() || n.get<long long>() < 0 || n.get<long long>() > 1'000'000) {
                setError(error, "\"solid\" should contain tile numbers (0 or more).");
                return std::nullopt;
            }
            t.solid.insert(n.get<int>());
        }
    }
    return t;
}

std::string serializeTileset(const Tileset& t)
{
    Json j;
    j["format"] = TilesetFormat::kName;
    j["version"] = TilesetFormat::kVersion;
    j["image"] = t.image.isNull() ? Json() : Json(t.image.toString());
    j["tileSize"] = Json::array({t.tileWidth, t.tileHeight});
    j["margin"] = t.margin;
    j["spacing"] = t.spacing;
    j["solid"] = Json::array();
    for (int index : t.solid) {
        j["solid"].push_back(index);
    }
    return j.dump(2) + "\n";
}

} // namespace lantern
