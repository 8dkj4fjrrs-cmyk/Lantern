// Projects saved by Lantern Engine (Rolo's old name) keep opening: their
// "lantern." format and component names and their project.lantern file are
// read, and saving writes the new names (rolo/core/Names.hpp).

#include "support/TempFolder.hpp"

#include "rolo/assets/AssetDatabase.hpp"
#include "rolo/core/Files.hpp"
#include "rolo/core/Names.hpp"
#include "rolo/scene/Components.hpp"
#include "rolo/scene/Project.hpp"
#include "rolo/scene/SceneFile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace rolo;
namespace fs = std::filesystem;

namespace {

struct OldProject {
    fs::path folder = test::uniqueTempFolder("lantern-project");
    OldProject()
    {
        fs::create_directories(folder / "scenes");
        fs::create_directories(folder / "assets");
        // Exactly as Lantern 0.9.0 wrote them.
        std::ofstream(folder / "project.lantern") << R"({
  "format": "lantern.project",
  "version": 1,
  "name": "Old Game",
  "startScene": "scenes/main.lscene"
})";
        std::ofstream(folder / "scenes" / "main.lscene") << R"({
  "format": "lantern.scene",
  "version": 1,
  "name": "Main",
  "entities": [
    {
      "id": "0000000000000001",
      "name": "Box",
      "active": true,
      "components": [
        { "type": "lantern.Transform", "position": [10, 20], "rotation": 0, "scale": [1, 1] },
        { "type": "lantern.Shape", "kind": "rectangle", "size": [5, 5], "color": "#ff0000ff",
          "filled": true, "lineWidth": 2 }
      ]
    }
  ]
})";
        std::ofstream(folder / "scenes" / "main.lscene.meta")
            << R"({ "format": "lantern.asset", "version": 1, "id": "0000000000000abc", "type": "scene" })";
    }
    ~OldProject()
    {
        std::error_code ec;
        fs::remove_all(folder, ec);
    }
};

} // namespace

TEST_CASE("Lantern's names are recognised and upgraded", "[scene][legacy]")
{
    CHECK(isFormatName("rolo.scene", "rolo.scene"));
    CHECK(isFormatName("lantern.scene", "rolo.scene"));
    CHECK_FALSE(isFormatName("lantern.quest", "rolo.scene"));
    CHECK_FALSE(isFormatName("other.scene", "rolo.scene"));
    CHECK(upgradeLegacyName("lantern.Transform") == "rolo.Transform");
    CHECK(upgradeLegacyName("mygame.Spinner") == std::nullopt);
    CHECK(upgradeLegacyName("lantern.") == std::nullopt);
}

TEST_CASE("A project saved by Lantern opens, and saving writes Rolo's names", "[scene][legacy]")
{
    OldProject old;
    REQUIRE(isProjectFolder(old.folder));
    std::string error;
    auto project = Project::open(old.folder, &error);
    INFO(error);
    REQUIRE(project);
    CHECK(project->settings().name == "Old Game");

    // Its scene loads, with Lantern's component names upgraded.
    const ComponentRegistry registry = ComponentRegistry::withBuiltins();
    SceneLoadResult loaded = loadSceneFile(old.folder / "scenes" / "main.lscene", registry);
    REQUIRE(loaded.ok());
    CHECK(loaded.issues.empty());
    const Entity* box = loaded.scene->find(EntityId{1});
    REQUIRE(box != nullptr);
    REQUIRE(box->get<Transform>() != nullptr);
    CHECK(box->get<Transform>()->position == Vec2{10, 20});
    CHECK(box->get<Shape>() != nullptr);

    // Its assets keep their IDs.
    AssetDatabase assets(old.folder, AssetDatabase::Access::ReadOnly);
    assets.refresh();
    const AssetRecord* scene = assets.findByPath("scenes/main.lscene");
    REQUIRE(scene != nullptr);
    CHECK(scene->id == AssetId{0xabc});

    // Saving writes the new names, and the old project file goes.
    REQUIRE(saveSceneFile(old.folder / "scenes" / "main.lscene", *loaded.scene));
    const auto saved = readTextFile(old.folder / "scenes" / "main.lscene");
    REQUIRE(saved);
    CHECK(saved->find("\"rolo.scene\"") != std::string::npos);
    CHECK(saved->find("\"rolo.Transform\"") != std::string::npos);
    CHECK(saved->find("lantern.") == std::string::npos);
    REQUIRE(project->save());
    CHECK(fs::exists(old.folder / "project.rolo"));
    CHECK_FALSE(fs::exists(old.folder / "project.lantern"));
    CHECK(Project::open(old.folder));
}
