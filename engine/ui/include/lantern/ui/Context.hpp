#pragma once

// Context: one running instance of Lantern UI — the widget tree, popups,
// fonts, theme, focus and input state. The host creates one, feeds it input
// events, and once per frame calls render() to get a DrawList.

#include "lantern/ui/DrawList.hpp"
#include "lantern/ui/Font.hpp"
#include "lantern/ui/Input.hpp"
#include "lantern/ui/Theme.hpp"
#include "lantern/ui/Widget.hpp"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace lantern::ui {

struct OverlayOptions {
    /// Top-left position in window coordinates (ignored when `centered`).
    Vec2 position;
    bool centered = false;
    /// Modal overlays dim the window and block input to everything below.
    bool modal = false;
    /// Close when the user clicks outside it (menus, dropdowns).
    bool closeOnOutsideClick = true;
    /// Called after the overlay has been closed.
    std::function<void()> onClosed;
};

class Context {
public:
    /// Loads the built-in fonts (Inter) into the theme.
    explicit Context(Theme theme = {});
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    FontAtlas& atlas() noexcept { return *m_atlas; }
    Theme& theme() noexcept { return m_theme; }
    const Theme& theme() const noexcept { return m_theme; }
    Platform& platform() noexcept { return m_platform; }

    // --- Tree ----------------------------------------------------------------------
    void setRoot(std::unique_ptr<Widget> root);
    Widget* root() const noexcept { return m_root.get(); }

    // --- Frame -----------------------------------------------------------------------
    /// Window size in points and physical pixels per point.
    void setViewport(Size size, float pixelScale);
    Size viewport() const noexcept { return m_viewport; }
    void advanceTime(double seconds);
    double time() const noexcept { return m_time; }

    /// Lays out and paints everything. The result is valid until the next call.
    const DrawList& render();

    /// Runs `task` after the current event or frame (e.g. to destroy widgets safely).
    void post(std::function<void()> task);

    // --- Input from the host -------------------------------------------------------------
    void pointerMove(Vec2 position);
    void pointerButton(MouseButton button, bool pressed, Modifiers modifiers);
    void pointerLeftWindow();
    void scroll(Vec2 delta);
    void keyDown(Key key, Modifiers modifiers, bool repeat = false);
    void keyUp(Key key, Modifiers modifiers);
    void textInput(char32_t codepoint);

    /// The mouse cursor the host should show.
    CursorShape cursor() const noexcept { return m_cursor; }
    Vec2 pointerPosition() const noexcept { return m_pointer; }
    bool isPointerDown(MouseButton button) const noexcept;
    Modifiers modifiers() const noexcept { return m_modifiers; }

    // --- Focus, hover, capture ------------------------------------------------------------
    void setFocus(Widget* widget);
    Widget* focused() const noexcept { return m_focused; }
    /// Moves focus to the next (or previous) focusable widget, like Tab.
    void focusNext(bool backwards = false);
    Widget* hovered() const noexcept { return m_hovered; }
    Widget* captured() const noexcept { return m_captured; }
    /// Sends all pointer events to `widget` until the button is released (drags).
    void capturePointer(Widget* widget);
    void releasePointer();

    // --- Overlays: menus, popups, dialogs ---------------------------------------------------
    Widget& openOverlay(std::unique_ptr<Widget> widget, OverlayOptions options);
    /// Closes the overlay containing `widget` (deferred until after the current event).
    void closeOverlay(Widget* widget);
    void closeAllOverlays();
    bool hasOverlays() const noexcept { return !m_overlays.empty(); }
    bool hasModal() const noexcept;
    /// True if `widget` is inside an open overlay.
    bool isInOverlay(const Widget* widget) const;

    // --- Global shortcuts -------------------------------------------------------------------
    /// Runs `action` when `key` is pressed with exactly `modifiers` and no
    /// focused widget handled it.
    void addShortcut(Key key, Modifiers modifiers, std::function<void()> action);

    /// Called from ~Widget so the context never keeps a dangling pointer.
    void forget(Widget* widget) noexcept;

private:
    struct Overlay {
        std::unique_ptr<Widget> widget;
        OverlayOptions options;
        bool closing = false;
    };
    struct Shortcut {
        Key key;
        Modifiers modifiers;
        std::function<void()> action;
    };

    Widget* hitTestAll(Vec2 point) const;
    void updateHover();
    void setHovered(Widget* widget);
    void runPosted();
    void layoutOverlays();
    void paintTooltip(DrawList& draw);
    bool dispatchKey(const KeyEvent& event);
    static bool isAncestorOrSelf(const Widget* ancestor, const Widget* widget);

    Theme m_theme;
    std::unique_ptr<FontAtlas> m_atlas;
    std::unique_ptr<DrawList> m_draw;
    Platform m_platform;
    std::unique_ptr<Widget> m_root;
    std::vector<Overlay> m_overlays;
    std::vector<Shortcut> m_shortcuts;
    std::vector<std::function<void()>> m_posted;

    Size m_viewport{1280.0f, 720.0f};
    double m_time = 0.0;

    Vec2 m_pointer{-1.0f, -1.0f};
    bool m_pointerInside = false;
    std::array<bool, 3> m_buttons{};
    Modifiers m_modifiers;
    Widget* m_hovered = nullptr;
    Widget* m_focused = nullptr;
    Widget* m_captured = nullptr;
    CursorShape m_cursor = CursorShape::Arrow;

    // Click counting (double/triple click).
    Widget* m_lastClickTarget = nullptr;
    double m_lastClickTime = -10.0;
    Vec2 m_lastClickPosition;
    int m_clickCount = 0;

    // Tooltip.
    double m_hoverStart = 0.0;
    bool m_tooltipSuppressed = false;
};

} // namespace lantern::ui
