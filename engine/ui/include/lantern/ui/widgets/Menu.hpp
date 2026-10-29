#pragma once

// Menus: the menu bar along the top of the window and the drop-down lists it opens.

#include "lantern/ui/Widget.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace lantern::ui {

struct MenuItem {
    std::string label;
    std::string shortcut;                 // display only, e.g. "⌘Q"
    std::function<void()> action;
    std::function<bool()> isChecked;      // optional: shows a tick when true
    std::function<bool()> isEnabled;      // optional: greyed out when false
    bool separator = false;

    static MenuItem divider()
    {
        MenuItem item;
        item.separator = true;
        return item;
    }
};

struct Menu {
    std::string title;
    std::vector<MenuItem> items;
};

/// A drop-down list of menu items (opened by MenuBar, or on its own as a
/// context menu). Closes itself after an item is chosen.
class MenuPopup : public Widget {
public:
    explicit MenuPopup(std::vector<MenuItem> items);

    Size measure(Context& ctx, Size available) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    bool onPointerUp(Context& ctx, const PointerEvent& event) override;
    void onPointerMove(Context& ctx, const PointerEvent& event) override;
    void onPointerLeave(Context& ctx) override;
    bool onKeyDown(Context& ctx, const KeyEvent& event) override;

    std::optional<std::size_t> highlighted() const noexcept { return m_highlight; }

private:
    std::optional<std::size_t> itemAt(Vec2 point) const;
    float itemTop(std::size_t index) const;
    bool isEnabled(std::size_t index) const;
    void choose(Context& ctx, std::size_t index);
    void moveHighlight(int direction);

    std::vector<MenuItem> m_items;
    std::optional<std::size_t> m_highlight;
};

class MenuBar : public Widget {
public:
    MenuBar() = default;

    void addMenu(Menu menu) { m_menus.push_back(std::move(menu)); }
    /// Index of the menu whose list is showing, if any.
    std::optional<std::size_t> openMenu() const noexcept { return m_open; }
    /// Opens a menu by index (e.g. from a keyboard shortcut or a test).
    void open(Context& ctx, std::size_t index);

    Size measure(Context& ctx, Size available) override;
    void arrange(Context& ctx, Rect rect) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onPointerDown(Context& ctx, const PointerEvent& event) override;
    void onPointerMove(Context& ctx, const PointerEvent& event) override;

private:
    std::optional<std::size_t> titleAt(Vec2 point) const;

    std::vector<Menu> m_menus;
    std::vector<Rect> m_titleRects;
    std::optional<std::size_t> m_open;
    Widget* m_popup = nullptr;
};

} // namespace lantern::ui
