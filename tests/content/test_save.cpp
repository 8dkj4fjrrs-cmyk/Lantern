#include "ContentFixture.hpp"

#include "rolo/content/ContentRunner.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace rolo;
using namespace rolo::test;

TEST_CASE("A save holds the progress and reads back identically", "[content][save]")
{
    const ContentLibrary lib = makeLibrary();
    Logger log;
    ContentRunner runner(lib, log);
    runner.run({addVar("gems", 2), setVar("hero", std::string("Bo"))});
    runner.startQuest(kGemsQuest);
    runner.completeObjective(kGemsQuest, "return");
    GameState& state = runner.state();
    state.scene = kMeadow;
    state.playerPosition = Vec2{212.5f, 480.0f};
    state.removed(kMeadow).insert(EntityId{0xabc});
    state.fired(kMeadow).insert(EntityId{0xdef});

    const std::string text = serializeSave(state, "Test Game");
    CHECK(text.find("\"game\": \"Test Game\"") != std::string::npos);
    const SaveLoadResult loaded = parseSave(text, lib);
    REQUIRE(loaded.ok());
    CHECK(loaded.issues.empty());
    CHECK(*loaded.state == state);
    CHECK(loaded.state->isRemoved(kMeadow, EntityId{0xabc}));
    CHECK(loaded.state->hasFired(kMeadow, EntityId{0xdef}));
    CHECK_FALSE(loaded.state->isRemoved(kMeadow, EntityId{0xdef}));

    // A runner given the loaded state carries on from there.
    ContentRunner resumed(lib, log);
    resumed.setState(*loaded.state);
    resumed.run({addVar("gems", 1)});
    resumed.update(0.1f);
    CHECK(resumed.state().questState(kGemsQuest) == QuestState::Completed);
}

TEST_CASE("Old saves keep working after the game's content changes", "[content][save]")
{
    const ContentLibrary lib = makeLibrary();
    const auto loaded = parseSave(R"({
        "format": "rolo.save", "version": 1, "game": "Test",
        "scene": "0000000000004001",
        "variables": { "gems": 4, "met_elder": "yes", "old_flag": true },
        "quests": { "0000000000002001": { "state": "active", "objectives": ["collect", "removed_objective"] },
                    "0000000000002999": { "state": "completed" } },
        "removed": { "0000000000004001": ["0000000000000abc", "not-an-id"] }
    })", lib);
    REQUIRE(loaded.ok());
    const GameState& s = *loaded.state;
    CHECK(std::get<int>(*s.variable("gems")) == 4);
    CHECK(std::get<bool>(*s.variable("met_elder")) == false);              // wrong type: starting value
    CHECK(std::get<std::string>(*s.variable("hero")) == "Ana");            // new since the save: starting value
    CHECK_FALSE(s.variable("old_flag"));                                   // gone from the game: dropped
    CHECK(s.isObjectiveDone(kGemsQuest, "collect"));
    CHECK_FALSE(s.isObjectiveDone(kGemsQuest, "removed_objective"));
    CHECK(s.questState(AssetId{0x2999}) == QuestState::NotStarted);
    CHECK(s.isRemoved(kMeadow, EntityId{0xabc}));
    CHECK(loaded.issues.size() == 3);   // met_elder, old_flag, the missing quest
}

TEST_CASE("Damaged or foreign save files are refused", "[content][save]")
{
    const ContentLibrary lib = makeLibrary();
    CHECK_FALSE(parseSave("{oops", lib).ok());
    CHECK_FALSE(parseSave(R"({"format":"rolo.scene","version":1})", lib).ok());
    const auto newer = parseSave(R"({"format":"rolo.save","version":2})", lib);
    REQUIRE_FALSE(newer.ok());
    CHECK(newer.issues.front().message == "This save was made by a newer version of the game.");
}
