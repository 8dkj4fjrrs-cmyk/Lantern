#include "Theme.hpp"

#include <imgui.h>

namespace lantern::editor {

void applyLanternTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);

    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ScrollbarSize = 12.0f;

    const ImVec4 bg0(0.090f, 0.095f, 0.110f, 1.0f);   // deepest (docking background)
    const ImVec4 bg1(0.120f, 0.125f, 0.145f, 1.0f);   // panels
    const ImVec4 bg2(0.165f, 0.170f, 0.195f, 1.0f);   // controls
    const ImVec4 bg3(0.215f, 0.220f, 0.250f, 1.0f);   // hovered controls
    const ImVec4 accent(0.96f, 0.66f, 0.26f, 1.0f);   // lantern amber
    const ImVec4 accentDim(0.96f, 0.66f, 0.26f, 0.45f);
    const ImVec4 accentFaint(0.96f, 0.66f, 0.26f, 0.22f);
    const ImVec4 text(0.90f, 0.90f, 0.92f, 1.0f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.56f, 0.60f, 1.0f);
    c[ImGuiCol_WindowBg] = bg1;
    c[ImGuiCol_ChildBg] = bg1;
    c[ImGuiCol_PopupBg] = bg1;
    c[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.45f);
    c[ImGuiCol_FrameBg] = bg2;
    c[ImGuiCol_FrameBgHovered] = bg3;
    c[ImGuiCol_FrameBgActive] = bg3;
    c[ImGuiCol_TitleBg] = bg0;
    c[ImGuiCol_TitleBgActive] = bg0;
    c[ImGuiCol_TitleBgCollapsed] = bg0;
    c[ImGuiCol_MenuBarBg] = bg0;
    c[ImGuiCol_ScrollbarBg] = bg1;
    c[ImGuiCol_ScrollbarGrab] = bg3;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Button] = bg2;
    c[ImGuiCol_ButtonHovered] = bg3;
    c[ImGuiCol_ButtonActive] = accentDim;
    c[ImGuiCol_Header] = accentFaint;
    c[ImGuiCol_HeaderHovered] = accentDim;
    c[ImGuiCol_HeaderActive] = accentDim;
    c[ImGuiCol_Separator] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
    c[ImGuiCol_SeparatorHovered] = accentDim;
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_ResizeGrip] = accentFaint;
    c[ImGuiCol_ResizeGripHovered] = accentDim;
    c[ImGuiCol_ResizeGripActive] = accent;
    c[ImGuiCol_Tab] = bg0;
    c[ImGuiCol_TabHovered] = bg3;
    c[ImGuiCol_TabSelected] = bg1;
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = bg0;
    c[ImGuiCol_TabDimmedSelected] = bg1;
    c[ImGuiCol_TabDimmedSelectedOverline] = accentFaint;
    c[ImGuiCol_DockingPreview] = accentDim;
    c[ImGuiCol_DockingEmptyBg] = bg0;
    c[ImGuiCol_TableHeaderBg] = bg2;
    c[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);
    c[ImGuiCol_TextSelectedBg] = accentDim;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
}

} // namespace lantern::editor
