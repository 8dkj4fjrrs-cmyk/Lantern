#include "lantern/scene/Scene.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace lantern;
using Catch::Approx;

namespace {
Scene seeded()
{
    return Scene("Test", EntityIdGenerator(42));
}
} // namespace

TEST_CASE("Entity IDs are 16 hex digits and survive a round trip", "[scene][id]")
{
    EntityIdGenerator gen(1);
    const EntityId id = gen.next();
    REQUIRE_FALSE(id.isNull());
    CHECK(id.toString().size() == 16);
    CHECK(EntityId::parse(id.toString()) == id);
    CHECK(EntityId::parse(std::string(16, '0')) == std::nullopt); // zero is "no entity"
    CHECK(EntityId::parse("12345") == std::nullopt);
    CHECK(EntityId::parse("zzzzzzzzzzzzzzzz") == std::nullopt);
    CHECK(EntityId::parse("ABCDEF0123456789")->toString() == "abcdef0123456789");
}

TEST_CASE("Seeded generators repeat; different scenes get different IDs", "[scene][id]")
{
    EntityIdGenerator a(7), b(7);
    CHECK(a.next() == b.next());
    Scene s1, s2;
    CHECK(s1.create("A")->id() != s2.create("A")->id());
}

TEST_CASE("Colours read and write as hex", "[scene][types]")
{
    CHECK(Color{255, 136, 0, 255}.toHex() == "#ff8800ff");
    CHECK(Color::fromHex("#FF8800") == Color{255, 136, 0, 255});
    CHECK(Color::fromHex("ff880080") == Color{255, 136, 0, 128});
    CHECK_FALSE(Color::fromHex("#ff88"));
    CHECK_FALSE(Color::fromHex("#gg8800"));
}

TEST_CASE("Entities form a tree with ordered children", "[scene]")
{
    Scene scene = seeded();
    Entity* a = scene.create("A");
    Entity* b = scene.create("B");
    Entity* child = scene.create("Child", a->id());
    REQUIRE(child != nullptr);
    CHECK(scene.roots() == std::vector<EntityId>{a->id(), b->id()});
    CHECK(scene.childrenOf(a->id()) == std::vector<EntityId>{child->id()});
    CHECK(child->get<Transform>() != nullptr); // every entity has a Transform

    Entity* first = scene.create("First", {}, 0);
    CHECK(scene.roots().front() == first->id());
    CHECK(scene.indexOf(b->id()) == 2u);

    CHECK(scene.create("Orphan", EntityId(12345)) == nullptr); // missing parent
    CHECK(scene.create("Dup", {}, std::nullopt, a->id()) == nullptr); // ID in use

    std::vector<std::string> order;
    scene.forEach([&](const Entity& e, int depth) { order.push_back(std::to_string(depth) + e.name()); });
    CHECK(order == std::vector<std::string>{"0First", "0A", "1Child", "0B"});
}

TEST_CASE("Removing an entity removes everything inside it", "[scene]")
{
    Scene scene = seeded();
    const EntityId a = scene.create("A")->id();
    const EntityId child = scene.create("Child", a)->id();
    scene.create("Grandchild", child);
    const auto before = scene.revision();
    REQUIRE(scene.remove(a));
    CHECK(scene.empty());
    CHECK(scene.revision() > before);
    CHECK_FALSE(scene.remove(a));
}

TEST_CASE("Moving refuses to put an entity inside itself", "[scene]")
{
    Scene scene = seeded();
    const EntityId a = scene.create("A")->id();
    const EntityId b = scene.create("B", a)->id();
    CHECK_FALSE(scene.move(a, b, 0));
    CHECK_FALSE(scene.move(a, a, 0));
    REQUIRE(scene.move(b, {}, 0));
    CHECK(scene.roots() == std::vector<EntityId>{b, a});
    CHECK(scene.find(b)->parent().isNull());
}

TEST_CASE("Properties are set by name, with type checks and limits", "[scene][components]")
{
    Scene scene = seeded();
    const EntityId id = scene.create("Box")->id();
    REQUIRE(scene.addComponent(id, std::make_unique<Shape>()) != nullptr);
    CHECK(scene.addComponent(id, std::make_unique<Shape>()) == nullptr); // one per type

    CHECK(scene.setProperty(id, "lantern.Shape", "size", Vec2{-5.0f, 20.0f}));
    CHECK(scene.find(id)->get<Shape>()->size == Vec2{0.0f, 20.0f}); // clamped at 0
    CHECK(scene.setProperty(id, "lantern.Shape", "lineWidth", 3)); // int accepted for float
    CHECK(scene.find(id)->get<Shape>()->lineWidth == 3.0f);
    CHECK(scene.setProperty(id, "lantern.Shape", "kind", 1));
    CHECK(scene.find(id)->get<Shape>()->kind == ShapeKind::Ellipse);
    CHECK_FALSE(scene.setProperty(id, "lantern.Shape", "kind", 7));            // no such choice
    CHECK_FALSE(scene.setProperty(id, "lantern.Shape", "size", std::string("big"))); // wrong kind
    CHECK_FALSE(scene.setProperty(id, "lantern.Shape", "nope", 1.0f));
    CHECK_FALSE(scene.setProperty(id, "lantern.Sprite", "size", 1.0f));
}

