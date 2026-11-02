#include "support/TempFolder.hpp"
#include "ContentFixture.hpp"

#include "lantern/core/Files.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace lantern;
using namespace lantern::test;

namespace {

bool hasIssue(const std::vector<FileIssue>& issues, std::string_view text, bool error = true)
{
    return std::any_of(issues.begin(), issues.end(), [&](const FileIssue& i) {
        return i.isError() == error && (i.where + ": " + i.message).find(text) != std::string::npos;
    });
}

std::string all(const std::vector<FileIssue>& issues)
{
    std::string out;
    for (const FileIssue& i : issues) {
        out += i.toString() + "\n";
    }
    return out;
}

} // namespace

TEST_CASE("Content that fits together has no problems", "[content][library]")
{
    const ContentLibrary lib = makeLibrary();
    const auto issues = lib.check();
    INFO(all(issues));
    CHECK(issues.empty());
    CHECK(lib.variable("gems")->type == ValueType::Int);
    CHECK(lib.labelOf(kElder) == "content/elder.ldialogue");
    CHECK(lib.questIds().size() == 2);
}

TEST_CASE("References between content are checked", "[content][library]")
{
    ContentLibrary lib = makeLibrary();
    Dialogue d = *lib.dialogue(kElder);
    d.nodes[1].next = "missing";                                            // a line leading nowhere
    d.nodes[1].speaker = "ghost";                                           // an unknown speaker
    d.nodes[2].choices[0].actions.push_back(addVar("met_elder", 1));        // add to a bool
    d.nodes[2].choices[1].condition = compare("gems", CompareOp::Equal, true);  // int compared with bool
    d.nodes[3].actions.push_back(questAction(Action::Kind::CompleteObjective, kGemsQuest, "dance"));
    DialogueNode lonely;
    lonely.id = "lonely";
    lonely.text = "Nobody comes here.";
    d.nodes.push_back(lonely);
    lib.addDialogue(kElder, "content/elder.ldialogue", d);

    Quest q = *lib.quest(kCaveQuest);
    q.startWhen = compare("hero", CompareOp::Greater, std::string("B"));    // ordering on text
    q.rewards.push_back(questAction(Action::Kind::StartQuest, AssetId{0x9999}));
    Action scene;
    scene.kind = Action::Kind::ChangeScene;
    scene.asset = AssetId{0x8888};
    q.rewards.push_back(scene);
    lib.addQuest(kCaveQuest, "content/cave.lquest", q);

    const auto issues = lib.check();
    INFO(all(issues));
    CHECK(hasIssue(issues, "content/elder.ldialogue, node 'greet': It leads to 'missing', but there is no such node."));
    CHECK(hasIssue(issues, "node 'greet': There is no speaker 'ghost'"));
    CHECK(hasIssue(issues, "choice 'yes', action 2 (add): Only whole-number variables can be added to"));
    CHECK(hasIssue(issues, "choice 'rich': Variable 'gems' holds int values; it cannot be compared with true"));
    CHECK(hasIssue(issues, "node 'thanks', action 1 (completeObjective): Quest 'content/gems.lquest' has no objective 'dance'"));
    CHECK(hasIssue(issues, "node 'lonely': No other node leads here", false));
    CHECK(hasIssue(issues, "content/cave.lquest, startWhen: Only whole numbers can be compared with >"));
    CHECK(hasIssue(issues, "rewards, action 1 (startQuest): The quest 0000000000009999 does not exist"));
    CHECK(hasIssue(issues, "rewards, action 2 (changeScene): The scene 0000000000008888 does not exist"));
}

TEST_CASE("Loops that could never finish are found", "[content][library]")
{
    ContentLibrary lib = makeLibrary();
    // Quests that need each other.
    Quest a = *lib.quest(kGemsQuest);
    a.prerequisites = {kCaveQuest};
    lib.addQuest(kGemsQuest, "content/gems.lquest", a);
    // Branch nodes that only lead to each other.
    Dialogue d;
    d.start = "x";
    DialogueNode x;
    x.id = "x";
    x.branches = {{compare("gems", CompareOp::Greater, 5), "y"}};
    x.next = "y";
    DialogueNode y;
    y.id = "y";
    y.branches = {{compare("gems", CompareOp::Greater, 9), "x"}};
    y.next = "x";
    d.nodes = {x, y};
    lib.addDialogue(AssetId{0x3002}, "content/loop.ldialogue", d);
    // The same variable in two files.
    VariableSet more;
    more.variables = {{"gems", ValueType::Int, 0, ""}};
    lib.addVariables(AssetId{0x1002}, "content/more.lvars", more);

    const auto issues = lib.check();
    INFO(all(issues));
    CHECK(hasIssue(issues, "content/gems.lquest, prerequisites: The quest's prerequisites lead back to itself"));
    CHECK(hasIssue(issues, "content/cave.lquest, prerequisites: The quest's prerequisites lead back to itself"));
    CHECK(hasIssue(issues, "content/loop.ldialogue, node 'x': Branch nodes lead back to this one"));
    CHECK(hasIssue(issues, "content/more.lvars, variable 'gems': The variable is also defined in content/game.lvars"));
}

TEST_CASE("A project's content is loaded from its asset files", "[content][library]")
{
    namespace fs = std::filesystem;
    const fs::path folder = lantern::test::uniqueTempFolder("content-library");
    fs::remove_all(folder);
    fs::create_directories(folder / "content");
    std::ofstream(folder / "project.lantern") << R"({"format":"lantern.project","version":1,"name":"T"})";
    std::ofstream(folder / "content/game.lvars")
        << R"({"format":"lantern.variables","version":1,"variables":[{"id":"gems","type":"int","initial":1}]})";
    std::ofstream(folder / "content/broken.lquest") << R"({"format":"lantern.quest","version":1})";
    std::ofstream(folder / "content/hi.ldialogue")
        << R"({"format":"lantern.dialogue","version":1,"start":"a","nodes":[{"id":"a","text":"You have {gems}."}]})";

    AssetDatabase assets(folder);
    assets.refresh();
    std::vector<FileIssue> issues;
    const ContentLibrary lib = ContentLibrary::load(assets, &issues);
    INFO(all(issues));
    CHECK(lib.variable("gems") != nullptr);
    CHECK(lib.dialogueIds().size() == 1);
    CHECK(lib.questIds().empty());   // the broken one is left out...
    CHECK(hasIssue(issues, "content/broken.lquest: \"objectives\" must be a list."));   // ...and reported
    CHECK(assets.findByPath("content/hi.ldialogue")->type == AssetType::Dialogue);
    fs::remove_all(folder);
}
