#include "SceneRenderer.hpp"

#include <unordered_map>

namespace lantern::detail {

namespace {

ax::Color4F toAxmol(Color c)
{
    return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

// Draws a Shape centred on the node's origin.
void drawShape(ax::DrawNode& node, const Shape& shape)
{
    const float hw = shape.size.x * 0.5f;
    const float hh = shape.size.y * 0.5f;
    if (hw <= 0.0f || hh <= 0.0f || shape.color.a == 0) {
        return;
    }
    const ax::Color4F color = toAxmol(shape.color);
    const float thickness = std::max(0.5f, shape.lineWidth);
    if (shape.kind == ShapeKind::Rectangle) {
        if (shape.filled) {
            node.drawSolidRect(ax::Vec2(-hw, -hh), ax::Vec2(hw, hh), color);
        } else {
            node.drawRect(ax::Vec2(-hw, -hh), ax::Vec2(hw, hh), color, thickness);
        }
        return;
    }
    // Ellipses: a circle of radius hw, stretched vertically to hh.
    const unsigned int segments = std::clamp(static_cast<unsigned int>(std::max(hw, hh) * 0.6f), 24u, 128u);
    if (shape.filled) {
        node.drawSolidCircle(ax::Vec2::ZERO, hw, 0.0f, segments, 1.0f, hh / hw, color);
    } else {
        node.drawCircle(ax::Vec2::ZERO, hw, 0.0f, segments, false, 1.0f, hh / hw, color, thickness);
    }
}

} // namespace

SceneRenderer::SceneRenderer(ax::Node& root, bool drawShapes)
    : m_root(root)
    , m_drawShapes(drawShapes)
{
}

SceneRenderer::~SceneRenderer()
{
    if (m_container != nullptr) {
        m_container->removeFromParent();
    }
}

void SceneRenderer::show(std::shared_ptr<const Scene> scene)
{
    m_scene = std::move(scene);
}

void SceneRenderer::update()
{
    if (m_scene.get() == m_builtScene && (m_scene == nullptr || m_scene->revision() == m_builtRevision)) {
        return;
    }
    rebuild();
}

void SceneRenderer::rebuild()
{
    if (m_container != nullptr) {
        m_container->removeFromParent();
        m_container = nullptr;
    }
    m_builtScene = m_scene.get();
    m_builtRevision = m_scene != nullptr ? m_scene->revision() : 0;
    ++m_builds;
    if (m_scene == nullptr) {
        return;
    }

    m_container = ax::Node::create();
    m_container->setName(m_scene->name());
    m_root.addChild(m_container);

    std::unordered_map<EntityId, ax::Node*> nodes;
    m_scene->forEach([&](const Entity& entity, int) {
        ax::Node* node = nullptr;
        if (const Shape* shape = entity.get<Shape>(); shape != nullptr && m_drawShapes) {
            auto* draw = ax::DrawNode::create();
            drawShape(*draw, *shape);
            node = draw;
        } else {
            node = ax::Node::create();
        }
        node->setName(entity.name());
        const Transform& t = entity.transform();
        node->setPosition(t.position.x, t.position.y);
        node->setRotation(t.rotation); // both Axmol and Lantern: degrees, clockwise
        node->setScale(t.scale.x, t.scale.y);
        node->setVisible(entity.isActive());

        ax::Node* parent = m_container;
        if (const auto it = nodes.find(entity.parent()); it != nodes.end()) {
            parent = it->second;
        }
        // Sibling order is drawing order.
        parent->addChild(node, static_cast<int>(m_scene->indexOf(entity.id()).value_or(0)));
        nodes.emplace(entity.id(), node);
    });
}

} // namespace lantern::detail
