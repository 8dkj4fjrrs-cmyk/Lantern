#pragma once

// Widget: the building block of Rolo UI.
//
// Widgets form a tree. Every frame the Context asks each widget how big it
// would like to be (measure), tells it where it goes (arrange), and asks it to
// draw itself (paint). Input events go to the widget under the pointer or
// with keyboard focus, and "bubble" up to its parents until one handles them.
//
// Ownership: a parent owns its children (std::unique_ptr). The Context owns
// the root and any open popups.
//
// Rule for event handlers: do not destroy widgets (including the one handling
// the event) directly inside a handler; use Context::post() to do it after the
// event has finished.

#include "rolo/ui/DrawList.hpp"
#include "rolo/ui/Input.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rolo::ui {

class Context;

class Widget {
public:
    Widget() = default;
    virtual ~Widget();

    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    // --- Tree --------------------------------------------------------------------
    Widget* parent() const noexcept { return m_parent; }
    const std::vector<std::unique_ptr<Widget>>& children() const noexcept { return m_children; }

    /// Adds a child and returns a reference to it (the parent keeps ownership).
    template <class T>
    T& addChild(std::unique_ptr<T> child)
    {
        T& ref = *child;
        adopt(std::move(child));
        return ref;
    }

    /// Creates a child in place: `auto& b = box.emplace<Button>("OK");`
    template <class T, class... Args>
    T& emplace(Args&&... args)
    {
        return addChild(std::make_unique<T>(std::forward<Args>(args)...));
    }

    /// Detaches a child and hands ownership back to the caller.
    std::unique_ptr<Widget> removeChild(Widget& child);
    void clearChildren();

    /// The Context this widget is attached to (null until it is in a tree).
    Context* context() const noexcept { return m_context; }

    /// Finds a descendant (or this) by name. Useful for tests.
    Widget* findByName(std::string_view name);

    // --- Layout --------------------------------------------------------------------
    /// The size this widget would like within `available` (which may be very
    /// large to mean "unlimited"). Default: the largest child size.
    virtual Size measure(Context& ctx, Size available);

    /// Places the widget at `rect` (window coordinates). Default: every child
    /// gets the same rectangle. Overrides must call setBounds().
    virtual void arrange(Context& ctx, Rect rect);

    const Rect& bounds() const noexcept { return m_bounds; }

    /// Share of left-over space this widget gets inside a Box (0 = none).
    float stretch() const noexcept { return m_stretch; }
    void setStretch(float stretch) { m_stretch = stretch; }

    /// Overrides the measured size along one or both axes (0 = not fixed).
    void setFixedSize(Size size) { m_fixedSize = size; }
    Size fixedSize() const noexcept { return m_fixedSize; }

    // --- Painting -------------------------------------------------------------------
    /// Default: paints visible children.
    virtual void paint(Context& ctx, DrawList& draw);
    void paintChildren(Context& ctx, DrawList& draw);

    // --- Hit testing ----------------------------------------------------------------
    /// The deepest visible widget containing `point`, or null.
    virtual Widget* hitTest(Vec2 point);

    // --- Events (return true when handled; otherwise the parent gets a turn) ----------
    virtual bool onPointerDown(Context& ctx, const PointerEvent& event);
    virtual bool onPointerUp(Context& ctx, const PointerEvent& event);
    /// Sent to the hovered widget, or to the widget holding pointer capture.
    virtual void onPointerMove(Context& ctx, const PointerEvent& event);
    virtual void onPointerEnter(Context& ctx);
    virtual void onPointerLeave(Context& ctx);
    virtual bool onScroll(Context& ctx, const ScrollEvent& event);
    virtual bool onKeyDown(Context& ctx, const KeyEvent& event);
    /// A key was let go (sent to the focused widget, then its parents, like
    /// onKeyDown). Most widgets only need onKeyDown; games need both.
    virtual bool onKeyUp(Context& ctx, const KeyEvent& event);
    /// Typed text (already UTF-8), sent to the focused widget.
    virtual bool onText(Context& ctx, std::string_view text);
    virtual void onFocusChanged(Context& ctx, bool focused);

    virtual CursorShape cursor(Vec2 point) const;

    /// Text shown after hovering for a moment (empty = none).
    virtual std::string tooltip() const { return m_tooltip; }
    void setTooltip(std::string text) { m_tooltip = std::move(text); }

    // --- State ----------------------------------------------------------------------
    bool isVisible() const noexcept { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }
    bool isEnabled() const noexcept;  // false if this or any parent is disabled
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isFocusable() const noexcept { return m_focusable; }
    void setFocusable(bool focusable) { m_focusable = focusable; }
    const std::string& name() const noexcept { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    /// Convenience queries against the attached context.
    bool isHovered() const noexcept;
    bool isFocused() const noexcept;
    bool hasPointerCapture() const noexcept;

protected:
    void setBounds(Rect rect) { m_bounds = rect; }

    /// Applies fixed-size overrides to a measured size.
    Size applyFixedSize(Size measured) const;

private:
    friend class Context;
    void adopt(std::unique_ptr<Widget> child);
    void attach(Context* context);

    Widget* m_parent = nullptr;
    Context* m_context = nullptr;
    std::vector<std::unique_ptr<Widget>> m_children;
    Rect m_bounds;
    Size m_fixedSize;
    float m_stretch = 0.0f;
    bool m_visible = true;
    bool m_enabled = true;
    bool m_focusable = false;
    std::string m_name;
    std::string m_tooltip;
};

/// Measures with "unlimited" space along both axes.
inline constexpr float kUnbounded = 1.0e9f;

} // namespace rolo::ui
