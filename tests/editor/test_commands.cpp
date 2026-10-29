#include "Commands.hpp"
#include "SceneDocument.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace lantern;
using namespace lantern::editor;

namespace {

struct Fixture {
    SceneDocument doc{Scene("Test", EntityIdGenerator(9))};
    Scene& scene() { return const_cast<Scene&>(doc.scene()); }

    EntityId make(const char* name, EntityId parent = {})
    {
        auto cmd = std::make_unique<CreateEntity>(name, parent, std::nullopt);
        CreateEntity* raw = cmd.get();
        REQUIRE(doc.execute(std::move(cmd)));
        return raw->id();
    }
};

std::string serialized(const Scene& scene)
{
    return serializeScene(scene);
}

} // namespace

TEST_CASE("Creating can be undone and redone with the same ID", "[editor][commands]")
{
    Fixture f;
    const EntityId id = f.make("Player");
    CHECK(f.doc.isDirty());
    CHECK(f.doc.history().undoLabel() == "Create Player");
    REQUIRE(f.doc.undo());
    CHECK(f.doc.scene().empty());
    CHECK_FALSE(f.doc.isDirty());
    REQUIRE(f.doc.redo());
    CHECK(f.doc.scene().contains(id)); // same ID: later commands still refer to it
}

TEST_CASE("Deleting restores the whole subtree, IDs and order", "[editor][commands]")
{
    Fixture f;
    const EntityId a = f.make("A");
    const EntityId b = f.make("B");
    const EntityId child = f.make("Child", b);
    f.make("C");
    f.doc.execute(std::make_unique<SetProperty>(child, "lantern.Transform", "rotation", 45.0f, "Rotation"));
    const std::string before = serialized(f.doc.scene());

    REQUIRE(f.doc.execute(std::make_unique<DeleteEntity>(b)));
    CHECK_FALSE(f.doc.scene().contains(child));
    CHECK(f.doc.history().undoLabel() == "Delete B");
    REQUIRE(f.doc.undo());
    CHECK(serialized(f.doc.scene()) == before);
    CHECK(f.doc.scene().roots()[1] == b);
    static_cast<void>(a);
}

TEST_CASE("Duplicating copies children with new IDs, right after the original", "[editor][commands]")
{
    Fixture f;
    const EntityId a = f.make("A");
    f.make("Inner", a);
    f.make("Z");
    auto dup = std::make_unique<DuplicateEntity>(a);
    DuplicateEntity* raw = dup.get();
    REQUIRE(f.doc.execute(std::move(dup)));
    const EntityId copy = raw->copyId();
    REQUIRE(f.doc.scene().contains(copy));
    CHECK(copy != a);
    CHECK(f.doc.scene().roots()[1] == copy);
    CHECK(f.doc.scene().find(copy)->children().size() == 1);

    f.doc.undo();
    CHECK_FALSE(f.doc.scene().contains(copy));
    f.doc.redo();
    CHECK(f.doc.scene().contains(copy)); // the same copy comes back
}

TEST_CASE("Property edits undo to the original value and merge while dragging", "[editor][commands]")
{
    Fixture f;
    const EntityId id = f.make("Box");
    f.doc.history().breakMerge();
    for (float x : {1.0f, 2.0f, 3.0f}) {
        f.doc.execute(std::make_unique<SetProperty>(id, "lantern.Transform", "position", Vec2{x, 0}, "Position"));
    }
    CHECK(f.doc.scene().find(id)->transform().position.x == 3.0f);
    CHECK(f.doc.history().size() == 2); // create + one merged edit
    f.doc.undo();
    CHECK(f.doc.scene().find(id)->transform().position.x == 0.0f);

    // After a break, the next edit is its own step.
    f.doc.redo();
    f.doc.history().breakMerge();
    f.doc.execute(std::make_unique<SetProperty>(id, "lantern.Transform", "position", Vec2{9, 0}, "Position"));
    CHECK(f.doc.history().size() == 3);
}

TEST_CASE("Edits that change nothing make no undo step", "[editor][commands]")
{
    Fixture f;
    const EntityId id = f.make("Box");
    CHECK_FALSE(f.doc.execute(std::make_unique<RenameEntity>(id, "Box")));
    CHECK_FALSE(f.doc.execute(std::make_unique<SetProperty>(id, "lantern.Transform", "rotation", 0.0f, "Rotation")));
    CHECK_FALSE(f.doc.execute(std::make_unique<SetEntityActive>(id, true)));
    CHECK_FALSE(f.doc.execute(std::make_unique<SetProperty>(id, "lantern.Shape", "size", Vec2{}, "Size"))); // no Shape
    CHECK(f.doc.history().size() == 1);
}

