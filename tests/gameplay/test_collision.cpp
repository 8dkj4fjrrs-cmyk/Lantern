#include "rolo/gameplay/Collision.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace rolo;
using Catch::Approx;

namespace {

EntityId addBox(Scene& scene, const char* name, Vec2 position, Vec2 size, bool solid = true, EntityId parent = {})
{
    const EntityId id = scene.create(name, parent)->id();
    scene.setProperty(id, "rolo.Transform", "position", position);
    auto collider = std::make_unique<Collider>();
    collider->size = size;
    collider->solid = solid;
    scene.addComponent(id, std::move(collider));
    return id;
}

} // namespace

TEST_CASE("Collider boxes follow the object, its parents and offset", "[gameplay][collision]")
{
    Scene scene;
    const EntityId parent = scene.create("Parent")->id();
    scene.setProperty(parent, "rolo.Transform", "position", Vec2{100, 0});
    scene.setProperty(parent, "rolo.Transform", "scale", Vec2{2, 2});
    const EntityId child = addBox(scene, "Child", {10, 10}, {4, 6}, true, parent);
    scene.setProperty(child, "rolo.Collider", "offset", Vec2{1, 0});

    const auto box = colliderBox(scene, child);
    REQUIRE(box);
    // centre: 100 + 2 * (10 + 1) = 122, 2 * 10 = 20; size doubled: 8 x 12
    CHECK(box->min.x == Approx(118));
    CHECK(box->max.x == Approx(126));
    CHECK(box->min.y == Approx(14));
    CHECK(box->max.y == Approx(26));

    scene.setActive(parent, false);
    CHECK_FALSE(colliderBox(scene, child));   // inactive objects do not collide
}

TEST_CASE("A rotated collider counts as the box around it", "[gameplay][collision]")
{
    Scene scene;
    const EntityId id = addBox(scene, "Bar", {0, 0}, {10, 2});
    scene.setProperty(id, "rolo.Transform", "rotation", 90.0f);
    const auto box = colliderBox(scene, id);
    REQUIRE(box);
    CHECK(box->size().x == Approx(2).margin(1e-4));
    CHECK(box->size().y == Approx(10).margin(1e-4));
}

TEST_CASE("Overlap queries find colliders but not mere neighbours", "[gameplay][collision]")
{
    Scene scene;
    const EntityId a = addBox(scene, "A", {0, 0}, {10, 10});
    const EntityId door = addBox(scene, "Door", {8, 0}, {10, 10}, /*solid=*/false);
    addBox(scene, "Neighbour", {-10, 0}, {10, 10});   // touches A's left edge only

    CHECK(overlappingColliders(scene, *colliderBox(scene, a), a) == std::vector<EntityId>{door});
    CHECK(isBlocked(scene, nullptr, Box::around({0, 0}, {10, 10}), a) == false); // the door is not solid
    CHECK(isBlocked(scene, nullptr, Box::around({-10, 0}, {4, 4})));
}

TEST_CASE("Moving stops at solid colliders and slides along them", "[gameplay][collision]")
{
    Scene scene;
    const EntityId wall = addBox(scene, "Wall", {20, 0}, {10, 100});   // x 15..25
    const Box hero = Box::around({0, 0}, {10, 10});                    // x -5..5

    SECTION("straight into the wall: stops touching it")
    {
        const MoveResult r = sweepBox(scene, nullptr, hero, {30, 0});
        CHECK(r.moved.x == Approx(10));
        CHECK(r.blockedX);
        CHECK(r.hit == wall);
    }
    SECTION("diagonally: the blocked axis stops, the other carries on")
    {
        const MoveResult r = sweepBox(scene, nullptr, hero, {30, 7});
        CHECK(r.moved.x == Approx(10));
        CHECK(r.moved.y == Approx(7));
        CHECK_FALSE(r.blockedY);
    }
    SECTION("away from the wall: free")
    {
        const MoveResult r = sweepBox(scene, nullptr, hero, {-30, 0});
        CHECK(r.moved.x == Approx(-30));
        CHECK_FALSE(r.blockedX);
    }
    SECTION("already touching (with rounding): still blocked")
    {
        const Box touching = hero.moved({10.0f + 2e-4f, 0});   // a hair inside the wall
        const MoveResult r = sweepBox(scene, nullptr, touching, {5, 0});
        CHECK(r.moved.x == Approx(0).margin(1e-3));
        CHECK(r.blockedX);
    }
    SECTION("stuck inside a wall: can get out")
    {
        const Box inside = Box::around({20, 0}, {4, 4});
        CHECK(sweepBox(scene, nullptr, inside, {-30, 0}).moved.x == Approx(-30));
    }
    SECTION("the moving object's own collider is ignored")
    {
        const MoveResult r = sweepBox(scene, nullptr, *colliderBox(scene, wall), {50, 0}, wall);
        CHECK(r.moved.x == Approx(50));
    }
}

TEST_CASE("Solid tiles block movement: painted collision and solid tileset tiles", "[gameplay][collision][tilemap]")
{
    Scene scene;
    const EntityId mapId = scene.create("Map")->id();
    scene.setProperty(mapId, "rolo.Transform", "position", Vec2{100, 100});
    auto map = std::make_unique<Tilemap>();
    map->cellSize = {16, 16};
    map->setSolid({2, 0}, true);          // painted: x 132..148, y 100..116
    map->setTile(0, {-1, 0}, 5);          // a tile, solid only if the tileset says so
    scene.addComponent(mapId, std::move(map));

    const std::vector<Box> tiles = solidTiles(scene, nullptr, Box{{0, 0}, {300, 300}});
    REQUIRE(tiles.size() == 1);
    CHECK(tiles[0] == Box{{132, 100}, {148, 116}});

    // Walking right along row 0 stops at the painted cell.
    const Box hero = Box::around({110, 108}, {8, 8});   // x 106..114
    const MoveResult r = sweepBox(scene, nullptr, hero, {40, 0});
    CHECK(r.moved.x == Approx(18));
    CHECK(r.blockedX);
    CHECK(r.hit.isNull());   // a tile, not a collider

    // Scaling the map scales its cells.
    scene.setProperty(mapId, "rolo.Transform", "scale", Vec2{2, 2});
    const std::vector<Box> scaled = solidTiles(scene, nullptr, Box{{0, 0}, {300, 300}});
    REQUIRE(scaled.size() == 1);
    CHECK(scaled[0] == Box{{164, 100}, {196, 132}});
}