TEST_CASE("Transform cannot be removed; other components can", "[scene][components]")
{
    Scene scene = seeded();
    const EntityId id = scene.create("Box")->id();
    scene.addComponent(id, std::make_unique<Shape>());
    CHECK(scene.removeComponent(id, "lantern.Transform") == nullptr);
    auto removed = scene.removeComponent(id, "lantern.Shape");
    REQUIRE(removed != nullptr);
    CHECK(scene.find(id)->get<Shape>() == nullptr);
    // Put back at the same position.
    CHECK(scene.addComponent(id, std::move(removed), 1) != nullptr);
    CHECK(scene.componentIndex(id, "lantern.Shape") == 1u);
}

TEST_CASE("Snapshots restore deleted entities with the same IDs", "[scene][snapshot]")
{
    Scene scene = seeded();
    const EntityId a = scene.create("A")->id();
    const EntityId b = scene.create("B")->id();
    const EntityId child = scene.create("Child", b)->id();
    scene.addComponent(child, std::make_unique<Shape>());
    scene.setProperty(child, "lantern.Shape", "color", Color{1, 2, 3, 4});

    const EntitySnapshot snap = scene.capture(b);
    CHECK(snap.items.size() == 2);
    CHECK(snap.index == 1);
    scene.remove(b);
    REQUIRE(scene.restore(snap, {}, snap.index));
    CHECK(scene.roots() == std::vector<EntityId>{a, b});
    CHECK(scene.find(child)->get<Shape>()->color == Color{1, 2, 3, 4});
    CHECK_FALSE(scene.restore(snap, {}, 0)); // IDs already present

    const EntitySnapshot copy = scene.withNewIds(snap);
    REQUIRE(scene.restore(copy, {}, 2));
    CHECK(scene.size() == 5);
    const Entity* dup = scene.find(copy.rootId());
    REQUIRE(dup != nullptr);
    CHECK(dup->id() != b);
    REQUIRE(dup->children().size() == 1);
    CHECK(dup->children()[0] != child);
}

TEST_CASE("Copying a scene copies its components", "[scene]")
{
    Scene scene = seeded();
    const EntityId id = scene.create("Box")->id();
    scene.addComponent(id, std::make_unique<Shape>());
    Scene copy = scene;
    scene.setProperty(id, "lantern.Shape", "size", Vec2{1, 1});
    CHECK(copy.find(id)->get<Shape>()->size == Vec2{100, 100});
}

TEST_CASE("World transforms combine parents; hit tests find the top shape", "[scene][geometry]")
{
    Scene scene = seeded();
    const EntityId parent = scene.create("Parent")->id();
    scene.setProperty(parent, "lantern.Transform", "position", Vec2{100, 100});
    scene.setProperty(parent, "lantern.Transform", "scale", Vec2{2, 2});
    const EntityId box = scene.create("Box", parent)->id();
    scene.setProperty(box, "lantern.Transform", "position", Vec2{10, 0});
    scene.addComponent(box, std::make_unique<Shape>()); // 100 x 100

    const Vec2 centre = scene.worldTransform(box).apply({0, 0});
    CHECK(centre.x == Approx(120));
    CHECK(centre.y == Approx(100));
    const auto corners = scene.worldCorners(box);
    REQUIRE(corners);
    CHECK((*corners)[0].x == Approx(20)); // 120 - 50*2
    CHECK((*corners)[2].y == Approx(200));

    CHECK(scene.hitTest({120, 100}) == box);
    CHECK(scene.hitTest({500, 500}).isNull());

    // Clockwise rotation by 90 degrees turns +x into -y.
    scene.setProperty(parent, "lantern.Transform", "rotation", 90.0f);
    const Vec2 rotated = scene.worldTransform(box).apply({0, 0});
    CHECK(rotated.x == Approx(100).margin(1e-3));
    CHECK(rotated.y == Approx(80).margin(1e-3));

    // Inactive parents hide their children from hit tests.
    scene.setActive(parent, false);
    CHECK(scene.hitTest(rotated).isNull());
}

TEST_CASE("Ellipses are hit inside the curve only", "[scene][geometry]")
{
    Scene scene = seeded();
    const EntityId id = scene.create("Ball")->id();
    auto shape = std::make_unique<Shape>();
    shape->kind = ShapeKind::Ellipse;
    scene.addComponent(id, std::move(shape));
    CHECK(scene.hitTest({0, 49}) == id);
    CHECK(scene.hitTest({45, 45}).isNull()); // inside the box, outside the circle
}

TEST_CASE("The start scene is a lantern made of shapes", "[scene]")
{
    const Scene scene = makeStartScene();
    CHECK(scene.size() > 10);
    CHECK(scene.find(scene.roots().front())->name() == "Background");
    CHECK(scene.hitTest({640, 352}) != EntityId{}); // the flame, centre of the world
}
