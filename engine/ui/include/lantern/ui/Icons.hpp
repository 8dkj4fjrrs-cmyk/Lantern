#pragma once

// Small vector icons drawn with DrawList lines and shapes, so they are sharp
// at any size and need no image files.

#include "lantern/ui/DrawList.hpp"

namespace lantern::ui {

enum class Icon {
    ChevronRight,  // collapsed tree row, submenu
    ChevronDown,   // expanded tree row, dropdown
    Close,         // "x" on tabs and dialogs
    Check,         // checkbox, checked menu item
    Search,        // magnifying glass in filter fields
    Dot,           // small filled circle (status, unsaved marker)
};

/// Draws `icon` centred in `area`.
void drawIcon(DrawList& draw, Icon icon, Rect area, Color color);

} // namespace lantern::ui
