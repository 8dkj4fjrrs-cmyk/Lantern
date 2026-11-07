#include "rolo/ui/Context.hpp"
#include "rolo/ui/DockLayout.hpp"
#include "rolo/ui/widgets/Basic.hpp"
#include "rolo/ui/widgets/DockSpace.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace rolo::ui;

namespace {

// The editor's default arrangement:
//   hierarchy | (viewport over console) | inspector
DockLayout editorLayout()
{
    DockLayout layout = DockLayout::tabs({"viewport"});
    layout.dock("hierarchy", "viewport", DockSide::Left, 0.18f);
    layout.dock("inspector", "viewport", DockSide::Right, 0.29f);
    layout.dock("console", "viewport", DockSide::Bottom, 0.3f);
    return layout;
}

std::function<bool(std::string_view)> known(std::set<std::string> ids)
{
    return [ids = std::move(ids)](std::string_view id) { return ids.count(std::string(id)) > 0; };
}

std::set<std::string> asSet(const std::vector<std::string>& v)
{
    return {v.begin(), v.end()};
}

} // namespace

TEST_CASE("Docking builds splits and tab groups", "[ui][dock]")
{
    const DockLayout layout = editorLayout();
    CHECK(asSet(layout.panels()) == std::set<std::string>{"viewport", "console", "hierarchy", "inspector"});
    CHECK(layout.root().isSplit);
    CHECK(layout.groupOf("viewport") != layout.groupOf("console"));

    DockLayout tabs = editorLayout();
    REQUIRE(tabs.dock("inspector", "hierarchy", DockSide::Center));
    CHECK(tabs.groupOf("inspector") == tabs.groupOf("hierarchy"));
    CHECK(tabs.isActive("inspector"));
    tabs.activate("hierarchy");
    CHECK(tabs.isActive("hierarchy"));
}

TEST_CASE("Removing a panel collapses its empty group", "[ui][dock]")
{
    DockLayout layout = editorLayout();
    REQUIRE(layout.remove("console"));
    CHECK_FALSE(layout.contains("console"));
    // The viewport now has the space the console had.
    std::vector<DockLayout::GroupRect> groups;
    std::vector<DockLayout::SplitterRect> splitters;
    layout.computeRects({0, 0, 1000, 600}, 4.0f, groups, splitters);
    CHECK(groups.size() == 3);
    CHECK(splitters.size() == 2);

    layout.remove("viewport");
    layout.remove("hierarchy");
    layout.remove("inspector");
    CHECK(layout.isEmpty());
    CHECK_FALSE(layout.remove("inspector"));
}

TEST_CASE("Moving a panel to another side keeps every panel exactly once", "[ui][dock]")
{
    DockLayout layout = editorLayout();
    REQUIRE(layout.dock("hierarchy", "inspector", DockSide::Top));
    CHECK(layout.panels().size() == 4);
    CHECK(asSet(layout.panels()).size() == 4);
    // Hierarchy and inspector are now stacked in the right column.
    const auto* h = layout.groupOf("hierarchy");
    const auto* i = layout.groupOf("inspector");
    REQUIRE(h->parent != nullptr);
    CHECK(h->parent == i->parent);
    CHECK(h->parent->axis == Axis::Vertical);
}

TEST_CASE("Impossible docks are refused", "[ui][dock]")
{
    DockLayout layout = editorLayout();
    CHECK_FALSE(layout.dock("console", "console", DockSide::Left)); // onto itself
    CHECK_FALSE(layout.dock("console", "nonexistent", DockSide::Left));
    CHECK(layout.panels().size() == 4);
}

TEST_CASE("Group rectangles tile the area without overlapping", "[ui][dock]")
{
    DockLayout layout = editorLayout();
    std::vector<DockLayout::GroupRect> groups;
    std::vector<DockLayout::SplitterRect> splitters;
    const Rect area{0, 0, 1200, 800};
    layout.computeRects(area, 4.0f, groups, splitters);
    REQUIRE(groups.size() == 4);
    float total = 0.0f;
    for (const auto& g : groups) {
        total += g.rect.width * g.rect.height;
        for (const auto& other : groups) {
            if (&g != &other) {
                CHECK(g.rect.intersect(other.rect).isEmpty());
            }
        }
    }
    const float gaps = 0.25f * area.width * area.height; // generous bound for the 4-point gaps
    CHECK(total > area.width * area.height - gaps);
}

