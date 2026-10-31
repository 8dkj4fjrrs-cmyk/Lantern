#include "SceneRenderer.hpp"

#include "TextureStore.hpp"

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

// What a sprite without a usable image looks like: a pink box with a cross,
// impossible to mistake for real art.
ax::Node* makePlaceholder(Vec2 size)
{
    auto* node = ax::DrawNode::create();
    const float hw = std::max(4.0f, size.x * 0.5f);
    const float hh = std::max(4.0f, size.y * 0.5f);
    const ax::Color4F pink(1.0f, 0.2f, 0.75f, 1.0f);
    node->drawSolidRect(ax::Vec2(-hw, -hh), ax::Vec2(hw, hh), ax::Color4F(1.0f, 0.2f, 0.75f, 0.25f));
    node->drawRect(ax::Vec2(-hw, -hh), ax::Vec2(hw, hh), pink, 2.0f);
    node->drawLine(ax::Vec2(-hw, -hh), ax::Vec2(hw, hh), pink, 2.0f);
    node->drawLine(ax::Vec2(-hw, hh), ax::Vec2(hw, -hh), pink, 2.0f);
    return node;
}

} // namespace

SceneRenderer::SceneRenderer(ax::Node& root, TextureStore* textures, bool drawContent)
    : m_root(root)
    , m_textures(textures)
    , m_drawContent(drawContent)
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

void SceneRenderer::setAssets(const AssetDatabase* assets)
{
    m_assets = assets;
}

void SceneRenderer::update()
{
    const bool sceneChanged =
        m_scene.get() != m_builtScene || (m_scene != nullptr && m_scene->revision() != m_builtRevision);
    const bool assetsChanged =
        m_assets != m_builtAssets || (m_assets != nullptr && m_assets->revision() != m_builtAssetsRevision);
    const bool texturesChanged = m_textures != nullptr && m_textures->generation() != m_builtTextures;
    if (sceneChanged || assetsChanged || texturesChanged) {
        rebuild();
    }
}

std::optional<CameraView> SceneRenderer::camera() const
{
    if (m_scene == nullptr) {
        return std::nullopt;
    }
    std::optional<CameraView> found;
    m_scene->forEach([&](const Entity& entity, int) {
        if (found) {
            return;
        }
        const Camera* camera = entity.get<Camera>();
        if (camera == nullptr) {
            return;
        }
        for (const Entity* e = &entity; e != nullptr; e = m_scene->find(e->parent())) {
            if (!e->isActive()) {
                return;
            }
        }
        found = CameraView{m_scene->worldTransform(entity.id()).apply({0.0f, 0.0f}), camera->zoom};
    });
    return found;
}

ax::Node* SceneRenderer::makeSpriteNode(const Sprite& sprite)
{
    const AssetRecord* asset = m_assets != nullptr && !sprite.image.isNull() ? m_assets->find(sprite.image) : nullptr;
    ax::Texture2D* texture = nullptr;
    if (asset != nullptr && asset->type == AssetType::Image && asset->isUsable() && m_textures != nullptr) {
        texture = m_textures->get(m_assets->absolutePath(*asset), asset->filter == TextureFilter::Pixel);
    }
    if (texture == nullptr) {
        // No image chosen yet: draw nothing in the game; a missing or broken image: a placeholder.
        return sprite.image.isNull() ? ax::Node::create() : makePlaceholder(sprite.size);
    }
    auto* node = ax::Sprite::createWithTexture(texture);
    const ax::Size natural = node->getContentSize();
    if (natural.width > 0.0f && natural.height > 0.0f) {
        node->setScale(sprite.size.x / natural.width, sprite.size.y / natural.height);
    }
    node->setFlippedX(sprite.flipX);
    node->setFlippedY(sprite.flipY);
    node->setColor(ax::Color3B(sprite.color.r, sprite.color.g, sprite.color.b));
    node->setOpacity(sprite.color.a);
    return node;
}

