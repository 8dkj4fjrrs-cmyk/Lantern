#include "Panels.hpp"

#include <cstdio>
#include <string>

namespace lantern::editor {

namespace {

template <class... Args>
std::string format(const char* pattern, Args... args)
{
    char buffer[96];
    std::snprintf(buffer, sizeof buffer, pattern, args...);
    return buffer;
}

std::string displayName(const SceneNodeInfo& node)
{
    return node.name.empty() ? "(unnamed)" : node.name;
}

} // namespace

InspectorPanel::InspectorPanel(EditorState& state)
    : m_state(state)
{
    setEmptyText("Select an object in the Hierarchy to see its properties.");
    setFootnote("Live values from the running scene. Editing, with undo, arrives with Lantern scenes in "
                "Milestone 3.");
}

void InspectorPanel::update(UiFrame& frame)
{
    const std::vector<SceneNodeInfo>& nodes = frame.sceneOutline();
    const SceneNodeInfo* selected = nullptr;
    const SceneNodeInfo* parent = nullptr;
    for (const SceneNodeInfo& node : nodes) {
        if (node.id == m_state.selectedNodeId) {
            selected = &node;
        }
    }
    if (selected != nullptr && selected->parentId != 0) {
        for (const SceneNodeInfo& node : nodes) {
            if (node.id == selected->parentId) {
                parent = &node;
            }
        }
    }

    setEmpty(selected == nullptr);
    if (selected == nullptr) {
        return;
    }

    const auto d = [](float v) { return static_cast<double>(v); };
    setTitle(displayName(*selected), selected->kind);
    setRows({
        {"Transform", "", true},
        {"Position", format("%.1f, %.1f", d(selected->x), d(selected->y))},
        {"Rotation", format("%.1f°", d(selected->rotationDegrees))},
        {"Scale", format("%.3f, %.3f", d(selected->scaleX), d(selected->scaleY))},
        {"Size", format("%.0f × %.0f", d(selected->width), d(selected->height))},
        {"Object", "", true},
        {"Draw order", std::to_string(selected->drawOrder)},
        {"Visible", selected->visible ? "Yes" : "No"},
        {"Children", std::to_string(selected->childCount)},
        {"Parent", parent != nullptr ? displayName(*parent) : "—"},
        {"Session id", std::to_string(selected->id)},
    });
}

} // namespace lantern::editor