TEST_CASE("Layouts survive a save and load", "[ui][dock]")
{
    DockLayout layout = editorLayout();
    layout.dock("inspector", "hierarchy", DockSide::Center);
    layout.activate("hierarchy");
    const std::string text = layout.serialize();
    CHECK(text.rfind("rolo-dock 1 ", 0) == 0);

    const auto loaded = DockLayout::parse(text, known({"viewport", "console", "hierarchy", "inspector"}));
    REQUIRE(loaded.has_value());
    CHECK(loaded->serialize() == text);
    CHECK(loaded->isActive("hierarchy"));
}

TEST_CASE("Unknown panels are dropped and bad files rejected", "[ui][dock]")
{
    const std::string text = editorLayout().serialize();
    const auto withoutConsole = DockLayout::parse(text, known({"viewport", "hierarchy", "inspector"}));
    REQUIRE(withoutConsole.has_value());
    CHECK_FALSE(withoutConsole->contains("console"));
    CHECK(withoutConsole->panels().size() == 3);

    CHECK_FALSE(DockLayout::parse("", known({"viewport"})).has_value());
    CHECK_FALSE(DockLayout::parse("rolo-dock 99 [0|viewport]", known({"viewport"})).has_value());
    CHECK_FALSE(DockLayout::parse("rolo-dock 1 (h 0.5 [0|viewport]", known({"viewport"})).has_value());
    CHECK_FALSE(DockLayout::parse("garbage", known({"viewport"})).has_value());
    CHECK_FALSE(DockLayout::parse("rolo-dock 1 [0|other]", known({"viewport"})).has_value());
}

TEST_CASE("A closed panel reopens next to its old neighbour", "[ui][dock]")
{
    const DockLayout reference = editorLayout();
    DockLayout layout = reference;
    layout.remove("console");
    layout.reopen("console", reference);
    REQUIRE(layout.contains("console"));
    const auto* console = layout.groupOf("console");
    const auto* viewport = layout.groupOf("viewport");
    REQUIRE(console->parent != nullptr);
    CHECK(console->parent == viewport->parent);
    CHECK(console->parent->axis == Axis::Vertical);
    CHECK(console->parent->second.get() == console); // below, as before

    DockLayout empty;
    empty.reopen("viewport", reference);
    CHECK(empty.contains("viewport"));
}

TEST_CASE("A reopened side panel spans its whole old side again", "[ui][dock]")
{
    const DockLayout reference = editorLayout();
    DockLayout layout = reference;
    layout.remove("hierarchy");
    layout.reopen("hierarchy", reference);
    // Back exactly where it was: the left column of the whole layout.
    CHECK(layout.serialize() == reference.serialize());
}

// --- DockSpace widget -------------------------------------------------------------------------

namespace {
struct DockHarness {
    Context ctx;
    DockSpace* dock = nullptr;
    int changes = 0;

    DockHarness()
    {
        auto space = std::make_unique<DockSpace>();
        for (const char* id : {"viewport", "console", "hierarchy", "inspector"}) {
            space->addPanel(id, std::string(1, static_cast<char>(id[0] - 32)) + (id + 1),
                            std::make_unique<Label>(id));
        }
        space->setDefaultLayout(editorLayout());
        space->setOnLayoutChanged([this] { ++changes; });
        dock = space.get();
        ctx.setRoot(std::move(space));
        ctx.setViewport({1200, 800}, 1.0f);
        ctx.render();
    }

    void drag(Vec2 from, Vec2 to)
    {
        ctx.pointerMove(from);
        ctx.pointerButton(MouseButton::Left, true, {});
        ctx.pointerMove({from.x + 10.0f, from.y + 10.0f});
        ctx.pointerMove(to);
        ctx.pointerButton(MouseButton::Left, false, {});
        ctx.render();
    }
};
} // namespace

TEST_CASE("DockSpace shows one active panel per group", "[ui][dock]")
{
    DockHarness h;
    CHECK(h.dock->findByName("") != nullptr);
    int visible = 0;
    for (const auto& child : h.dock->children()) {
        visible += child->isVisible() ? 1 : 0;
    }
    CHECK(visible == 4);
    CHECK_FALSE(h.dock->tabRect("console").isEmpty());
}

