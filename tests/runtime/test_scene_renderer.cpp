#include "SceneRenderer.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lantern;
using lantern::detail::SceneRenderer;

namespace {

// Axmol nodes are reference-counted: retain() keeps the root alive for the
// test, release() frees it and everything under it.
struct Root {
    ax::Node* node = ax::Node::create();
    Root() { node->retain(); }
    ~Root() { node->release(); }
    Root(const Root&) = delete;
    Root& operator=(const Root&) = delete;

    ax::Node* container() const { return node->getChildrenCount() > 0 ? node->getChildren().at(0) : nullptr; }
};

} // namespace

TEST_CASE("The renderer mirrors the scene tree as Axmol nodes", "[runtime][renderer]")
{
    Root root;
    SceneRenderer renderer(*root.node, /*textures=*/nullptr, /*drawContent=*/false);
    auto scene = std::make_shared<Scene>("Test");
    const EntityId parent = scene->create("Parent")->id();
    scene->setProperty(parent, "lantern.Transform", "position", Vec2{10, 20});
    scene->setProperty(parent, "lantern.Transform", "rotation", 30.0f);
    const EntityId child = scene->create("Child", parent)->id();
    scene->addComponent(child, std::make_unique<Shape>());
    scene->create("Hidden");
    scene->setActive(scene->roots().back(), false);

    renderer.show(scene);
    renderer.update();
    ax::Node* container = root.container();
    REQUIRE(container != nullptr);
    REQUIRE(container->getChildrenCount() == 2);
    ax::Node* p = container->getChildByName("Parent");
    REQUIRE(p != nullptr);
    CHECK(p->getPositionX() == 10.0f);
    CHECK(p->getRotation() == 30.0f);
    ax::Node* c = p->getChildByName("Child");
    REQUIRE(c != nullptr);
    CHECK_FALSE(container->getChildByName("Hidden")->isVisible());
}

TEST_CASE("The renderer rebuilds only when the scene changes", "[runtime][renderer]")
{
    Root root;
    SceneRenderer renderer(*root.node, /*textures=*/nullptr, /*drawContent=*/false);
    auto scene = std::make_shared<Scene>(makeStartScene());
    renderer.show(scene);
    renderer.update();
    renderer.update();
    CHECK(renderer.buildCount() == 1);

    scene->rename(scene->roots().front(), "Sky");
    renderer.update();
    CHECK(renderer.buildCount() == 2);
    CHECK(root.container()->getChildByName("Sky") != nullptr);
    CHECK(root.node->getChildrenCount() == 1); // the old mirror was removed

    renderer.show(nullptr);
    renderer.update();
    CHECK(root.node->getChildrenCount() == 0);
}

TEST_CASE("Moving things moves the nodes without rebuilding", "[runtime][renderer]")
{
    Root root;
    SceneRenderer renderer(*root.node, /*textures=*/nullptr, /*drawContent=*/false);
    auto scene = std::make_shared<Scene>("Test");
    const EntityId hero = scene->create("Hero")->id();
    renderer.show(scene);
    renderer.update();
    REQUIRE(renderer.buildCount() == 1);

    // Through a property, and directly (as gameplay code does).
    scene->setProperty(hero, "lantern.Transform", "position", Vec2{5, 6});
    renderer.update();
    CHECK(renderer.nodeFor(hero)->getPositionX() == 5.0f);
    scene->edit<Transform>(hero)->rotation = 90.0f;
    renderer.update();
    CHECK(renderer.nodeFor(hero)->getRotation() == 90.0f);
    CHECK(renderer.buildCount() == 1);

    // Anything else still rebuilds.
    scene->addComponent(hero, std::make_unique<Shape>());
    renderer.update();
    CHECK(renderer.buildCount() == 2);
    scene->edit<Shape>(hero)->color = Color{1, 2, 3, 255};
    renderer.update();
    CHECK(renderer.buildCount() == 3);
}

TEST_CASE("The first active camera decides the view", "[runtime][renderer][camera]")
{
    Root root;
    SceneRenderer renderer(*root.node, nullptr, false);
    auto scene = std::make_shared<Scene>("Cameras");
    renderer.show(scene);
    CHECK_FALSE(renderer.camera().has_value());

    const EntityId rig = scene->create("Rig")->id();
    scene->setProperty(rig, "lantern.Transform", "position", Vec2{100, 50});
    const EntityId off = scene->create("Off camera", rig)->id();
    scene->addComponent(off, std::make_unique<Camera>());
    scene->setActive(off, false);
    const EntityId on = scene->create("Main camera", rig)->id();
    scene->setProperty(on, "lantern.Transform", "position", Vec2{10, 5});
    scene->addComponent(on, std::make_unique<Camera>());
    scene->setProperty(on, "lantern.Camera", "zoom", 2.0f);

    const auto view = renderer.camera();
    REQUIRE(view.has_value());
    CHECK(view->center == Vec2{110, 55}); // world position: parent + own
    CHECK(view->zoom == 2.0f);

    scene->setActive(rig, false); // switching off a parent switches off its cameras
    CHECK_FALSE(renderer.camera().has_value());
}

TEST_CASE("Asset changes trigger a rebuild", "[runtime][renderer]")
{
    Root root;
    SceneRenderer renderer(*root.node, nullptr, false);
    auto scene = std::make_shared<Scene>("S");
    renderer.show(scene);
    renderer.update();
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / "lantern-tests" / "renderer-assets";
    std::filesystem::create_directories(folder);
    AssetDatabase assets(folder);
    renderer.setAssets(&assets);
    renderer.update();
    CHECK(renderer.buildCount() == 2);
    renderer.update();
    CHECK(renderer.buildCount() == 2);
    renderer.setAssets(nullptr);
    std::filesystem::remove_all(folder);
}
