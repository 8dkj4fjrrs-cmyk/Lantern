#include "ContentFixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace lantern;
using namespace lantern::test;

namespace {

bool hasIssue(const std::vector<FileIssue>& issues, std::string_view text, bool error = true)
{
    return std::any_of(issues.begin(), issues.end(), [&](const FileIssue& i) {
        return i.isError() == error && (i.where + ": " + i.message).find(text) != std::string::npos;
    });
}

} // namespace

TEST_CASE("Content files round-trip: variables, dialogue, quest", "[content][files]")
{
    const ContentLibrary lib = makeLibrary();
    VariableSet vars;
    for (const VariableDefinition* v : lib.variables()) {
        vars.variables.push_back(*v);
    }
    const auto readVars = parseVariables(serializeVariables(vars));
    REQUIRE(readVars.ok());
    CHECK(readVars.issues.empty());
    CHECK(*readVars.content == vars);

    const auto readDialogue = parseDialogue(serializeDialogue(*lib.dialogue(kElder)));
    REQUIRE(readDialogue.ok());
    CHECK(readDialogue.issues.empty());
    CHECK(*readDialogue.content == *lib.dialogue(kElder));

    const auto readQuest = parseQuest(serializeQuest(*lib.quest(kCaveQuest)));
    REQUIRE(readQuest.ok());
    CHECK(readQuest.issues.empty());
    CHECK(*readQuest.content == *lib.quest(kCaveQuest));
    CHECK(parseQuest(serializeQuest(*lib.quest(kGemsQuest))).content == *lib.quest(kGemsQuest));
}

TEST_CASE("A hand-written dialogue file reads as documented", "[content][files]")
{
    const auto read = parseDialogue(R"({
      "format": "lantern.dialogue", "version": 1,
      "speakers": [{ "id": "elder", "name": "Village Elder", "color": "#f2a93b" }],
      "start": "hello",
      "nodes": [
        { "id": "hello", "speaker": "elder", "text": "Hello!", "next": "ask" },
        { "id": "ask", "speaker": "elder", "text": "Help me?",
          "choices": [
            { "id": "yes", "text": "Yes", "actions": [{ "type": "set", "variable": "helping", "value": true }], "next": null },
            { "id": "no", "text": "No", "condition": { "type": "compare", "variable": "brave", "value": false } }
          ] }
      ],
      "mood": "calm"
    })");
    REQUIRE(read.ok());
    const Dialogue& d = *read.content;
    CHECK(d.speakers.front().color == Color{0xf2, 0xa9, 0x3b, 0xff});
    CHECK(d.findNode("ask")->choices.size() == 2);
    CHECK(d.findNode("ask")->choices[1].condition.op == CompareOp::Equal); // "op" defaults to ==
    CHECK(hasIssue(read.issues, "Unknown field \"mood\"", false));
}

TEST_CASE("Broken content files say what is wrong and where", "[content][files]")
{
    SECTION("duplicate ids")
    {
        const auto read = parseDialogue(R"({"format":"lantern.dialogue","version":1,"start":"a",
            "nodes":[{"id":"a","text":"x"},{"id":"a","text":"y"}]})");
        CHECK_FALSE(read.ok());
        CHECK(hasIssue(read.issues, "node 2: Another item already has the id \"a\""));
    }
    SECTION("bad id")
    {
        const auto read = parseVariables(R"({"format":"lantern.variables","version":1,
            "variables":[{"id":"2 gems","type":"int"}]})");
        CHECK_FALSE(read.ok());
        CHECK(hasIssue(read.issues, "must start with a letter"));
    }
    SECTION("starting value of the wrong type")
    {
        const auto read = parseVariables(R"({"format":"lantern.variables","version":1,
            "variables":[{"id":"gems","type":"int","initial":"lots"}]})");
        CHECK_FALSE(read.ok());
        CHECK(hasIssue(read.issues, "variable 'gems': \"initial\" must be a int value"));
    }
    SECTION("a condition that is not in the language")
    {
        const auto read = parseQuest(R"({"format":"lantern.quest","version":1,"name":"Q",
            "objectives":[{"id":"o","completeWhen":{"type":"script","code":"x"}}]})");
        CHECK_FALSE(read.ok());
        CHECK(hasIssue(read.issues, "objective 'o', completeWhen: Condition: unknown condition type \"script\""));
    }
    SECTION("newer version, wrong format, not JSON")
    {
        CHECK(hasIssue(parseQuest(R"({"format":"lantern.quest","version":99,"objectives":[]})").issues, "newer version"));
        CHECK(hasIssue(parseQuest(R"({"format":"lantern.dialogue","version":1})").issues, "not a lantern.quest file"));
        CHECK(hasIssue(parseQuest("{").issues, "not valid JSON"));
    }
    SECTION("missing defaults are filled in")
    {
        const auto read = parseVariables(R"({"format":"lantern.variables","version":1,
            "variables":[{"id":"name","type":"text"},{"id":"flag","type":"bool"}]})");
        REQUIRE(read.ok());
        CHECK(std::get<std::string>(read.content->variables[0].initial).empty());
        CHECK(std::get<bool>(read.content->variables[1].initial) == false);
    }
    CHECK(isValidContentId("_gem_2"));
    CHECK_FALSE(isValidContentId(""));
    CHECK_FALSE(isValidContentId("gem-2"));
    CHECK_FALSE(isValidContentId(std::string(65, 'a')));
}
