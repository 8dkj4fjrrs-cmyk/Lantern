#include "Panels.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <string>

namespace lantern::editor {

namespace {

ImVec4 levelColour(LogLevel level)
{
    switch (level) {
    case LogLevel::Trace: return ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
    case LogLevel::Debug: return ImVec4(0.62f, 0.70f, 0.78f, 1.0f);
    case LogLevel::Info: return ImVec4(0.88f, 0.88f, 0.88f, 1.0f);
    case LogLevel::Warning: return ImVec4(1.00f, 0.78f, 0.35f, 1.0f);
    case LogLevel::Error: return ImVec4(1.00f, 0.42f, 0.40f, 1.0f);
    }
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

void levelToggle(const char* name, LogLevel level, bool& shown, const ConsoleLog& log)
{
    const std::string label = std::string(name) + " (" + std::to_string(log.count(level)) + ")";
    ImGui::PushStyleColor(ImGuiCol_Text, levelColour(level));
    ImGui::Checkbox(label.c_str(), &shown);
    ImGui::PopStyleColor();
    ImGui::SameLine();
}

} // namespace

void ConsolePanel::draw(bool& open, ConsoleLog& log)
{
    if (!open) {
        return;
    }
    if (!ImGui::Begin(kConsolePanel, &open)) {
        ImGui::End();
        return;
    }

    // --- Toolbar -----------------------------------------------------------
    if (ImGui::Button("Clear")) {
        log.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        ImGui::SetClipboardText(log.toText(m_filter).c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    levelToggle("Errors", LogLevel::Error, m_filter.showError, log);
    levelToggle("Warnings", LogLevel::Warning, m_filter.showWarning, log);
    levelToggle("Info", LogLevel::Info, m_filter.showInfo, log);
    levelToggle("Debug", LogLevel::Debug, m_filter.showDebug, log);
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##search", "Filter text", &m_filter.search);
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);

    // --- Messages ----------------------------------------------------------
    const std::vector<ConsoleEntry> entries = log.snapshot(m_filter);
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
                                  | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("messages", 4, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        // Only lay out the rows that are actually visible.
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(entries.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const ConsoleEntry& entry = entries[static_cast<std::size_t>(i)];
                const ImVec4 colour = levelColour(entry.level);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%8.2fs", entry.secondsSinceStart);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(colour, "%s", toString(entry.level).data());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextDisabled("%s", entry.category.c_str());
                ImGui::TableSetColumnIndex(3);
                // One line per row keeps every row the same height (needed by
                // the clipper); long multi-line messages show in full on hover.
                std::string_view text = entry.text;
                while (!text.empty() && (text.front() == '\n' || text.front() == '\r')) {
                    text.remove_prefix(1); // skip blank leading lines
                }
                const std::size_t newline = text.find('\n');
                ImGui::PushStyleColor(ImGuiCol_Text, colour);
                if (newline == std::string_view::npos) {
                    ImGui::TextUnformatted(text.data(), text.data() + text.size());
                } else {
                    const std::string_view firstLine = text.substr(0, newline);
                    ImGui::TextUnformatted(firstLine.data(), firstLine.data() + firstLine.size());
                    ImGui::SameLine();
                    ImGui::TextDisabled("(more...)");
                }
                ImGui::PopStyleColor();
                if (newline != std::string_view::npos && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", entry.text.c_str());
                }
            }
        }

        // Keep following new messages (unless the user turned it off).
        const std::uint64_t latest = log.totalAdded();
        if (m_autoScroll && latest != m_lastSeen) {
            m_scrollFrames = 2; // the new row's height is known one frame later
        }
        if (m_scrollFrames > 0) {
            ImGui::SetScrollY(ImGui::GetScrollMaxY());
            --m_scrollFrames;
        }
        m_lastSeen = latest;
        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace lantern::editor
