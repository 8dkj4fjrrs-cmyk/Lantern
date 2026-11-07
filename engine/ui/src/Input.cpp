#include "rolo/ui/Input.hpp"

namespace rolo::ui {

std::string primaryShortcutLabel(const Platform& platform, std::string_view key)
{
    // macOS menus show the Command symbol directly before the key (⌘Q).
    return platform.macConventions ? "⌘" + std::string(key) : "Ctrl+" + std::string(key);
}

} // namespace rolo::ui
