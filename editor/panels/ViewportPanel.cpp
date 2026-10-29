#include "Panels.hpp"

#include <imgui.h>

#include <cmath>
#include <cstdio>

namespace lantern::editor {

void drawViewportPanel(bool& open, UiFrame& ui)
{
    if (!open) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin(kViewportPanel, &open);
    ImGui::PopStyleVar();

    if (visible) {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const WorldImageRect rect = ui.worldImage(available.x, available.y);

        if (rect.width > 0.0f) {
            // Small caption in the image's top-left corner: resolution and zoom.
            char caption[64];
            std::snprintf(caption, sizeof caption, "%d x %d   %d%%", ui.worldWidth(), ui.worldHeight(),
                          static_cast<int>(std::lround(rect.scale * 100.0f)));
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 textPos(rect.x + 8.0f, rect.y + 6.0f);
            const ImVec2 textSize = ImGui::CalcTextSize(caption);
            draw->AddRectFilled(ImVec2(textPos.x - 4.0f, textPos.y - 2.0f),
                                ImVec2(textPos.x + textSize.x + 4.0f, textPos.y + textSize.y + 2.0f),
                                IM_COL32(0, 0, 0, 140), 3.0f);
            draw->AddText(textPos, IM_COL32(230, 230, 230, 255), caption);
        }
    }
    ImGui::End();
}

} // namespace lantern::editor
