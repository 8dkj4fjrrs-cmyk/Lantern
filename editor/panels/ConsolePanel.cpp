#include "Panels.hpp"

#include "rolo/ui/Context.hpp"
#include "rolo/ui/Icons.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace rolo::editor {

namespace {

// Column widths (points); the message column takes the rest. In a narrow
// panel the Time, then the Source column are hidden to leave the message room.
struct Columns {
    float time = 72.0f;
    float level = 64.0f;
    float source = 96.0f;
};

Columns columnsFor(float width)
{
    Columns c;
    if (width < 560.0f) {
        c.time = 0.0f;
    }
    if (width < 420.0f) {
        c.source = 0.0f;
    }
    return c;
}

ui::Color levelColour(const ui::Theme& t, LogLevel level)
{
    switch (level) {
    case LogLevel::Trace: return t.textFaint;
    case LogLevel::Debug: return t.debug;
    case LogLevel::Info: return t.info;
    case LogLevel::Warning: return t.warning;
    case LogLevel::Error: return t.error;
    }
    return t.text;
}

std::string_view levelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Trace: return "Trace";
    case LogLevel::Debug: return "Debug";
    case LogLevel::Info: return "Info";
    case LogLevel::Warning: return "Warning";
    case LogLevel::Error: return "Error";
    }
    return "";
}

// Top of a line of text centred vertically in `rect`.
float textTop(ui::Context& ctx, float size, const ui::Rect& rect)
{
    const ui::FontMetrics fm = ctx.atlas().metrics(ctx.theme().font, size);
    return std::round(rect.y + (rect.height - fm.lineHeight) * 0.5f);
}

} // namespace

ConsolePanel::ConsolePanel(ConsoleLog& log)
    : ui::Column(0.0f)
    , m_log(log)
    , m_toggles{{
          {LogLevel::Error, "Errors", &ConsoleFilter::showError},
          {LogLevel::Warning, "Warnings", &ConsoleFilter::showWarning},
          {LogLevel::Info, "Info", &ConsoleFilter::showInfo},
          {LogLevel::Debug, "Debug", &ConsoleFilter::showDebug},
      }}
{
    const ui::Theme defaults;

    // --- Toolbar -------------------------------------------------------------
    // A Flow, so the toolbar wraps onto a second line in a narrow panel.
    auto& toolbar = emplace<ui::Flow>(defaults.spacing, 6.0f, ui::Insets::symmetric(defaults.padding, 6.0f));

    auto& clear = toolbar.emplace<ui::Button>("Clear", [this] {
        m_log.clear();
        m_dirty = true;
    });
    clear.setStyle(ui::Button::Style::Subtle);
    clear.setTooltip("Remove all messages");

    auto& copy = toolbar.emplace<ui::Button>("Copy", [this] {
        if (ui::Context* ctx = context(); ctx != nullptr && ctx->platform().setClipboard) {
            ctx->platform().setClipboard(m_log.toText(m_filter));
        }
    });
    copy.setStyle(ui::Button::Style::Subtle);
    copy.setTooltip("Copy the shown messages as text");

    toolbar.emplace<ui::Separator>(ui::Axis::Vertical).setFixedSize({1.0f, 16.0f});

    for (LevelToggle& toggle : m_toggles) {
        auto& box = toolbar.emplace<ui::Checkbox>(toggle.name, m_filter.*toggle.shown);
        box.setTextColor(levelColour(defaults, toggle.level));
        box.setOnChange([this, &toggle](bool shown) {
            m_filter.*toggle.shown = shown;
            m_dirty = true;
        });
        toggle.checkbox = &box;
    }

    toolbar.emplace<ui::Spacer>();

    auto& search = toolbar.emplace<ui::TextField>("Filter messages");
    search.setLeadingIcon(ui::Icon::Search);
    search.setClearButton(true);
    search.setFixedSize({200.0f, 0.0f});
    search.setOnChange([this](const std::string& text) {
        m_filter.search = text;
        m_dirty = true;
    });

    m_autoScroll = &toolbar.emplace<ui::Checkbox>("Auto-scroll", true, [this](bool on) {
        m_list->setStickToEnd(on);
        if (on) {
            m_list->scroll().scrollToEnd();
        }
    });
    m_autoScroll->setTooltip("Keep the newest message in view");

    emplace<ui::Separator>();

    // --- Messages ------------------------------------------------------------
    auto& list = emplace<ui::ListView>();
    list.setStretch(1.0f);
    list.setStickToEnd(true);
    list.setSelectable(true);
    list.setRowHeight(22.0f);
    list.setEmptyText("No messages.");
    list.setHeader(24.0f, [this](ui::Context& ctx, ui::DrawList& draw, ui::Rect rect) { paintHeader(ctx, draw, rect); });
    list.setRowPainter([this](ui::Context& ctx, ui::DrawList& draw, const ui::RowInfo& row) { paintRow(ctx, draw, row); });
    m_list = &list;
}