TEST_CASE("Closing a tab with its x removes the panel; View can reopen it", "[ui][dock]")
{
    DockHarness h;
    const Rect tab = h.dock->tabRect("console");
    const Vec2 close{tab.right() - 14.0f, tab.center().y};
    h.ctx.pointerMove(close);
    h.ctx.pointerButton(MouseButton::Left, true, {});
    h.ctx.pointerButton(MouseButton::Left, false, {});
    h.ctx.render();
    CHECK_FALSE(h.dock->isOpen("console"));
    CHECK(h.changes == 1);

    h.dock->setOpen("console", true);
    h.ctx.render();
    CHECK(h.dock->isOpen("console"));
}

TEST_CASE("Dragging a tab onto another group's centre makes it a tab there", "[ui][dock]")
{
    DockHarness h;
    const Rect from = h.dock->tabRect("inspector");
    const Rect target = h.dock->tabRect("hierarchy");
    // Drop in the middle of the hierarchy group's content.
    h.drag({from.x + 20.0f, from.center().y}, {target.x + 60.0f, target.bottom() + 200.0f});
    CHECK(h.dock->layout().groupOf("inspector") == h.dock->layout().groupOf("hierarchy"));
    CHECK(h.changes >= 1);
}

TEST_CASE("Dragging a tab to a group's edge splits that group", "[ui][dock]")
{
    DockHarness h;
    const Rect from = h.dock->tabRect("console");
    const Rect inspectorTab = h.dock->tabRect("inspector");
    // Near the bottom edge of the inspector group.
    h.drag({from.x + 20.0f, from.center().y}, {inspectorTab.x + 60.0f, 780.0f});
    const auto* console = h.dock->layout().groupOf("console");
    const auto* inspector = h.dock->layout().groupOf("inspector");
    REQUIRE(console != nullptr);
    REQUIRE(console->parent != nullptr);
    CHECK(console->parent == inspector->parent);
    CHECK(console->parent->axis == Axis::Vertical);
}

TEST_CASE("Dragging the gap between groups resizes them", "[ui][dock]")
{
    DockHarness h;
    const float before = h.dock->tabRect("viewport").x;
    const Rect hierarchyTab = h.dock->tabRect("hierarchy");
    // The vertical gap just right of the hierarchy group (starts ~18% across).
    const float gapX = before - 1.5f; // tabs start at the group's edge; the gap is 3 points
    REQUIRE(h.ctx.cursor() == CursorShape::Arrow);
    h.ctx.pointerMove({gapX, hierarchyTab.bottom() + 100.0f});
    h.ctx.render();
    CHECK(h.ctx.cursor() == CursorShape::ResizeHorizontal);
    h.drag({gapX, hierarchyTab.bottom() + 100.0f}, {gapX + 150.0f, hierarchyTab.bottom() + 100.0f});
    CHECK(h.dock->tabRect("viewport").x > before + 100.0f);
}

TEST_CASE("The panel menu closes the visible panel or resets the layout", "[ui][dock]")
{
    DockHarness h;
    const Rect button = h.dock->panelMenuButton("console");
    REQUIRE_FALSE(button.isEmpty());
    // Click the three-line button: a menu opens with "Close Console" first.
    h.ctx.pointerMove(button.center());
    h.ctx.pointerButton(MouseButton::Left, true, {});
    h.ctx.pointerButton(MouseButton::Left, false, {});
    h.ctx.render();
    REQUIRE(h.ctx.hasOverlays());
    h.ctx.keyDown(Key::Down, {});
    h.ctx.keyDown(Key::Enter, {});
    h.ctx.render();
    CHECK_FALSE(h.ctx.hasOverlays());
    CHECK_FALSE(h.dock->isOpen("console"));

    // Reset Layout from another group's menu brings it back.
    h.dock->openPanelMenu(h.ctx, "viewport");
    h.ctx.render();
    h.ctx.keyDown(Key::Up, {}); // last item
    h.ctx.keyDown(Key::Enter, {});
    h.ctx.render();
    CHECK(h.dock->isOpen("console"));
}
