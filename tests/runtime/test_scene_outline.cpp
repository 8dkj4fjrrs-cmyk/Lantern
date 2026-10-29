#include "SceneOutline.hpp"

#include <catch2/catch_test_macros.hpp>

using lantern::SceneNodeInfo;
using lantern::detail::buildSceneOutline;
using lantern::detail::NodeIdRegistry;

namespace {

// Builds:  Root ─┬─ B (draw order 2)
//                └─ A (draw order 1) ── A1
// ax::Node objects are reference-counted; retain() keeps the root alive for
// the test and release() frees the whole tree.
ax::Node* makeTree()
{
    auto* root = ax::Node::create();
    root->setName("Root");
    auto* b = ax::Node::create();
    b->setName("B");
    root->addChild(b, 2);
    auto* a = ax::Node::create();
    a->setName("A");
    a->setPosition(10.0f, 20.0f);
    root->addChild(a, 1);
    auto* a1 = ax::Node::create();
    a1->setName("A1");
    a->addChild(a1);
    root->retain();
    return root;
}

} // namespace

TEST_CASE("The outline lists parents before children, siblings in draw order", "[runtime][outline]")
{
    ax::Node* root = makeTree();
    NodeIdRegistry ids;
    std::vector<SceneNodeInfo> out;
    buildSceneOutline(*root, ids, out);

    REQUIRE(out.size() == 4);
    CHECK(out[0].name == "Root");
    CHECK(out[1].name == "A");
    CHECK(out[2].name == "A1");
    CHECK(out[3].name == "B");
    CHECK(out[0].depth == 0);
    CHECK(out[2].depth == 2);
    CHECK(out[2].parentId == out[1].id);
    CHECK(out[1].x == 10.0f);
    CHECK(out[1].childCount == 1);
    CHECK(out[0].kind == "Node");
    root->release();
}

TEST_CASE("Ids stay the same between snapshots and are never 0", "[runtime][outline]")
{
    ax::Node* root = makeTree();
    NodeIdRegistry ids;
    std::vector<SceneNodeInfo> first;
    std::vector<SceneNodeInfo> second;
    buildSceneOutline(*root, ids, first);
    buildSceneOutline(*root, ids, second);

    REQUIRE(first.size() == second.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(first[i].id != 0);
        CHECK(first[i].id == second[i].id);
    }
    root->release();
}

TEST_CASE("Removed nodes are forgotten by the id registry", "[runtime][outline]")
{
    ax::Node* root = makeTree();
    NodeIdRegistry ids;
    std::vector<SceneNodeInfo> out;
    buildSceneOutline(*root, ids, out);
    CHECK(ids.size() == 4);

    root->removeChildByName("B");
    out.clear();
    buildSceneOutline(*root, ids, out);
    CHECK(out.size() == 3);
    CHECK(ids.size() == 3);
    root->release();
}
