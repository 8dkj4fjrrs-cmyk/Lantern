#include "lantern/content/Logic.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lantern;

TEST_CASE("Conditions read and write as typed JSON", "[content][logic]")
{
    const std::string text = R"({"type":"all","conditions":[
        {"type":"compare","variable":"gems","op":">=","value":3},
        {"type":"not","condition":{"type":"quest","quest":"0000000000002001","state":"completed"}},
        {"type":"objective","quest":"0000000000002001","objective":"collect"}]})";
    std::string error;
    const auto c = parseCondition(text, &error);
    REQUIRE(c);
    CHECK(error.empty());
    REQUIRE(c->kind == Condition::Kind::All);
    REQUIRE(c->children.size() == 3);
    CHECK(c->children[0].op == CompareOp::GreaterEqual);
    CHECK(std::get<int>(c->children[0].value) == 3);
    CHECK(c->children[1].children.front().state == QuestState::Completed);
    CHECK(describe(*c) == "(gems >= 3) and (not (quest 0000000000002001 is completed)) and (objective 'collect' of "
                         "quest 0000000000002001 is done)");
    // Writing and reading again gives the same condition.
    CHECK(parseCondition(serializeCondition(*c)) == c);
}

TEST_CASE("Conditions and actions with the wrong shape are refused with a reason", "[content][logic]")
{
    std::string error;
    CHECK_FALSE(parseCondition(R"({"type":"eval","code":"rm -rf /"})", &error));
    CHECK(error.find("unknown condition type \"eval\"") != std::string::npos);
    CHECK_FALSE(parseCondition(R"({"type":"compare","variable":"gems","op":"=~","value":1})", &error));
    CHECK(error.find("\"op\" must be one of") != std::string::npos);
    CHECK_FALSE(parseCondition(R"({"type":"quest","quest":"nope","state":"active"})", &error));
    CHECK(error.find("asset ID") != std::string::npos);
    CHECK_FALSE(parseCondition("{not json", &error));
    CHECK(error == "not valid JSON");

    CHECK_FALSE(parseActions(R"([{"type":"add","variable":"gems","value":"many"}])", &error));
    CHECK(error.find("action 1: \"add\" needs a whole number") != std::string::npos);
    CHECK_FALSE(parseActions(R"([{"type":"wait","seconds":-1}])", &error));
    CHECK_FALSE(parseActions(R"({"type":"log"})", &error));
    CHECK(error == "actions must be a list");

    // Deep nesting is limited (no stack exhaustion from a hostile file).
    std::string deep = R"({"type":"always"})";
    for (int i = 0; i < 40; ++i) {
        deep = R"({"type":"not","condition":)" + deep + "}";
    }
    CHECK_FALSE(parseCondition(deep, &error));
    CHECK(error.find("nested too deeply") != std::string::npos);
}

TEST_CASE("Every action type round-trips", "[content][logic]")
{
    const std::string text = R"([
        {"type":"set","variable":"met_elder","value":true},
        {"type":"add","variable":"gems","value":2},
        {"type":"startQuest","quest":"0000000000002001"},
        {"type":"completeObjective","quest":"0000000000002001","objective":"return"},
        {"type":"failQuest","quest":"0000000000002002"},
        {"type":"startDialogue","dialogue":"0000000000003001"},
        {"type":"changeScene","scene":"0000000000004001"},
        {"type":"setActive","entity":"00000000000000aa","active":false},
        {"type":"destroy","entity":null},
        {"type":"emit","event":"door_opened"},
        {"type":"log","text":"Gems: {gems}"},
        {"type":"wait","seconds":1.5},
        {"type":"saveGame","slot":"auto"},
        {"type":"loadGame","slot":"auto"}])";
    std::string error;
    const auto actions = parseActions(text, &error);
    REQUIRE(actions);
    REQUIRE(actions->size() == 14);
    CHECK((*actions)[7].entity == EntityId{0xaa});
    CHECK_FALSE((*actions)[7].active);
    CHECK((*actions)[8].entity.isNull());
    CHECK(describe((*actions)[8]) == "remove this object");
    CHECK(describe((*actions)[11]) == "wait 1.5 s");
    CHECK(parseActions(serializeActions(*actions)) == actions);
}

TEST_CASE("Variables are filled into text", "[content][logic]")
{
    const auto lookup = [](std::string_view name) -> std::optional<std::string> {
        if (name == "gems") {
            return "2";
        }
        return std::nullopt;
    };
    CHECK(substituteVariables("You have {gems} gems", lookup) == "You have 2 gems");
    CHECK(substituteVariables("{unknown} stays", lookup) == "{unknown} stays");
    CHECK(substituteVariables("{{gems}", lookup) == "{gems}");
    CHECK(substituteVariables("open { brace", lookup) == "open { brace");
    CHECK(toDisplayString(Value{true}) == "true");
    CHECK(valueTypeFromName("text") == ValueType::Text);
    CHECK_FALSE(valueTypeFromName("float"));
}
