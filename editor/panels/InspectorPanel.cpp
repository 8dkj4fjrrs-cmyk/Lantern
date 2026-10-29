#include "Panels.hpp"

#include <imgui.h>

#include <cstdarg>
#include <string>

namespace lantern::editor {

namespace {

void row(const char* label, const char* format, ...) IM_FMTARGS(2);

void row(const char* label, const char* format, ...)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1);
    va_list args;
    va_start(args, format);
    ImGui::TextV(format, args);
    va_end(args);
}

} // namespace

void drawInspectorPanel(bool& open, UiFrame& ui, EditorState& state)
{
    if (!open) {
        return;
    }
    if (!ImGui::Begin(kInspectorPanel, &open)) {
        ImGui::End();
        return;
    }

    const SceneNodeInfo* selected = nullptr;
    const SceneNodeInfo* parent = nullptr;
    const std::vector<SceneNodeInfo>& nodes = ui.sceneOutline();
    for (const SceneNodeInfo& node : nodes) {
        if (node.id == state.selectedNodeId) {
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

    if (selected == nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Select an object in the Hierarchy to see its properties.");
        ImGui::PopStyleColor();
        ImGui::End();
        return;
    }

    const std::string name = selected->name.empty() ? "(unnamed)" : selected->name;
    ImGui::TextUnformatted(name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", selected->kind.c_str());
    ImGui::Separator();

    if (ImGui::BeginTable("properties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        row("Position", "%.1f, %.1f", static_cast<double>(selected->x), static_cast<double>(selected->y));
        row("Rotation", "%.1f deg", static_cast<double>(selected->rotationDegrees));
        row("Scale", "%.3f, %.3f", static_cast<double>(selected->scaleX), static_cast<double>(selected->scaleY));
        row("Size", "%.0f x %.0f", static_cast<double>(selected->width), static_cast<double>(selected->height));
        row("Draw order", "%d", selected->drawOrder);
        row("Visible", "%s", selected->visible ? "yes" : "no");
        row("Children", "%zu", selected->childCount);
        row("Parent", "%s", parent != nullptr ? (parent->name.empty() ? "(unnamed)" : parent->name.c_str()) : "-");
        row("Session id", "%llu", static_cast<unsigned long long>(selected->id));
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Live values from the running scene. Editing (with undo) arrives with "
                       "Lantern scenes in Milestone 3.");
    ImGui::PopStyleColor();

    ImGui::End();
}

} // namespace lantern::editor
