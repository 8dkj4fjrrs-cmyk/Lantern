#pragma once

// The look of Rolo UI: colours, sizes and fonts in one place, so every
// widget is consistent and the whole look can be changed at once.

#include "rolo/ui/Font.hpp"
#include "rolo/ui/Geometry.hpp"

namespace rolo::ui {

struct Theme {
    // --- Surfaces, darkest to lightest ---------------------------------------
    // Neutral greys, laid out like a desktop game editor: dark tab strips,
    // lighter panel content, and input fields darker than the panel they sit on.
    Color window = Color::hex(0x141414);      // behind everything (gaps between panels)
    Color panel = Color::hex(0x272727);       // panel content
    Color panelHeader = Color::hex(0x1E1E1E); // tab strips, menu bar, toolbars, status bar
    Color control = Color::hex(0x383838);     // buttons
    Color controlHover = Color::hex(0x424242);
    Color controlPressed = Color::hex(0x4C4C4C);
    Color field = Color::hex(0x1B1B1B);       // text fields and value boxes
    Color fieldHover = Color::hex(0x202020);
    Color popup = Color::hex(0x2F2F2F);       // menus, tooltips, dialogs
    Color border = Color::hex(0x3A3A3A);
    Color divider = Color::hex(0x151515);

    // --- Text -----------------------------------------------------------------
    Color text = Color::hex(0xDDDDDD);
    Color textDim = Color::hex(0xA0A0A0);
    Color textFaint = Color::hex(0x6E6E6E);
    Color textOnAccent = Color::hex(0x1A1206);

    // --- Accent (warm orange) and states -----------------------------------
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
    float rowHeight = 22.0f;          // list, tree and property rows
    float controlHeight = 24.0f;      // buttons, text fields
    float menuBarHeight = 26.0f;
    float tabBarHeight = 28.0f;
    float statusBarHeight = 24.0f;
    float radius = 3.0f;              // rounded corners of controls
    float popupRadius = 5.0f;
    float spacing = 8.0f;             // gap between neighbouring controls
    float padding = 10.0f;            // inside panels
    float indent = 16.0f;             // per tree level
    float splitterThickness = 4.0f;
    float toolbarHeight = 34.0f;      // the editor's main toolbar; panel toolbars use controlHeight + 8
    float scrollbarWidth = 8.0f;
};

} // namespace rolo::ui
