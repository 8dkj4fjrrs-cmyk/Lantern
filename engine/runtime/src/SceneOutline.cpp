#include "SceneOutline.hpp"

#include <algorithm>

namespace lantern::detail {

std::uint64_t NodeIdRegistry::idFor(const ax::Node* node)
{
    auto [it, inserted] = m_ids.try_emplace(node, m_nextId);
    if (inserted) {
        ++m_nextId;
    }
    return it->second;
}

void NodeIdRegistry::retainOnly(const std::vector<const ax::Node*>& alive)
{
    std::unordered_map<const ax::Node*, std::uint64_t> kept;
    kept.reserve(alive.size());
    for (const ax::Node* node : alive) {
        if (auto it = m_ids.find(node); it != m_ids.end()) {
            kept.emplace(node, it->second);
        }
    }
    m_ids = std::move(kept);
}

std::string nodeKind(const ax::Node& node)
{
    if (dynamic_cast<const ax::Scene*>(&node) != nullptr) {
        return "Scene";
    }
    if (dynamic_cast<const ax::DrawNode*>(&node) != nullptr) {
        return "Shape";
    }
    if (dynamic_cast<const ax::Sprite*>(&node) != nullptr) {
        return "Sprite";
    }
    if (dynamic_cast<const ax::Label*>(&node) != nullptr) {
        return "Text";
    }
    return "Node";
}

namespace {

void visit(const ax::Node& node, std::uint64_t parentId, int depth, NodeIdRegistry& ids,
           std::vector<SceneNodeInfo>& out)
{
    SceneNodeInfo info;
    info.id = ids.idFor(&node);
    info.parentId = parentId;
    info.depth = depth;
    info.name = std::string(node.getName());
    info.kind = nodeKind(node);
    info.x = node.getPositionX();
    info.y = node.getPositionY();
    info.rotationDegrees = node.getRotation();
    info.scaleX = node.getScaleX();
    info.scaleY = node.getScaleY();
    info.width = node.getContentSize().width;
    info.height = node.getContentSize().height;
    info.drawOrder = node.getLocalZOrder();
    info.visible = node.isVisible();
    info.childCount = static_cast<std::size_t>(node.getChildrenCount());
    const std::uint64_t id = info.id;
    out.push_back(std::move(info));

    // Children are kept sorted by draw order once the node has been drawn;
    // sort a copy here so the outline is ordered even before the first frame.
    std::vector<const ax::Node*> children(node.getChildren().begin(), node.getChildren().end());
    std::stable_sort(children.begin(), children.end(), [](const ax::Node* a, const ax::Node* b) {
        return a->getLocalZOrder() < b->getLocalZOrder();
    });
    for (const ax::Node* child : children) {
        visit(*child, id, depth + 1, ids, out);
    }
}

} // namespace

void buildSceneOutline(const ax::Node& root, NodeIdRegistry& ids, std::vector<SceneNodeInfo>& out)
{
    const std::size_t first = out.size();
    visit(root, 0, 0, ids, out);

    std::vector<const ax::Node*> alive;
    alive.reserve(out.size() - first);
    // Re-walk to collect pointers (the outline itself stores no pointers on purpose).
    std::vector<const ax::Node*> stack{&root};
    while (!stack.empty()) {
        const ax::Node* node = stack.back();
        stack.pop_back();
        alive.push_back(node);
        for (const ax::Node* child : node->getChildren()) {
            stack.push_back(child);
        }
    }
    ids.retainOnly(alive);
}

} // namespace lantern::detail