void ConsolePanel::update(UiFrame& /*frame*/)
{
    // Level counts in the toggles ("Warnings 2").
    for (LevelToggle& toggle : m_toggles) {
        toggle.checkbox->setText(std::string(toggle.name) + "  " + std::to_string(m_log.count(toggle.level)));
    }
    refresh();
}

void ConsolePanel::refresh()
{
    // Rebuild the shown list only when something changed.
    const std::uint64_t total = m_log.totalAdded();
    const std::size_t size = m_log.size();
    if (!m_dirty && total == m_shownTotal && size == m_shownSize) {
        return;
    }
    m_entries = m_log.snapshot(m_filter);
    m_shownTotal = total;
    m_shownSize = size;
    m_dirty = false;
    m_list->setRowCount(m_entries.size());
}

void ConsolePanel::paintHeader(ui::Context& ctx, ui::DrawList& draw, ui::Rect rect) const
{
    const ui::Theme& t = ctx.theme();
    draw.fillRect(rect, t.panelHeader);
    draw.fillRect({rect.x, rect.bottom() - 1.0f, rect.width, 1.0f}, t.divider);

    const float size = t.smallFontSize;
    const float y = textTop(ctx, size, rect);
    const Columns columns = columnsFor(rect.width);
    float x = rect.x + t.padding;
    const auto column = [&](std::string_view title, float width) {
        if (width > 0.0f) {
            draw.text(t.fontStrong, size, {x, y}, t.textDim, title);
            x += width;
        }
    };
    column("Time", columns.time);
    column("Level", columns.level);
    column("Source", columns.source);
    column("Message", 1.0f);
}

void ConsolePanel::paintRow(ui::Context& ctx, ui::DrawList& draw, const ui::RowInfo& row) const
{
    if (row.index >= m_entries.size()) {
        return;
    }
    const ConsoleEntry& entry = m_entries[row.index];
    const ui::Theme& t = ctx.theme();
    const ui::Color colour = levelColour(t, entry.level);
    const float size = t.fontSize;
    const float y = textTop(ctx, size, row.rect);
    const Columns columns = columnsFor(row.rect.width);
    float x = row.rect.x + t.padding;

    if (columns.time > 0.0f) {
        char time[32];
        std::snprintf(time, sizeof time, "%.2fs", entry.secondsSinceStart);
        draw.textEllipsized(t.font, t.smallFontSize, {x, textTop(ctx, t.smallFontSize, row.rect)}, t.textFaint, time,
                            columns.time - 8.0f);
        x += columns.time;
    }

    draw.textEllipsized(t.fontStrong, size, {x, y}, colour, levelName(entry.level), columns.level - 8.0f);
    x += columns.level;

    if (columns.source > 0.0f) {
        draw.textEllipsized(t.font, size, {x, y}, t.textDim, entry.category, columns.source - 8.0f);
        x += columns.source;
    }

    // One line per row; a multi-line message shows its first line and how
    // many more there are (Copy gives the full text).
    std::string_view text = entry.text;
    while (!text.empty() && (text.front() == '\n' || text.front() == '\r')) {
        text.remove_prefix(1);
    }
    const std::size_t newline = text.find('\n');
    std::string_view first = text.substr(0, newline);
    if (!first.empty() && first.back() == '\r') {
        first.remove_suffix(1);
    }
    const float available = row.rect.right() - x - t.padding;
    const ui::Color messageColour = entry.level == LogLevel::Info ? t.text : colour;
    const float drawn = draw.textEllipsized(t.font, size, {x, y}, messageColour, first, available);
    if (newline != std::string_view::npos) {
        std::size_t extra = 0;
        for (std::size_t i = newline; i < text.size(); ++i) {
            extra += text[i] == '\n' ? 1u : 0u;
        }
        const std::string more = "  +" + std::to_string(extra) + (extra == 1 ? " line" : " lines");
        draw.textEllipsized(t.font, t.smallFontSize, {x + drawn, textTop(ctx, t.smallFontSize, row.rect)}, t.textFaint,
                            more, std::max(0.0f, available - drawn));
    }
}

} // namespace rolo::editor
