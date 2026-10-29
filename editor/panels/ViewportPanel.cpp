#include "Panels.hpp"

#include <cmath>
#include <string>

namespace lantern::editor {

ViewportPanel::ViewportPanel()
    : ui::ImageView(0, {})
{
    setBackground(ui::Color::hex(0x0E0F12));
}

void ViewportPanel::update(UiFrame& frame)
{
    const WorldImage image = frame.worldImage();
    setTexture(image.texture, image.size);
    setUv(image.uv0, image.uv1);

    // "1280 × 720 · 75%": the game's resolution and how much it is scaled to fit.
    const int percent = static_cast<int>(std::lround(imageScale() * 100.0f));
    setCaption(std::to_string(frame.worldWidth()) + " × " + std::to_string(frame.worldHeight()) + "  ·  "
               + std::to_string(percent) + "%");
}

} // namespace lantern::editor
