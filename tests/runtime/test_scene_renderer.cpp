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
    SceneRenderer renderer(*root.node, /*drawShapes=*/false);
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
    SceneRenderer renderer(*root.node, /*drawShapes=*/false);
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