TEST_CASE("Components can be added and removed, and put back in place", "[editor][commands]")
{
    Fixture f;
    const EntityId id = f.make("Box");
    REQUIRE(f.doc.execute(std::make_unique<AddComponent>(id, std::make_unique<Shape>())));
    f.doc.execute(std::make_unique<SetProperty>(id, "lantern.Shape", "color", Color{1, 2, 3, 4}, "Color"));
    REQUIRE(f.doc.execute(std::make_unique<RemoveComponent>(id, "lantern.Shape")));
    CHECK(f.doc.scene().find(id)->get<Shape>() == nullptr);
    CHECK_FALSE(f.doc.execute(std::make_unique<RemoveComponent>(id, "lantern.Transform"))); // required
    f.doc.undo();
    REQUIRE(f.doc.scene().find(id)->get<Shape>() != nullptr);
    CHECK(f.doc.scene().find(id)->get<Shape>()->color == Color{1, 2, 3, 4}); // values come back too
    CHECK(f.doc.scene().componentIndex(id, "lantern.Shape") == 1u);
}

TEST_CASE("Renaming, hiding and moving undo cleanly", "[editor][commands]")
{
    Fixture f;
    const EntityId a = f.make("A");
    const EntityId b = f.make("B");
    f.doc.execute(std::make_unique<RenameEntity>(a, "Hero"));
    f.doc.execute(std::make_unique<SetEntityActive>(a, false));
    f.doc.execute(std::make_unique<MoveEntity>(b, EntityId{}, 0));
    CHECK(f.doc.scene().roots().front() == b);
    CHECK(f.doc.scene().find(a)->name() == "Hero");
    CHECK_FALSE(f.doc.scene().find(a)->isActive());
    f.doc.undo();
    f.doc.undo();
    f.doc.undo();
    CHECK(f.doc.scene().roots().front() == a);
    CHECK(f.doc.scene().find(a)->name() == "A");
    CHECK(f.doc.scene().find(a)->isActive());
    CHECK_FALSE(f.doc.execute(std::make_unique<MoveEntity>(a, a, 0))); // not inside itself
}

TEST_CASE("A new command clears the redo history", "[editor][commands]")
{
    Fixture f;
    f.make("A");
    f.make("B");
    f.doc.undo();
    CHECK(f.doc.history().canRedo());
    f.make("C");
    CHECK_FALSE(f.doc.history().canRedo());
}

TEST_CASE("The document knows when it has unsaved changes", "[editor][document]")
{
    const auto dir = std::filesystem::temp_directory_path() / "lantern_document_test";
    std::filesystem::remove_all(dir);
    Fixture f;
    CHECK(f.doc.isUntitled());
    CHECK(f.doc.displayName() == "Untitled");
    CHECK_FALSE(f.doc.save()); // no path yet
    f.make("A");
    REQUIRE(f.doc.saveAs(dir / "level.lscene"));
    CHECK_FALSE(f.doc.isDirty());
    CHECK(f.doc.displayName() == "level.lscene");

    f.make("B");
    CHECK(f.doc.isDirty());
    f.doc.undo();
    CHECK_FALSE(f.doc.isDirty()); // back at the saved state

    // The saved state can no longer be reached once history moves on without it.
    f.doc.undo();
    f.make("C");
    CHECK(f.doc.isDirty());

    std::vector<FileIssue> issues;
    const auto reopened = SceneDocument::open(dir / "level.lscene", ComponentRegistry::withBuiltins(), issues);
    REQUIRE(reopened);
    CHECK(reopened->scene().size() == 1);
    CHECK_FALSE(reopened->isDirty());
    std::filesystem::remove_all(dir);
}

TEST_CASE("The history has a size limit", "[editor][commands]")
{
    Scene scene;
    CommandStack stack(3);
    const EntityId id = scene.create("A")->id();
    for (int i = 0; i < 5; ++i) {
        stack.breakMerge();
        stack.execute(scene, std::make_unique<RenameEntity>(id, "Name " + std::to_string(i)));
    }
    CHECK(stack.size() == 3);
    CHECK_FALSE(stack.isClean()); // the clean state fell off the end
    while (stack.undo(scene)) {
    }
    CHECK(scene.find(id)->name() == "Name 1");
}

TEST_CASE("A command group undoes as one step, in reverse order", "[editor][commands]")
{
    Fixture f;
    const EntityId id = f.make("Box");
    auto group = std::make_unique<CommandGroup>("Reset Transform");
    group->add(std::make_unique<SetProperty>(id, "lantern.Transform", "rotation", 10.0f, "Rotation"));
    group->add(std::make_unique<SetProperty>(id, "lantern.Transform", "rotation", 20.0f, "Rotation"));
    group->add(std::make_unique<RenameEntity>(id, "Box")); // no change: skipped
    REQUIRE(f.doc.execute(std::move(group)));
    CHECK(f.doc.scene().find(id)->transform().rotation == 20.0f);
    CHECK(f.doc.history().undoLabel() == "Reset Transform");
    f.doc.undo();
    CHECK(f.doc.scene().find(id)->transform().rotation == 0.0f);
    CHECK_FALSE(f.doc.execute(std::make_unique<CommandGroup>("Nothing")));
}

TEST_CASE("The scene can be renamed and renamed back", "[editor][commands]")
{
    Fixture f;
    REQUIRE(f.doc.execute(std::make_unique<RenameScene>("Level 1")));
    CHECK(f.doc.scene().name() == "Level 1");
    f.doc.undo();
    CHECK(f.doc.scene().name() == "Test");
}
