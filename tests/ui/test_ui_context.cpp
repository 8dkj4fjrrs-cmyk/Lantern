#include "rolo/ui/Context.hpp"
#include "rolo/ui/widgets/Box.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace rolo::ui;

namespace {

// A test widget with a fixed size that records what happens to it.
class Probe : public Widget {
public:
    explicit Probe(Size size, bool focusable = false)
        : m_size(size)
    {
        setFocusable(focusable);
    }

    Size measure(Context&, Size) override { return applyFixedSize(m_size); }

    bool onPointerDown(Context&, const PointerEvent& e) override
    {
        ++downs;
        lastClickCount = e.clickCount;
        return handles;
    }
    bool onPointerUp(Context&, const PointerEvent&) override
    {
        ++ups;
        return handles;
    }
    void onPointerMove(Context&, const PointerEvent& e) override { lastMove = e.position; }
    void onPointerEnter(Context&) override { ++enters; }
    void onPointerLeave(Context&) override { ++leaves; }
    bool onKeyDown(Context&, const KeyEvent& e) override
    {
        keys.push_back(e.key);
        return handlesKeys;
    }
    bool onKeyUp(Context&, const KeyEvent& e) override
    {
        released.push_back(e.key);
        return handlesKeys;
    }
    bool onText(Context&, std::string_view t) override
    {
        text += t;
        return true;
    }
    bool onScroll(Context&, const ScrollEvent& e) override
    {
        scrolled += e.delta.y;
        return true;
    }

    Size m_size;
    bool handles = true;
    bool handlesKeys = true;
    int downs = 0;
    int ups = 0;
    int enters = 0;
    int leaves = 0;
    int lastClickCount = 0;
    Vec2 lastMove;
    std::vector<Key> keys;
    std::vector<Key> released;
    std::string text;
    float scrolled = 0.0f;
};

struct Fixture {
    Context ctx;
    Column* column = nullptr;
    Probe* a = nullptr;
    Probe* b = nullptr;

    Fixture()
    {
        auto root = std::make_unique<Column>(10.0f, Insets::all(5.0f));
        column = root.get();
        a = &root->emplace<Probe>(Size{100, 30}, true);
        b = &root->emplace<Probe>(Size{100, 30}, true);
        ctx.setRoot(std::move(root));
        ctx.setViewport({400, 300}, 1.0f);
        ctx.render();
    }

    void click(Vec2 p)
    {
        ctx.pointerMove(p);
        ctx.pointerButton(MouseButton::Left, true, {});
        ctx.pointerButton(MouseButton::Left, false, {});
    }
};

} // namespace

TEST_CASE("Column places children top to bottom with padding and spacing", "[ui][layout]")
{
    Fixture f;
    CHECK(f.a->bounds() == Rect{5, 5, 390, 30});  // stretched across by default
    CHECK(f.b->bounds() == Rect{5, 45, 390, 30});
}

TEST_CASE("Stretchy children share the left-over space", "[ui][layout]")
{
    Context ctx;
    auto row = std::make_unique<Row>();
    auto& fixed = row->emplace<Probe>(Size{100, 20});
    auto& one = row->emplace<Probe>(Size{0, 20});
    one.setStretch(1.0f);
    auto& three = row->emplace<Probe>(Size{0, 20});
    three.setStretch(3.0f);
    ctx.setRoot(std::move(row));
    ctx.setViewport({500, 100}, 1.0f);
    ctx.render();
    CHECK(fixed.bounds().width == 100.0f);
    CHECK(one.bounds().width == 100.0f);
    CHECK(three.bounds().width == 300.0f);
    CHECK(three.bounds().x == 200.0f);
}

TEST_CASE("Clicking focuses, captures and delivers down and up", "[ui][input]")
{
    Fixture f;
    f.click({20, 20});
    CHECK(f.a->downs == 1);
    CHECK(f.a->ups == 1);
    CHECK(f.ctx.focused() == f.a);
    CHECK(f.ctx.captured() == nullptr);

    f.click({20, 60});
    CHECK(f.ctx.focused() == f.b);
}

TEST_CASE("While a button is held, moves go to the pressed widget even outside it", "[ui][input]")
{
    Fixture f;
    f.ctx.pointerMove({20, 20});
    f.ctx.pointerButton(MouseButton::Left, true, {});
    CHECK(f.ctx.captured() == f.a);
    f.ctx.pointerMove({350, 250}); // far away
    CHECK(f.a->lastMove == Vec2{350, 250});
    f.ctx.pointerButton(MouseButton::Left, false, {});
    CHECK(f.a->ups == 1);
    CHECK(f.ctx.captured() == nullptr);
}

TEST_CASE("Unhandled events bubble up to the parent", "[ui][input]")
{
    Fixture f;
    f.a->handles = false;
    f.click({20, 20});
    CHECK(f.a->downs == 1);
    CHECK(f.ctx.captured() == nullptr); // nobody handled it, so nobody captured
}

TEST_CASE("Hover enter and leave", "[ui][input]")
{
    Fixture f;
    f.ctx.pointerMove({20, 20});
    CHECK(f.ctx.hovered() == f.a);
    CHECK(f.a->enters == 1);
    f.ctx.pointerMove({20, 60});
    CHECK(f.a->leaves == 1);
    CHECK(f.b->enters == 1);
    f.ctx.pointerLeftWindow();
    CHECK(f.ctx.hovered() == nullptr);
}

