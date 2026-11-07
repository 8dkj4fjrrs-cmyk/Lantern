#include "rolo/assets/Tileset.hpp"
#include "rolo/scene/SceneFile.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace rolo;

namespace {

const ComponentRegistry& registry()
{
    static const ComponentRegistry r = ComponentRegistry::withBuiltins();
    return r;
}

} // namespace

TEST_CASE("Tiles can be set, replaced and removed", "[scene][tilemap]")
{
    Tilemap map;
    CHECK(map.tileAt(0, {0, 0}) == -1);
    CHECK(map.setTile(0, {0, 0}, 3));
    CHECK_FALSE(map.setTile(0, {0, 0}, 3)); // no change
    CHECK(map.setTile(0, {-2, 5}, 7));
    CHECK(map.tileAt(0, {-2, 5}) == 7);
    CHECK(map.setTile(0, {0, 0}, -1));
    CHECK(map.tileAt(0, {0, 0}) == -1);
    CHECK_FALSE(map.setTile(4, {0, 0}, 1)); // no such layer

    const auto used = map.usedCells();
    REQUIRE(used.has_value());
    CHECK(used->min == TileCell{-2, 5});
    CHECK(used->max == TileCell{-2, 5});

    map.cellSize = {16.0f, 8.0f};
    CHECK(map.cellAt({17.0f, 3.0f}) == TileCell{1, 0});
    CHECK(map.cellAt({-0.5f, -0.5f}) == TileCell{-1, -1});
}

TEST_CASE("A tilemap survives save and load, one line per row", "[scene][tilemap][file]")
{
    Scene scene("Map", EntityIdGenerator(11));
    Entity* e = scene.create("Level");
    auto map = std::make_unique<Tilemap>();
    map->tileset = AssetId(0xabc);
    for (int x = -2; x <= 2; ++x) {
        map->setTile(0, {x, 0}, 3);
    }
    map->setTile(0, {1, 0}, -1); // a gap inside the row
    map->layers.push_back(TileLayer{"Decor", false, {}});
    map->setTile(1, {0, 1}, 9);
    map->setSolid({4, 0}, true);
    map->setSolid({6, 0}, true);
    scene.addComponent(e->id(), std::move(map));

    const std::string text = serializeScene(scene);
    CHECK(text.find("[0, -2, 3, 3, 3, -1, 3]") != std::string::npos);
    CHECK(text.find("[0, 4, 1, 0, 1]") != std::string::npos);
    CHECK(text.find("\"name\": \"Decor\"") != std::string::npos);

    const SceneLoadResult loaded = parseScene(text, registry());
    REQUIRE(loaded.ok());
    CHECK(loaded.issues.empty());
    const Tilemap* back = loaded.scene->find(e->id())->get<Tilemap>();
    REQUIRE(back != nullptr);
    CHECK(back->tileset == AssetId(0xabc));
    REQUIRE(back->layers.size() == 2);
    CHECK(back->layers[0].tiles.size() == 4);
    CHECK(back->tileAt(0, {1, 0}) == -1);
    CHECK(back->tileAt(1, {0, 1}) == 9);
    CHECK_FALSE(back->layers[1].visible);
    CHECK(back->solid.size() == 2);
    CHECK(serializeScene(*loaded.scene) == text);
}

TEST_CASE("Damaged tilemap data keeps what can be read, with a warning", "[scene][tilemap][file]")
{
    const std::string text = R"({"format": "rolo.scene", "version": 1, "name": "T", "entities": [
        {"id": "0000000000000001", "name": "Map", "components": [
          {"type": "rolo.Tilemap", "tileset": null, "cellSize": [16, 16],
           "data": {"layers": [{"name": "Ground", "rows": [[0, 0, 1, "x", 2], "bad"]}]}}]}]})";
    const SceneLoadResult r = parseScene(text, registry());
    REQUIRE(r.ok());
    REQUIRE_FALSE(r.issues.empty());
    CHECK(r.issues[0].message.find("could not be read") != std::string::npos);
    const Tilemap* map = r.scene->find(EntityId(1))->get<Tilemap>();
    CHECK(map->tileAt(0, {0, 0}) == 1);
    CHECK(map->tileAt(0, {2, 0}) == 2);
}

TEST_CASE("A tilemap can be picked where it has tiles", "[scene][tilemap]")
{
    Scene scene("Pick", EntityIdGenerator(5));
    Entity* e = scene.create("Map");
    scene.setProperty(e->id(), "rolo.Transform", "position", Vec2{100.0f, 100.0f});
    auto map = std::make_unique<Tilemap>();
    map->setTile(0, {0, 0}, 1);
    map->setTile(0, {2, 1}, 1);
    scene.addComponent(e->id(), std::move(map));
    // Cells 0..2 x 0..1 of 16 points: world 100..148 x 100..132.
    CHECK(scene.hitTest({101.0f, 101.0f}) == e->id());
    CHECK(scene.hitTest({147.0f, 131.0f}) == e->id());
    CHECK(scene.hitTest({99.0f, 101.0f}).isNull());
    const auto corners = scene.worldCorners(e->id());
    REQUIRE(corners.has_value());
    CHECK((*corners)[0] == Vec2{100.0f, 100.0f});
    CHECK((*corners)[2] == Vec2{148.0f, 132.0f});
}

TEST_CASE("Collision comes from painted cells and solid tiles of visible layers", "[scene][tilemap][collision]")
{
    Tilemap map;
    map.setTile(0, {0, 0}, 2);  // solid in the tileset
    map.setTile(0, {1, 0}, 1);  // not solid
    map.setSolid({5, 5}, true);
    Tileset tileset;
    tileset.solid = {2};

    CHECK(isTileSolid(map, &tileset, {0, 0}));
    CHECK_FALSE(isTileSolid(map, &tileset, {1, 0}));
    CHECK(isTileSolid(map, &tileset, {5, 5}));
    CHECK_FALSE(isTileSolid(map, nullptr, {0, 0})); // no tileset: only painted collision
    map.layers[0].visible = false;
    CHECK_FALSE(isTileSolid(map, &tileset, {0, 0})); // hidden layers do not collide
}
