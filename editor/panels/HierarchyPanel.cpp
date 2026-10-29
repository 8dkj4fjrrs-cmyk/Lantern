#include "Panels.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
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
            || a[i].label != b[i].label || a[i].detail != b[i].detail || a[i].hasChildren != b[i].hasChildren
            || a[i].icon != b[i].icon) {
            return false;
        }
    }
    return true;
}

bool containsIgnoringCase(std::string_view text, std::string_view needle)
{
    const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    return std::search(text.begin(), text.end(), needle.begin(), needle.end(),
                       [&](char a, char b) { return lower(a) == lower(b); })
           != text.end();
}

} // namespace

HierarchyPanel::HierarchyPanel(EditorState& state)
    : ui::Column(0.0f)
    , m_state(state)
{
    auto& toolbar = makePanelToolbar(*this);
    addToolButton(toolbar, ui::Icon::Plus, std::string("Create an object. ") + kComingWithEditing);
    auto& search = toolbar.emplace<ui::TextField>("Search name");
    search.setLeadingIcon(ui::Icon::Search);
    search.setClearButton(true);
    search.setStretch(1.0f);
    search.setOnChange([this](const std::string& text) { m_filter = text; });

    auto tree = std::make_unique<ui::TreeView>();
    tree->setStretch(1.0f);
    tree->setAutoExpandDepth(2); // World and its direct children start open
    tree->setEmptyText("The scene is empty.");
    tree->setOnSelect([this](std::uint64_t id) { m_state.selectedNodeId = id; });
    m_tree = &addChild(std::move(tree));
}

void HierarchyPanel::update(UiFrame& frame)
{
    const std::vector<SceneNodeInfo>& nodes = frame.sceneOutline();

    // With a search: the matching objects, plus their parents so each match
    // still appears in its place in the tree.
    std::unordered_set<std::uint64_t> keep;
    if (!m_filter.empty()) {
        std::unordered_map<std::uint64_t, std::uint64_t> parentOf;
        for (const SceneNodeInfo& node : nodes) {
            parentOf[node.id] = node.parentId;
        }
        for (const SceneNodeInfo& node : nodes) {
            if (containsIgnoringCase(node.name, m_filter)) {
                for (std::uint64_t id = node.id; id != 0 && keep.insert(id).second; id = parentOf[id]) {
                }
            }
        }
    }

    std::vector<ui::TreeItem> items;
    items.reserve(nodes.size());
    for (const SceneNodeInfo& node : nodes) {
        if (!m_filter.empty() && keep.count(node.id) == 0) {
            continue;
        }
        ui::TreeItem item;
        item.id = node.id;
        item.parentId = node.parentId;
        item.depth = node.depth;
        item.label = node.name.empty() ? "(unnamed)" : node.name;
        item.detail = node.kind;
        item.hasChildren = node.childCount > 0;
        if (node.depth == 0) {
            item.icon = ui::Icon::Flame; // the scene's root
        }
        items.push_back(std::move(item));
    }
    m_tree->setEmptyText(m_filter.empty() ? "The scene is empty." : "No objects match “" + m_filter + "”.");
    if (!sameItems(items, m_tree->items())) {
        m_tree->setItems(std::move(items));
    }
    // The selection can also change elsewhere (or vanish with its object).
    if (m_tree->selectedId() != m_state.selectedNodeId) {
        m_tree->setSelectedId(m_state.selectedNodeId);
    }
}

} // namespace lantern::editor
