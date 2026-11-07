#include "Panels.hpp"

#include "rolo/ui/Context.hpp"
#include "rolo/ui/widgets/Menu.hpp"

namespace rolo::editor {

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

bool editAnimationSet(EditorServices& editor, AssetId id, const std::string& label,
                      const std::function<void(AnimationSet&)>& change, bool merge)
{
    AssetDatabase* db = editor.assets();
    const AssetRecord* asset = db != nullptr ? db->find(id) : nullptr;
    if (asset == nullptr) {
        return false;
    }
    if (!asset->animation) {
        editor.showMessage("Cannot Edit", "The animation file has errors that must be fixed first: " + asset->problem);
        return false;
    }
    AnimationSet changed = *asset->animation;
    change(changed);
    if (changed == *asset->animation) {
        return false;
    }
    const std::string path = asset->path;
    if (!merge) {
        editor.endEdit();
    }
    const bool done = editor.execute(std::make_unique<ChangeAssetText>(id, serializeAnimationSet(changed), label));
    if (!merge) {
        editor.endEdit();
    }
    if (!done) {
        editor.showMessage("Could Not Save the Animations",
                           "Rolo could not write " + path + ". Check that the file is not read-only.");
    }
    return done;
}

} // namespace rolo::editor
