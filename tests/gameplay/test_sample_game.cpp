#include "SampleBehaviours.hpp"

#include "lantern/gameplay/GameModule.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lantern;

TEST_CASE("The sample game module is linked and registers its behaviours", "[gameplay][sample]")
{
    Logger log;
    ComponentRegistry registry = ComponentRegistry::withBuiltins();
    registerGameModules(registry, log);
    CHECK(linkedGameModules().size() == 1);
    CHECK(linkedGameModules()[0] == "sample_game");
    for (const char* id : {"sample.PlayerController", "sample.SceneDoor", "sample.Collectible", "sample.Spinner"}) {
        const ComponentType* type = registry.find(id);
        REQUIRE(type != nullptr);
        CHECK(type->category == "Gameplay");
    }
}

#include "lantern/gameplay/Game.hpp"
#include "lantern/scene/Project.hpp"
#include "lantern/scene/SceneFile.hpp"

#include <catch2/catch_approx.hpp>

namespace {

const std::filesystem::path kSample = LANTERN_SAMPLE_PROJECT_DIR;

// Everything the player needs to play the sample, read-only like a game.
struct SampleProject {
    Logger log;
    std::vector<std::string> errors;
    std::vector<std::string> infos;
    ComponentRegistry registry = ComponentRegistry::withBuiltins();
    AssetDatabase assets{kSample, AssetDatabase::Access::ReadOnly};

    SampleProject()
    {
        log.addSink([this](const LogMessage& m) { (m.level == LogLevel::Error ? errors : infos).push_back(m.text); });
        registerGameModules(registry, log);
        for (const FileIssue& issue : assets.refresh()) {
            errors.push_back(issue.toString());
        }
    }

    std::optional<Scene> load(AssetId id, std::string& error) const
    {
        const AssetRecord* record = assets.find(id);
        if (record == nullptr || record->type != AssetType::Scene) {
            error = "not a scene in this project";
            return std::nullopt;
        }
        SceneLoadResult loaded = loadSceneFile(assets.absolutePath(*record), registry);
        if (!loaded.ok()) {
            error = "could not be read";
            return std::nullopt;
        }
        return std::move(*loaded.scene);
    }
};

EntityId named(const Scene& scene, std::string_view name)
{
    EntityId found;
    scene.forEach([&](const Entity& e, int) {
        if (found.isNull() && e.name() == name) {
            found = e.id();
        }
    });
    return found;
}

} // namespace

TEST_CASE("The sample project opens cleanly: scenes, assets and behaviours", "[gameplay][sample]")
{
    SampleProject p;
    CHECK(p.errors.empty());
    std::string error;
    const auto project = Project::open(kSample, &error);
    REQUIRE(project);
    for (const char* path : {"scenes/meadow.lscene", "scenes/cave.lscene"}) {
        SceneLoadResult loaded = loadSceneFile(kSample / path, p.registry);
        REQUIRE(loaded.ok());
        CHECK(loaded.issues.empty());   // no unknown components: the module is linked
        CHECK(checkAssetReferences(*loaded.scene, p.assets).empty());
    }
    CHECK(project->settings().startScene == "scenes/meadow.lscene");
}

TEST_CASE("Playing the sample: walls stop the player, the door leads to the cave", "[gameplay][sample]")
{
    SampleProject p;
    std::string error;
    const AssetRecord* meadowAsset = p.assets.findByPath("scenes/meadow.lscene");
    REQUIRE(meadowAsset != nullptr);
    const auto meadow = p.load(meadowAsset->id, error);
    REQUIRE(meadow);

    Game game(p.log, &p.assets, [&](AssetId id, std::string& why) { return p.load(id, why); });
    game.start(*meadow);
    const EntityId player = named(game.scene(), "Player");
    REQUIRE_FALSE(player.isNull());
    const float startX = game.worldPosition(player).x;

    // Walking left: stopped by the wall around the meadow (cell 0 is 0..32).
    game.input().press(Key::Left);
    for (int i = 0; i < 120; ++i) {
        game.update(1.0f / 60.0f);
    }
    game.input().release(Key::Left);
    const Box feet = *game.colliderBox(player);
    CHECK(feet.min.x == Catch::Approx(32.0f).margin(0.01f));
    CHECK(game.worldPosition(player).x < startX);
    CHECK(game.scene().find(player)->get<Sprite>()->flipX);   // facing left

    // Walking right along the path for long enough reaches the door.
    game.input().press(Key::Right);
    for (int i = 0; i < 900 && game.scene().name() == "Meadow"; ++i) {
        game.update(1.0f / 60.0f);
    }
    CHECK(game.scene().name() == "Cave");
    CHECK(p.errors.empty());

    // In the cave, the player starts next to the way back, not on it.
    game.input().releaseAll();
    game.update(1.0f / 60.0f);
    CHECK(game.scene().name() == "Cave");
}

TEST_CASE("Playing the sample: gems are collected and logged", "[gameplay][sample]")
{
    SampleProject p;
    std::string error;
    auto meadow = p.load(p.assets.findByPath("scenes/meadow.lscene")->id, error);
    REQUIRE(meadow);
    Game game(p.log, &p.assets);
    game.start(*meadow);
    const EntityId player = named(game.scene(), "Player");
    const EntityId gem = named(game.scene(), "Gem 1");
    REQUIRE_FALSE(gem.isNull());

    // Put the player's feet (its collider) on the gem, as if they had walked there.
    const Vec2 offset = game.worldPosition(gem) - game.colliderBox(player)->center();
    game.transform(player)->position = game.scene().find(player)->transform().position + offset;
    game.update(1.0f / 60.0f);
    CHECK_FALSE(game.scene().contains(gem));
    CHECK(std::find(p.infos.begin(), p.infos.end(), "Picked up a gem by the pond") != p.infos.end());
    CHECK(game.scene().contains(named(game.scene(), "Gem 2")));   // the others are still there
}