ax::Node* SceneRenderer::makeTilemapNode(const Tilemap& map)
{
    auto* root = ax::Node::create();
    root->setName("Tiles");
    // The tileset, its image and the image's texture, if all are usable.
    const AssetRecord* tilesetAsset = m_assets != nullptr && !map.tileset.isNull() ? m_assets->find(map.tileset) : nullptr;
    const Tileset* tileset = tilesetAsset != nullptr && tilesetAsset->tileset ? &*tilesetAsset->tileset : nullptr;
    const AssetRecord* image = tileset != nullptr && m_assets != nullptr ? m_assets->find(tileset->image) : nullptr;
    ax::Texture2D* texture = nullptr;
    if (image != nullptr && image->isUsable() && image->image && m_textures != nullptr) {
        texture = m_textures->get(m_assets->absolutePath(*image), image->filter == TextureFilter::Pixel);
    }
    const float cw = map.cellSize.x;
    const float ch = map.cellSize.y;
    for (std::size_t i = 0; i < map.layers.size(); ++i) {
        const TileLayer& layer = map.layers[i];
        auto* layerNode = ax::Node::create();
        layerNode->setName(layer.name);
        layerNode->setVisible(layer.visible);
        root->addChild(layerNode, static_cast<int>(i));
        if (texture == nullptr) {
            if (layer.tiles.empty() || map.tileset.isNull()) {
                continue;
            }
            // Tiles whose tileset or image is missing: pink cells, impossible to miss.
            auto* draw = ax::DrawNode::create();
            for (const auto& [cell, tile] : layer.tiles) {
                const ax::Vec2 lo(static_cast<float>(cell.x) * cw, static_cast<float>(cell.y) * ch);
                draw->drawSolidRect(lo, lo + ax::Vec2(cw, ch), ax::Color4F(1.0f, 0.2f, 0.75f, 0.35f));
                draw->drawRect(lo, lo + ax::Vec2(cw, ch), ax::Color4F(1.0f, 0.2f, 0.75f, 1.0f), 1.0f);
            }
            layerNode->addChild(draw);
            continue;
        }
        const int imageWidth = image->image->width;
        const int count = tileset->columns(imageWidth) * tileset->rows(image->image->height);
        for (const auto& [cell, tile] : layer.tiles) {
            if (tile < 0 || tile >= count) {
                continue; // a tile number the tileset does not have (e.g. after changing the tile size)
            }
            const TileRect r = tileset->tileRect(tile, imageWidth);
            auto* sprite = ax::Sprite::createWithTexture(
                texture, ax::Rect(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.width),
                                  static_cast<float>(r.height)));
            sprite->setAnchorPoint(ax::Vec2::ZERO);
            sprite->setPosition(static_cast<float>(cell.x) * cw, static_cast<float>(cell.y) * ch);
            sprite->setScale(cw / static_cast<float>(r.width), ch / static_cast<float>(r.height));
            layerNode->addChild(sprite);
        }
    }
    return root;
}

void SceneRenderer::rebuild()
{
    if (m_container != nullptr) {
        m_container->removeFromParent();
        m_container = nullptr;
    }
    m_builtScene = m_scene.get();
    m_builtRevision = m_scene != nullptr ? m_scene->revision() : 0;
    m_builtAssets = m_assets;
    m_builtAssetsRevision = m_assets != nullptr ? m_assets->revision() : 0;
    ++m_builds;
    if (m_scene == nullptr) {
        m_builtTextures = m_textures != nullptr ? m_textures->generation() : 0;
        return;
    }

    m_container = ax::Node::create();
    m_container->setName(m_scene->name());
    m_root.addChild(m_container);

    std::unordered_map<EntityId, ax::Node*> nodes;
    m_scene->forEach([&](const Entity& entity, int) {
        auto* node = ax::Node::create();
        node->setName(entity.name());
        const Transform& t = entity.transform();
        node->setPosition(t.position.x, t.position.y);
        node->setRotation(t.rotation); // both Axmol and Lantern: degrees, clockwise
        node->setScale(t.scale.x, t.scale.y);
        node->setVisible(entity.isActive());

        // The entity's own content, drawn below its children (local z -1).
        if (m_drawContent) {
            if (const Shape* shape = entity.get<Shape>()) {
                auto* draw = ax::DrawNode::create();
                drawShape(*draw, *shape);
                node->addChild(draw, -1);
            }
            if (const Sprite* sprite = entity.get<Sprite>()) {
                node->addChild(makeSpriteNode(*sprite), -1);
            }
            if (const Tilemap* map = entity.get<Tilemap>()) {
                node->addChild(makeTilemapNode(*map), -1);
            }
        }

        ax::Node* parent = m_container;
        if (const auto it = nodes.find(entity.parent()); it != nodes.end()) {
            parent = it->second;
        }
        // Sibling order is drawing order.
        parent->addChild(node, static_cast<int>(m_scene->indexOf(entity.id()).value_or(0)));
        nodes.emplace(entity.id(), node);
    });
    // Loading textures above may itself have bumped the generation.
    m_builtTextures = m_textures != nullptr ? m_textures->generation() : 0;
}

} // namespace lantern::detail
