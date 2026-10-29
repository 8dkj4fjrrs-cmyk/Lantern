#include "Panels.hpp"

#include "lantern/ui/Context.hpp"
#include "lantern/ui/widgets/Menu.hpp"

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

void openMenuAt(ui::Context& ctx, ui::Rect anchor, std::vector<ui::MenuItem> items)
{
    ctx.openOverlay(std::make_unique<ui::MenuPopup>(std::move(items)),
                    ui::OverlayOptions{.position = {anchor.x, anchor.bottom() + 2.0f}});
}

AssetsPreviewPanel::AssetsPreviewPanel()
    : ui::Column(0.0f)
{
    auto& toolbar = makePanelToolbar(*this);
    addToolButton(toolbar, ui::Icon::ChevronLeft, std::string("Back. ") + kComingWithAssets);
    addToolButton(toolbar, ui::Icon::ChevronRight, std::string("Forward. ") + kComingWithAssets);
    toolbar.emplace<ui::IconView>(ui::Icon::Folder, 14.0f).setFixedSize({22.0f, 14.0f});
    toolbar.emplace<ui::Label>("assets", ui::Label::Style::Dim);

    emplace<ui::EmptyState>(ui::Icon::Eye, "Nothing to preview",
                            "Thumbnails of images and other assets arrive with asset import in Milestone 4.");
}

} // namespace lantern::editor
