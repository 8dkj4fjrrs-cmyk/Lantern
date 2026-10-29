#include "Panels.hpp"

#include <cmath>
#include <string>

namespace lantern::editor {

ViewportPanel::ViewportPanel()
    : ui::Column(0.0f)
{
    // Tool strip: the scene editing tools, then the game's resolution and zoom.
    auto& toolbar = makePanelToolbar(*this);
    addToolButton(toolbar, ui::Icon::Move, std::string("Move tool. ") + kComingWithEditing);
    addToolButton(toolbar, ui::Icon::Rotate, std::string("Rotate tool. ") + kComingWithEditing);
    addToolButton(toolbar, ui::Icon::Scale, std::string("Scale tool. ") + kComingWithEditing);
    addToolButton(toolbar, ui::Icon::RectTool, std::string("Rect tool. ") + kComingWithEditing);
    toolbar.emplace<ui::Spacer>();
    m_zoom = &toolbar.emplace<ui::Label>("", ui::Label::Style::Dim);
    m_zoom->setTooltip("The game's resolution, and how much it is scaled to fit this panel. "
                       "Zooming and panning arrive in Milestone 4.");
    toolbar.emplace<ui::Spacer>(0.0f).setFixedSize({6.0f, 1.0f});

    auto image = std::make_unique<ui::ImageView>(0, ui::Size{});
    image->setBackground(ui::Color::hex(0x111111));
    image->setStretch(1.0f);
    m_image = &addChild(std::move(image));
}

void ViewportPanel::update(UiFrame& frame)
{
    const WorldImage image = frame.worldImage();
    m_image->setTexture(image.texture, image.size);
    m_image->setUv(image.uv0, image.uv1);

    // "1280 × 720 · 75%": the game's resolution and how much it is scaled to fit.
    const int percent = static_cast<int>(std::lround(m_image->imageScale() * 100.0f));
    m_zoom->setText(std::to_string(frame.worldWidth()) + " × " + std::to_string(frame.worldHeight())
                    + "   ·   " + std::to_string(percent) + "%");
}

} // namespace lantern::editor
