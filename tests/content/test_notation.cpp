#include "ContentFixture.hpp"

#include "lantern/content/Notation.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lantern;
using namespace lantern::test;

namespace {

// Names as the editor gives them: assets by file name, objects by name.
NotationNames names()
{
    NotationNames n;
    n.assetName = [](AssetId id) -> std::optional<std::string> {
        if (id == kGemsQuest) {
            return "gems";
        }
        if (id == kElder) {
            return "elder";
        }
        if (id == kMeadow) {
            return "meadow";
        }
        return std::nullopt;
    };
    n.findAsset = [](std::string_view name, std::string_view kind, std::string& error) -> std::optional<AssetId> {
        if (kind == "quest" && name == "gems") {
            return kGemsQuest;
        }
        if (kind == "dialogue" && name == "elder") {
            return kElder;
        }
        if (kind == "scene" && name == "meadow") {
            return kMeadow;
        }
        error = "there is no " + std::string(kind) + " called \"" + std::string(name) + "\"";
        return std::nullopt;
    };
    n.entityName = [](EntityId id) -> std::optional<std::string> {
        return id == EntityId{0x77} ? std::optional<std::string>("Talk Area") : std::nullopt;
    };
    n.findEntity = [](std::string_view name, std::string& error) -> std::optional<EntityId> {
        if (name == "Talk Area") {
            return EntityId{0x77};
        }
        error = "no such object";
        return std::nullopt;
    };
    return n;
}

Condition readCondition(std::string_view text)
{
    std::string error;
    const auto c = parseConditionNotation(text, names(), &error);
    INFO(text << " -> " << error);
    REQUIRE(c);
    return *c;
}

} // namespace

TEST_CASE("Conditions in the written form", "[content][notation]")
{
    CHECK(readCondition("") == Condition{});
    CHECK(readCondition("always") == Condition{});
    CHECK(readCondition("met_elder") == compare("met_elder", CompareOp::Equal, true));
    CHECK(readCondition("gems>=3") == compare("gems", CompareOp::GreaterEqual, 3));
    CHECK(readCondition("hero == \"Ana \\\"the\\\" brave\"") == compare("hero", CompareOp::Equal, std::string("Ana \"the\" brave")));
    CHECK(readCondition("gems > -2") == compare("gems", CompareOp::Greater, -2));
    CHECK(readCondition("quest gems is completed") == questIs(kGemsQuest, QuestState::Completed));
    CHECK(readCondition("quest 0000000000002001 is active") == questIs(kGemsQuest, QuestState::Active));
    const Condition objective = readCondition("objective gems collect done");
    CHECK(objective.kind == Condition::Kind::ObjectiveDone);
    CHECK(objective.objective == "collect");

    // and binds tighter than or; parentheses group.
    const Condition c = readCondition("gems >= 3 and met_elder or not rewarded");
    REQUIRE(c.kind == Condition::Kind::Any);
    CHECK(c.children[0].kind == Condition::Kind::All);
    CHECK(c.children[1].kind == Condition::Kind::Not);
    const Condition g = readCondition("gems >= 3 and (met_elder or rewarded)");
    REQUIRE(g.kind == Condition::Kind::All);
    CHECK(g.children[1].kind == Condition::Kind::Any);
}

TEST_CASE("Written conditions read back to the same condition", "[content][notation]")
{
    for (const char* text : {"always", "met_elder", "met_elder == false", "not met_elder", "gems >= 3",
                             "hero != \"Bo\"", "quest gems is failed", "objective gems return done",
                             "gems >= 3 and met_elder", "(gems < 1 and met_elder) or rewarded",
                             "not (gems >= 3 or met_elder)", "gems == 1 and (met_elder or rewarded) and not rewarded"}) {
        const Condition c = readCondition(text);
        const std::string written = formatCondition(c, names());
        CHECK(written == text);
        CHECK(readCondition(written) == c);
    }
    // Without names, IDs are written instead.
    CHECK(formatCondition(questIs(kGemsQuest, QuestState::Active)) == "quest 0000000000002001 is active");
}

TEST_CASE("Mistakes in written conditions are explained", "[content][notation]")
{
    const auto errorOf = [](std::string_view text) {
        std::string error;
        CHECK_FALSE(parseConditionNotation(text, names(), &error));
        return error;
    };
    CHECK(errorOf("gems = 3") == "\"=\" is not an operator; use == or !=");
    CHECK(errorOf("gems >= ") == "expected a value (true, false, a whole number or \"text\"), not the end");
    CHECK(errorOf("quest dragons is active") == "there is no quest called \"dragons\"");
    CHECK(errorOf("quest gems is done") == "expected notStarted, active, completed or failed, not \"done\"");
    CHECK(errorOf("(gems > 1") == "expected ) to close the (, not the end");
    CHECK(errorOf("gems > 1 met_elder") == "expected \"and\", \"or\" or the end, not \"met_elder\"");
    CHECK(errorOf("hero == \"open") == "a text in quotes is not closed with \"");
    CHECK(errorOf("2gems") == "expected a variable, quest, objective, not or (, not \"2gems\"");
}

TEST_CASE("Actions in the written form, one per line", "[content][notation]")
{
    const auto read = [](std::string_view text) {
        std::string error;
        const auto a = parseActionNotation(text, names(), &error);
        INFO(text << " -> " << error);
        REQUIRE(a);
        return *a;
    };
    CHECK(read("add gems 1") == addVar("gems", 1));
    CHECK(read("set hero \"Bo\"") == setVar("hero", std::string("Bo")));
    CHECK(read("startQuest gems") == questAction(Action::Kind::StartQuest, kGemsQuest));
    CHECK(read("completeObjective gems return") == questAction(Action::Kind::CompleteObjective, kGemsQuest, "return"));
    CHECK(read("startDialogue elder").asset == kElder);
    CHECK(read("changeScene meadow").asset == kMeadow);
    CHECK(read("setActive \"Talk Area\" off").entity == EntityId{0x77});
    CHECK(read("destroy this").entity.isNull());
    CHECK(read("wait 1.5").seconds == 1.5f);
    CHECK(read("log \"You have {gems}\"").text == "You have {gems}");

    // Every action writes back to text that reads back the same.
    for (const char* text : {"set met_elder true", "add gems -2", "startQuest gems", "completeObjective gems collect",
                             "failQuest gems", "startDialogue elder", "changeScene meadow", "setActive \"Talk Area\" on",
                             "destroy this", "emit door_opened", "log \"Hi \\\"you\\\"\"", "wait 0.25", "saveGame quick",
                             "loadGame quick"}) {
        const Action a = read(text);
        CHECK(formatAction(a, names()) == text);
    }

    std::string error;
    CHECK_FALSE(parseActionNotation("dance wildly", names(), &error));
    CHECK(error.find("\"dance\" is not an action") != std::string::npos);
    CHECK_FALSE(parseActionNotation("add gems many", names(), &error));
    CHECK_FALSE(parseActionNotation("add gems 1 2", names(), &error));
    CHECK(error == "unexpected \"2\" at the end");
    CHECK_FALSE(parseActionNotation("setActive Nobody off", names(), &error));
    CHECK(error == "no such object");
    CHECK_FALSE(parseActionNotation("log hello", names(), &error));
    CHECK(error == "log needs text in quotes, e.g. log \"Hello\"");
    CHECK_FALSE(parseActionNotation("", names(), &error));
    CHECK(error == "write an action, e.g. add gems 1");
}
