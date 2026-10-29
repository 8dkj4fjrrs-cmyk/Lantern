#include "Panels.hpp"

#include <vector>

namespace lantern::editor {

namespace {

bool sameItems(const std::vector<ui::TreeItem>& a, const std::vector<ui::TreeItem>& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || a[i].parentId != b[i].parentId || a[i].depth != b[i].depth
            || a[i].label != b[i].label || a[i].detail != b[i].detail || a[i].hasChildren != b[i].hasChildren) {
            return false;
        }
    }
    return true;
}

} // namespace

HierarchyPanel::HierarchyPanel(EditorState& state)
    : m_state(state)
{
    setAutoExpandDepth(2); // World and its direct children start open
    setEmptyText("The scene is empty.");
    setOnSelect([this](std::uint64_t id) { m_state.selectedNodeId = id; });
}

void HierarchyPanel::update(UiFrame& frame)
{
    const std::vector<SceneNodeInfo>& nodes = frame.sceneOutline();
    std::vector<ui::TreeItem> items;
    items.reserve(nodes.size());
    for (const SceneNodeInfo& node : nodes) {
        ui::TreeItem item;
        item.id = node.id;
        item.parentId = node.parentId;
        item.depth = node.depth;
        item.label = node.name.empty() ? "(unnamed)" : node.name;
        item.detail = node.kind;
        item.hasChildren = node.childCount > 0;
        items.push_back(std::move(item));
    }
    if (!sameItems(items, this->items())) {
        setItems(std::move(items));
    }
    // The selection can also change elsewhere (or vanish with its object).
    if (selectedId() != m_state.selectedNodeId) {
        setSelectedId(m_state.selectedNodeId);
    }
}

} // namespace lantern::editor
