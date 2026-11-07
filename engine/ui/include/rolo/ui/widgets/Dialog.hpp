#pragma once

// Dialog: a modal window with a title, content and buttons.
//
//     auto dialog = std::make_unique<Dialog>("About Rolo Engine");
//     dialog->body().emplace<Label>("Version 0.2.0");
//     dialog->addButton("Close", Button::Style::Primary);   // closes by default
//     showDialog(ctx, std::move(dialog));
//
// Escape closes it; Enter presses the primary button.

#include "rolo/ui/widgets/Basic.hpp"
#include "rolo/ui/widgets/Box.hpp"

#include <functional>
#include <string>

namespace rolo::ui {

class Dialog : public Widget {
public:
    explicit Dialog(std::string title, float width = 380.0f);

    /// Put the dialog's content in here.
    Column& body() noexcept { return *m_body; }

    /// Adds a button at the bottom right. Its action runs, then the dialog closes.
    Button& addButton(std::string text, Button::Style style = Button::Style::Normal,
                      std::function<void()> action = {});

    /// Runs when the dialog closes for any reason: a button, Escape, or the
    /// program closing it. (Set it before showDialog().)
    void setOnClosed(std::function<void()> onClosed) { m_onClosed = std::move(onClosed); }
    const std::function<void()>& onClosed() const noexcept { return m_onClosed; }

    Size measure(Context& ctx, Size available) override;
    void arrange(Context& ctx, Rect rect) override;
    void paint(Context& ctx, DrawList& draw) override;
    bool onKeyDown(Context& ctx, const KeyEvent& event) override;

private:
    std::function<void()> m_onClosed;
    std::string m_title;
    float m_width;
    Column* m_body = nullptr;
    Row* m_buttons = nullptr;
    Button* m_primary = nullptr;
};

/// Opens `dialog` centred and modal. Returns a reference to it.
Dialog& showDialog(Context& ctx, std::unique_ptr<Dialog> dialog);

} // namespace rolo::ui
