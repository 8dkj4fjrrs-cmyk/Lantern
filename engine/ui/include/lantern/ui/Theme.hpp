#pragma once

// The look of Lantern UI: colours, sizes and fonts in one place, so every
// widget is consistent and the whole look can be changed at once.

#include "lantern/ui/Font.hpp"
#include "lantern/ui/Geometry.hpp"

namespace lantern::ui {

struct Theme {
    // --- Surfaces, darkest to lightest ---------------------------------------
    Color window = Color::hex(0x141519);      // behind everything (gaps between panels)
    Color panel = Color::hex(0x1C1D22);       // panel content
    Color panelHeader = Color::hex(0x17181C); // tab strips, menu bar, status bar
    Color control = Color::hex(0x2A2C33);     // buttons, text fields
    Color controlHover = Color::hex(0x33363E);
    Color controlPressed = Color::hex(0x3B3E47);
    Color popup = Color::hex(0x24262C);       // menus, tooltips, dialogs
    Color border = Color::hex(0x2E3038);
    Color divider = Color::hex(0x0E0F12);

    // --- Text -----------------------------------------------------------------
    Color text = Color::hex(0xE6E7EB);
    Color textDim = Color::hex(0x9A9DA8);
    Color textFaint = Color::hex(0x6B6E79);
    Color textOnAccent = Color::hex(0x1A1206);

    // --- Accent (lantern amber) and states -----------------------------------
    Color accent = Color::hex(0xF2A93B);
    Color accentHover = Color::hex(0xF6BB5E);
    Color selection = Color::hex(0xF2A93B, 56);      // selected rows
    Color selectionFocused = Color::hex(0xF2A93B, 84);
    Color hover = Color::hex(0xFFFFFF, 14);          // hovered rows
    Color focusRing = Color::hex(0xF2A93B, 150);
    Color textSelection = Color::hex(0xF2A93B, 90);

    Color info = Color::hex(0xD9DBE1);
    Color debug = Color::hex(0x8FA3B8);
    Color warning = Color::hex(0xF5B94F);
    Color error = Color::hex(0xF26B65);

    // --- Fonts ------------------------------------------------------------------
    FontId font = 0;           // regular
    FontId fontStrong = 0;     // semibold, for titles and headings
    float fontSize = 13.0f;
    float smallFontSize = 11.5f;

    // --- Metrics (points) ------------------------------------------------------
    float rowHeight = 24.0f;          // list, tree and property rows
    float controlHeight = 26.0f;      // buttons, text fields
    float menuBarHeight = 28.0f;
    float tabBarHeight = 30.0f;
    float statusBarHeight = 24.0f;
    float radius = 5.0f;              // rounded corners of controls
    float popupRadius = 7.0f;
    float spacing = 8.0f;             // gap between neighbouring controls
    float padding = 10.0f;            // inside panels
    float indent = 16.0f;             // per tree level
    float splitterThickness = 4.0f;
    float scrollbarWidth = 8.0f;
};

} // namespace lantern::ui
