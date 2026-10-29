#include "Panels.hpp"

#include "lantern/ui/Context.hpp"

namespace lantern::editor {

ui::Row& makePanelToolbar(ui::Widget& parent)
{
    auto& toolbar = parent.emplace<ui::Row>(2.0f, ui::Insets{6.0f, 4.0f, 6.0f, 4.0f});
    toolbar.setCrossAlign(ui::CrossAlign::Center);
    parent.emplace<ui::Separator>();
    return toolbar;
}

ui::Button& addToolButton(ui::Widget& toolbar, ui::Icon icon, std::string tooltip, std::function<void()> action)
{
    auto& button = toolbar.emplace<ui::Button>("");
    button.setIcon(icon);
    button.setStyle(ui::Button::Style::Subtle);
    button.setFixedSize({26.0f, 24.0f});
    button.setTooltip(std::move(tooltip));
    if (action) {
        button.setOnClick(std::move(action));
    } else {
        button.setEnabled(false);
    }
    return button;
}

// ---------------------------------------------------------------------------
// Assets and Assets Preview: placeholders until projects exist (Milestone 3)
// ---------------------------------------------------------------------------
AssetsPanel::AssetsPanel()
    : ui::Column(0.0f)
{
    auto& toolbar = makePanelToolbar(*this);
    addToolButton(toolbar, ui::Icon::Plus, std::string("Create an asset. ") + kComingWithScenes);
    auto& search = toolbar.emplace<ui::TextField>("Search assets");
    search.setLeadingIcon(ui::Icon::Search);
    search.setStretch(1.0f);
    search.setEnabled(false);
    search.setTooltip(kComingWithScenes);
    addToolButton(toolbar, ui::Icon::Refresh, std::string("Refresh. ") + kComingWithScenes);

    emplace<ui::EmptyState>(ui::Icon::Folder, "No project open",
                            "Your game's images, sounds and scenes will be listed here once Lantern projects "
                            "arrive in Milestone 3.");
}

AssetsPreviewPanel::AssetsPreviewPanel()
    : ui::Column(0.0f)
{
    auto& toolbar = makePanelToolbar(*this);
    addToolButton(toolbar, ui::Icon::ChevronLeft, std::string("Back. ") + kComingWithScenes);
    addToolButton(toolbar, ui::Icon::ChevronRight, std::string("Forward. ") + kComingWithScenes);
    toolbar.emplace<ui::IconView>(ui::Icon::Folder, 14.0f).setFixedSize({22.0f, 14.0f});
    toolbar.emplace<ui::Label>("assets", ui::Label::Style::Dim);

    emplace<ui::EmptyState>(ui::Icon::Eye, "Nothing to preview",
                            "Select a folder in Assets to see its contents here as thumbnails.");
}

} // namespace lantern::editor
