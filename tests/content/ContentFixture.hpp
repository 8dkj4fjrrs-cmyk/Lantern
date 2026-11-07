#pragma once

// A small game's content, shared by the content tests: three variables, two
// quests (the second needs the first) and a dialogue with a branch and a
// conditional choice.

#include "rolo/content/ContentLibrary.hpp"

namespace rolo::test {

inline const AssetId kVars{0x1001};
inline const AssetId kGemsQuest{0x2001};
inline const AssetId kCaveQuest{0x2002};
inline const AssetId kElder{0x3001};
inline const AssetId kMeadow{0x4001};

inline Condition compare(std::string variable, CompareOp op, Value value)
{
    Condition c;
    c.kind = Condition::Kind::Compare;
    c.variable = std::move(variable);
    c.op = op;
    c.value = std::move(value);
    return c;
}

inline Condition questIs(AssetId quest, QuestState state)
{
    Condition c;
    c.kind = Condition::Kind::QuestIs;
    c.quest = quest;
    c.state = state;
    return c;
}

inline Action setVar(std::string variable, Value value)
{
    Action a;
    a.kind = Action::Kind::Set;
    a.variable = std::move(variable);
    a.value = std::move(value);
    return a;
}

inline Action addVar(std::string variable, int amount)
{
    Action a;
    a.kind = Action::Kind::Add;
    a.variable = std::move(variable);
    a.value = amount;
    return a;
}

inline Action questAction(Action::Kind kind, AssetId quest, std::string objective = {})
{
    Action a;
    a.kind = kind;
    a.quest = quest;
    a.objective = std::move(objective);
    return a;
}

inline ContentLibrary makeLibrary()
{
    ContentLibrary lib;
    VariableSet vars;
    vars.variables = {{"gems", ValueType::Int, 0, "Gems carried"},
                      {"met_elder", ValueType::Bool, false, ""},
                      {"hero", ValueType::Text, std::string("Ana"), ""},
                      {"rewarded", ValueType::Bool, false, ""}};
    lib.addVariables(kVars, "content/game.lvars", vars);

    Quest gems;
    gems.name = "Gems for the Elder";
    gems.objectives = {{"collect", "Find 3 gems ({gems}/3)", compare("gems", CompareOp::GreaterEqual, 3), false},
                       {"return", "Bring them back", std::nullopt, false},
                       {"bonus", "Find the hidden gem", std::nullopt, true}};
    gems.rewards = {setVar("rewarded", true)};
    lib.addQuest(kGemsQuest, "content/gems.lquest", gems);

    Quest cave;
    cave.name = "The Cave";
    cave.prerequisites = {kGemsQuest};
    cave.startWhen = compare("met_elder", CompareOp::Equal, true);
    cave.objectives = {{"enter", "Enter the cave", std::nullopt, false}};
    cave.failWhen = compare("gems", CompareOp::Less, 0);
    lib.addQuest(kCaveQuest, "content/cave.lquest", cave);

    Dialogue d;
    d.speakers = {{"elder", "Elder", Color{242, 169, 59, 255}, {}}};
    d.start = "begin";
    DialogueNode begin;
    begin.id = "begin";
    begin.branches = {{questIs(kGemsQuest, QuestState::Active), "waiting"}};
    begin.next = "greet";
    DialogueNode greet;
    greet.id = "greet";
    greet.speaker = "elder";
    greet.text = "Hello, {hero}.";
    greet.actions = {setVar("met_elder", true)};
    greet.next = "ask";
    DialogueNode ask;
    ask.id = "ask";
    ask.speaker = "elder";
    ask.text = "Will you find 3 gems?";
    ask.choices = {{"yes", "Yes.", {}, {questAction(Action::Kind::StartQuest, kGemsQuest)}, "thanks"},
                   {"rich", "I have {gems} gems already!", compare("gems", CompareOp::GreaterEqual, 3), {}, ""},
                   {"no", "No.", {}, {}, ""}};
    DialogueNode thanks;
    thanks.id = "thanks";
    thanks.speaker = "elder";
    thanks.text = "Thank you!";
    DialogueNode waiting;
    waiting.id = "waiting";
    waiting.speaker = "elder";
    waiting.text = "You have {gems} of 3.";
    d.nodes = {begin, greet, ask, thanks, waiting};
    lib.addDialogue(kElder, "content/elder.ldialogue", d);
    lib.addScene(kMeadow, "scenes/meadow.lscene");
    return lib;
}

} // namespace rolo::test
