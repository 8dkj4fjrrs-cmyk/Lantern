#include "Panels.hpp"

#include <imgui.h>

#include <climits>
#include <string>

namespace lantern::editor {

namespace {

std::string displayName(const SceneNodeInfo& node)
{
    return node.name.empty() ? "(unnamed " + node.kind + ")" : node.name;
}

void* idPointer(std::uint64_t id)
{
    // ImGui identifies tree rows by a pointer-sized value; our ids fit.
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));
}

} // namespace

void drawHierarchyPanel(bool& open, UiFrame& ui, EditorState& state)
{
    if (!open) {
        return;
    }
    if (!ImGui::Begin(kHierarchyPanel, &open)) {
        ImGui::End();
        return;
    }

    const std::vector<SceneNodeInfo>& nodes = ui.sceneOutline();

    // Drop a selection whose object no longer exists.
    bool selectionAlive = false;
    for (const SceneNodeInfo& node : nodes) {
        selectionAlive = selectionAlive || node.id == state.selectedNodeId;
    }
    if (!selectionAlive) {
        state.selectedNodeId = 0;
    }

    if (nodes.empty()) {
        ImGui::TextDisabled("The scene is empty.");
    }

    // The outline is a flat list in tree order (parents first, with a depth).
    // Walk it once, opening and closing tree levels as the depth changes.
    int openLevels = 0;            // how many TreeNode levels are currently pushed
    int skipDeeperThan = INT_MAX;  // set when a node is collapsed: hide its children
    for (const SceneNodeInfo& node : nodes) {
        if (node.depth > skipDeeperThan) {
            continue;
        }
        skipDeeperThan = INT_MAX;
        while (openLevels > node.depth) {
            ImGui::TreePop();
            --openLevels;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                                   | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (node.depth < 2) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if (node.childCount == 0) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (node.id == state.selectedNodeId) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const std::string label = displayName(node);
        const bool expanded = ImGui::TreeNodeEx(idPointer(node.id), flags, "%s", label.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            state.selectedNodeId = node.id;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", node.kind.c_str());

        if (node.childCount > 0) {
            if (expanded) {
                ++openLevels;
            } else {
                skipDeeperThan = node.depth;
            }
        }
    }
    while (openLevels > 0) {
        ImGui::TreePop();
        --openLevels;
    }

    // Clicking empty space clears the selection.
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
        state.selectedNodeId = 0;
    }

    ImGui::End();
}

} // namespace lantern::editor