TEST_CASE("Double and triple clicks are counted", "[ui][input]")
{
    Fixture f;
    f.click({20, 20});
    f.ctx.advanceTime(0.1);
    f.click({21, 20});
    CHECK(f.a->lastClickCount == 2);
    f.ctx.advanceTime(0.1);
    f.click({21, 21});
    CHECK(f.a->lastClickCount == 3);
    f.ctx.advanceTime(1.0); // too slow
    f.click({21, 21});
    CHECK(f.a->lastClickCount == 1);
}

TEST_CASE("Keys and text go to the focused widget; Tab moves focus", "[ui][input]")
{
    Fixture f;
    f.ctx.setFocus(f.a);
    f.ctx.keyDown(Key::A, {});
    f.ctx.textInput(U'é');
    CHECK(f.a->keys == std::vector<Key>{Key::A});
    CHECK(f.a->text == "é");

    f.a->handlesKeys = false;
    f.ctx.keyDown(Key::Tab, {});
    CHECK(f.ctx.focused() == f.b);
    f.b->handlesKeys = false;
    f.ctx.keyDown(Key::Tab, {Modifiers{.shift = true}});
    CHECK(f.ctx.focused() == f.a);
}

TEST_CASE("Global shortcuts run when nothing focused handles the key", "[ui][input]")
{
    Fixture f;
    int quits = 0;
    f.ctx.addShortcut(Key::Q, Modifiers{.super = true}, [&] { ++quits; });
    f.ctx.keyDown(Key::Q, Modifiers{.super = true});
    CHECK(quits == 1);
    f.ctx.keyDown(Key::Q, Modifiers{}); // different modifiers: no match
    CHECK(quits == 1);
}

TEST_CASE("Scrolling goes to the widget under the pointer", "[ui][input]")
{
    Fixture f;
    f.ctx.pointerMove({20, 60});
    f.ctx.scroll({0, 12});
    CHECK(f.b->scrolled == 12.0f);
}

TEST_CASE("Popups: open on top, close on outside click, then disappear", "[ui][overlay]")
{
    Fixture f;
    bool closed = false;
    auto popup = std::make_unique<Probe>(Size{80, 40});
    Probe* raw = popup.get();
    f.ctx.openOverlay(std::move(popup), OverlayOptions{.position = {200, 100}, .onClosed = [&] { closed = true; }});
    f.ctx.render();
    CHECK(raw->bounds() == Rect{200, 100, 80, 40});

    f.click({210, 110}); // inside: delivered to the popup
    CHECK(raw->downs == 1);

    const int aDownsBefore = f.a->downs;
    f.click({20, 20});   // outside: closes the popup and is swallowed
    CHECK(closed);
    CHECK_FALSE(f.ctx.hasOverlays());
    CHECK(f.a->downs == aDownsBefore);
}

TEST_CASE("Popups are kept inside the window", "[ui][overlay]")
{
    Fixture f;
    auto popup = std::make_unique<Probe>(Size{80, 40});
    Probe* raw = popup.get();
    f.ctx.openOverlay(std::move(popup), OverlayOptions{.position = {390, 290}});
    f.ctx.render();
    CHECK(raw->bounds().right() <= 400.0f);
    CHECK(raw->bounds().bottom() <= 300.0f);
}

TEST_CASE("Modal dialogs block everything below and close with Escape", "[ui][overlay]")
{
    Fixture f;
    auto dialog = std::make_unique<Probe>(Size{100, 60});
    dialog->handlesKeys = false;
    f.ctx.openOverlay(std::move(dialog), OverlayOptions{.centered = true, .modal = true, .closeOnOutsideClick = false});
    f.ctx.render();
    CHECK(f.ctx.hasModal());

    f.click({20, 20}); // on widget a, but the modal blocks it
    CHECK(f.a->downs == 0);
    CHECK(f.ctx.hasOverlays());

    f.ctx.keyDown(Key::Escape, {});
    CHECK_FALSE(f.ctx.hasOverlays());
}

TEST_CASE("Destroyed widgets are forgotten by the context", "[ui][lifetime]")
{
    Fixture f;
    f.ctx.pointerMove({20, 20});
    f.ctx.setFocus(f.a);
    REQUIRE(f.ctx.hovered() == f.a);
    f.column->clearChildren(); // destroys a and b
    CHECK(f.ctx.hovered() == nullptr);
    CHECK(f.ctx.focused() == nullptr);
    f.ctx.render(); // must not touch freed memory (run under ASan in the asan preset)
}

TEST_CASE("Rendering produces a draw list covering the viewport", "[ui][render]")
{
    Fixture f;
    const DrawList& draw = f.ctx.render();
    REQUIRE_FALSE(draw.commands().empty());
    CHECK(draw.commands()[0].clip == Rect{0, 0, 400, 300});
}

TEST_CASE("Key releases go to the focused widget", "[ui][input]")
{
    Fixture f;
    f.ctx.keyUp(Key::A, {});   // nothing focused: nobody hears it
    f.click({20, 20});
    REQUIRE(f.ctx.focused() == f.a);
    f.ctx.keyDown(Key::Right, {});
    f.ctx.keyUp(Key::Right, {});
    f.ctx.keyDown(Key::LeftShift, {});
    CHECK(f.a->keys == std::vector<Key>{Key::Right, Key::LeftShift});
    CHECK(f.a->released == std::vector<Key>{Key::Right});
    CHECK(f.b->released.empty());
}
