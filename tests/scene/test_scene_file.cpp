#include "lantern/scene/SceneFile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace lantern;

namespace {

const ComponentRegistry& registry()
{
    static const ComponentRegistry r = ComponentRegistry::withBuiltins();
    return r;
}

bool mentions(const SceneLoadResult& r, std::string_view text)
{
    for (const FileIssue& i : r.issues) {
        if (i.toString().find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string wrap(std::string entities)
{
    return R"({"format": "lantern.scene", "version": 1, "name": "T", "entities": [)" + entities + "]}";
}

} // namespace

TEST_CASE("A scene survives save and load unchanged", "[scene][file]")
{
    const Scene original = makeStartScene();
    const std::string text = serializeScene(original);
    const SceneLoadResult loaded = parseScene(text, registry());
    REQUIRE(loaded.ok());
    CHECK(loaded.issues.empty());
    CHECK(loaded.scene->name() == original.name());
    CHECK(loaded.scene->size() == original.size());
    CHECK(serializeScene(*loaded.scene) == text); // byte-for-byte stable

    // Same IDs, same tree, same values.
    original.forEach([&](const Entity& e, int) {
        const Entity* copy = loaded.scene->find(e.id());
        REQUIRE(copy != nullptr);
        CHECK(copy->name() == e.name());
        CHECK(copy->parent() == e.parent());
        CHECK(copy->children() == e.children());
        CHECK(copy->components().size() == e.components().size());
    });
}

TEST_CASE("Saved numbers are short and readable", "[scene][file]")
{
    Scene scene("Numbers");
    const EntityId id = scene.create("Box")->id();
    scene.setProperty(id, "lantern.Transform", "position", Vec2{0.1f, 640.0f});
    const std::string text = serializeScene(scene);
    CHECK(text.find("0.1,") != std::string::npos);
    CHECK(text.find("640") != std::string::npos);
    CHECK(text.find("0.10000000149") == std::string::npos);
    CHECK(text.find("640.0") == std::string::npos);
}

TEST_CASE("Files from a newer Lantern are refused with advice", "[scene][file]")
{
    const auto r = parseScene(R"({"format": "lantern.scene", "version": 99, "entities": []})", registry());
    CHECK_FALSE(r.ok());
    CHECK(r.firstError().find("newer version of Lantern") != std::string::npos);
}

TEST_CASE("Broken files give a clear error, never a crash", "[scene][file]")
{
    CHECK(parseScene("", registry()).firstError().find("not valid JSON") != std::string::npos);
    CHECK(parseScene("{\"format\": \"lantern.scene\",\n \"version\": 1, }", registry()).firstError().find("line 2")
          != std::string::npos);
    CHECK(parseScene(R"({"format": "something.else", "version": 1})", registry()).firstError().find("not a Lantern scene")
          != std::string::npos);
    CHECK_FALSE(parseScene(R"({"format": "lantern.scene", "version": 1})", registry()).ok()); // no entities
    CHECK_FALSE(parseScene(R"({"format": "lantern.scene", "entities": []})", registry()).ok()); // no version
    CHECK_FALSE(parseScene(wrap("42"), registry()).ok());
}

TEST_CASE("IDs must be present, valid and unique", "[scene][file]")
{
    const auto missing = parseScene(wrap(R"({"name": "A"})"), registry());
    CHECK_FALSE(missing.ok());
    CHECK(missing.firstError().find("16 hex digits") != std::string::npos);

    const auto twice = parseScene(wrap(R"({"id": "00000000000000aa", "name": "A"},
                                         {"id": "00000000000000aa", "name": "B"})"),
                                  registry());
    CHECK_FALSE(twice.ok());
    CHECK(twice.firstError().find("same ID") != std::string::npos);

    const auto orphan = parseScene(wrap(R"({"id": "00000000000000aa", "parent": "00000000000000bb"})"), registry());
    CHECK_FALSE(orphan.ok());
    CHECK(orphan.firstError().find("parent") != std::string::npos);
}

TEST_CASE("Bad values fall back to defaults with a warning", "[scene][file]")
{
    const auto r = parseScene(wrap(R"({"id": "00000000000000aa", "name": "A", "components": [
        {"type": "lantern.Transform", "position": "here", "rotation": 45, "scale": [1, 1]},
        {"type": "lantern.Shape", "kind": "triangle", "size": [-10, 5], "color": "#12345678", "wobble": 3}]})"),
                           registry());
    REQUIRE(r.ok());
    const Entity* e = r.scene->find(*EntityId::parse("00000000000000aa"));
    REQUIRE(e != nullptr);
    CHECK(e->transform().position == Vec2{});      // default
    CHECK(e->transform().rotation == 45.0f);       // the good value is kept
    CHECK(e->get<Shape>()->kind == ShapeKind::Rectangle);
    CHECK(e->get<Shape>()->size == Vec2{0, 5});    // clamped
    CHECK(e->get<Shape>()->color == Color{0x12, 0x34, 0x56, 0x78});
    CHECK(mentions(r, "\"position\" should be two numbers"));
    CHECK(mentions(r, "\"kind\" should be one of"));
    CHECK(mentions(r, "out of range"));
    CHECK(mentions(r, "Unknown property \"wobble\""));
    CHECK(r.warningCount() == 4);
}

TEST_CASE("Unknown component types are kept and saved unchanged", "[scene][file]")
{
    const std::string text = wrap(R"({"id": "00000000000000aa", "name": "A", "components": [
        {"type": "lantern.Transform", "position": [1, 2], "rotation": 0, "scale": [1, 1]},
        {"type": "mygame.Health", "max": 10, "regen": [1, 2, {"deep": true}]}]})");
    const auto r = parseScene(text, registry());
    REQUIRE(r.ok());
    CHECK(mentions(r, "not known to this version"));
    const Entity* e = r.scene->find(*EntityId::parse("00000000000000aa"));
    REQUIRE(e->component("mygame.Health") != nullptr);

    const std::string saved = serializeScene(*r.scene);
    CHECK(saved.find(R"("type": "mygame.Health")") != std::string::npos);
    CHECK(saved.find(R"("deep": true)") != std::string::npos);
    // ...and it can be read again with the same data.
    CHECK(parseScene(saved, registry()).ok());
}

TEST_CASE("Hierarchy and order come back from the file", "[scene][file]")
{
    const auto r = parseScene(wrap(R"(
        {"id": "00000000000000a1", "name": "Parent"},
        {"id": "00000000000000b2", "parent": "00000000000000a1", "name": "Second"},
        {"id": "00000000000000b1", "parent": "00000000000000a1", "name": "First", "active": false})"),
                              registry());
    REQUIRE(r.ok());
    const Entity* parent = r.scene->find(*EntityId::parse("00000000000000a1"));
    REQUIRE(parent->children().size() == 2);
    CHECK(r.scene->find(parent->children()[0])->name() == "Second"); // file order = drawing order
    CHECK_FALSE(r.scene->find(parent->children()[1])->isActive());
}

TEST_CASE("Scene files are written atomically and read back", "[scene][file]")
{
    const auto dir = std::filesystem::temp_directory_path() / "lantern_scene_file_test";
    std::filesystem::remove_all(dir);
    const auto path = dir / "nested" / "main.lscene";
    REQUIRE(saveSceneFile(path, makeStartScene()));
    CHECK_FALSE(std::filesystem::exists(path.string() + ".tmp"));
    const auto r = loadSceneFile(path, registry());
    CHECK(r.ok());
    CHECK_FALSE(loadSceneFile(dir / "missing.lscene", registry()).ok());
    std::filesystem::remove_all(dir);
}

TEST_CASE("Validation accepts scenes built through the API", "[scene][file]")
{
    CHECK(validateScene(makeStartScene()).empty());
    CHECK(validateScene(Scene()).empty());
}

TEST_CASE("The hand-written test card scene loads cleanly", "[scene][file]")
{
    const auto r = loadSceneFile(std::filesystem::path(LANTERN_TEST_DATA_DIR) / "test_card.lscene", registry());
    REQUIRE(r.ok());
    CHECK(r.issues.empty());
    CHECK(r.scene->name() == "Test Card");
    CHECK(r.scene->size() == 4);
}
