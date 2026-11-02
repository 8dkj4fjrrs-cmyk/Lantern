#include "ContentFixture.hpp"

#include "lantern/content/ContentRunner.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace lantern;
using namespace lantern::test;

namespace {

// Records what the runner asks the game to do.
struct Host : ContentHost {
    std::vector<std::string> calls;
    std::vector<ContentEvent> events;
    void changeScene(AssetId scene) override { calls.push_back("scene " + scene.toString()); }
    void setActive(EntityId e, bool active) override
    {
        calls.push_back(std::string(active ? "on " : "off ") + e.toString());
    }
    void destroy(EntityId e) override { calls.push_back("destroy " + e.toString()); }
    void emit(std::string_view event) override { calls.push_back("emit " + std::string(event)); }
    void saveGame(std::string_view slot) override { calls.push_back("save " + std::string(slot)); }
    void loadGame(std::string_view slot) override { calls.push_back("load " + std::string(slot)); }
    void notify(const ContentEvent& e) override { events.push_back(e); }
};

struct Fixture {
    ContentLibrary lib = makeLibrary();
    Logger log;
    std::vector<std::string> lines;
    Host host;
    ContentRunner runner{lib, log};
    Fixture()
    {
        log.addSink([this](const LogMessage& m) { lines.push_back(m.text); });
        runner.setHost(&host);
    }
    bool logged(std::string_view text) const
    {
        return std::find(lines.begin(), lines.end(), text) != lines.end();
    }
};

} // namespace

TEST_CASE("A new game starts with the variables' starting values", "[content][runner]")
{
    Fixture f;
    CHECK(std::get<int>(*f.runner.state().variable("gems")) == 0);
    CHECK(std::get<std::string>(*f.runner.state().variable("hero")) == "Ana");
    CHECK(f.runner.fillIn("{hero} has {gems}") == "Ana has 0");
    CHECK(f.runner.evaluate(compare("gems", CompareOp::Less, 1)));
    CHECK_FALSE(f.runner.evaluate(compare("gems", CompareOp::Equal, true)));   // wrong kind: false, not a crash
    CHECK_FALSE(f.runner.evaluate(compare("nothing", CompareOp::Equal, 0)));
}

TEST_CASE("Quests: start, objectives by condition and by action, rewards, the next quest", "[content][runner]")
{
    Fixture f;
    // The cave quest needs the gems quest completed first, even though its
    // start condition (met the elder) comes true earlier.
    f.runner.run({setVar("met_elder", true)});
    f.runner.update(0.1f);
    CHECK(f.runner.state().questState(kCaveQuest) == QuestState::NotStarted);

    CHECK(f.runner.startQuest(kGemsQuest));
    CHECK_FALSE(f.runner.startQuest(kGemsQuest));   // already started
    CHECK(f.logged("Quest started: Gems for the Elder"));

    f.runner.run({addVar("gems", 2)});
    f.runner.update(0.1f);
    CHECK_FALSE(f.runner.state().isObjectiveDone(kGemsQuest, "collect"));
    f.runner.run({addVar("gems", 1)});
    f.runner.update(0.1f);
    CHECK(f.runner.state().isObjectiveDone(kGemsQuest, "collect"));
    CHECK(f.logged("Objective done (Gems for the Elder): Find 3 gems (3/3)"));
    CHECK(f.runner.state().questState(kGemsQuest) == QuestState::Active);   // "return" is still to do

    // The last required objective (the optional bonus is not needed).
    f.runner.run({questAction(Action::Kind::CompleteObjective, kGemsQuest, "return")});
    CHECK(f.runner.state().questState(kGemsQuest) == QuestState::Completed);
    CHECK(std::get<bool>(*f.runner.state().variable("rewarded")));   // the reward ran
    // ...which lets the cave quest start by itself.
    CHECK(f.runner.state().questState(kCaveQuest) == QuestState::Active);

    // Failing.
    f.runner.run({addVar("gems", -10)});
    f.runner.update(0.1f);
    CHECK(f.runner.state().questState(kCaveQuest) == QuestState::Failed);

    std::vector<ContentEvent::Kind> kinds;
    for (const ContentEvent& e : f.host.events) {
        kinds.push_back(e.kind);
    }
    CHECK(kinds == std::vector<ContentEvent::Kind>{ContentEvent::Kind::QuestStarted, ContentEvent::Kind::ObjectiveCompleted,
                                                   ContentEvent::Kind::ObjectiveCompleted, ContentEvent::Kind::QuestCompleted,
                                                   ContentEvent::Kind::QuestStarted, ContentEvent::Kind::QuestFailed});
}

TEST_CASE("Dialogue: lines, variables, conditional choices, actions and branches", "[content][runner]")
{
    Fixture f;
    REQUIRE(f.runner.startDialogue(kElder));
    REQUIRE(f.runner.inDialogue());
    CHECK(f.runner.dialogueLine()->speakerName == "Elder");
    CHECK(f.runner.dialogueLine()->text == "Hello, Ana.");
    CHECK(std::get<bool>(*f.runner.state().variable("met_elder")));   // the node's action ran
    CHECK_FALSE(f.runner.startDialogue(kElder));                       // one at a time

    f.runner.continueDialogue();
    CHECK(f.runner.dialogueLine()->text == "Will you find 3 gems?");
    // "I have 3 gems already!" is hidden: its condition is false.
    REQUIRE(f.runner.dialogueLine()->choices.size() == 2);
    CHECK(f.runner.dialogueLine()->choices[0].text == "Yes.");
    CHECK(f.runner.dialogueLine()->choices[1].text == "No.");
    f.runner.continueDialogue();   // a line with choices waits for a choice
    CHECK(f.runner.dialogueLine()->node == "ask");

    f.runner.choose(0);
    CHECK(f.runner.state().questState(kGemsQuest) == QuestState::Active);   // the choice's action
    CHECK(f.runner.dialogueLine()->text == "Thank you!");
    f.runner.continueDialogue();
    CHECK_FALSE(f.runner.inDialogue());

    // Talking again: the branch node sends us to a different line.
    f.runner.run({addVar("gems", 2)});
    REQUIRE(f.runner.startDialogue(kElder));
    CHECK(f.runner.dialogueLine()->text == "You have 2 of 3.");
    f.runner.endDialogue();

    using K = ContentEvent::Kind;
    const auto count = [&](K kind) {
        return std::count_if(f.host.events.begin(), f.host.events.end(), [&](const ContentEvent& e) { return e.kind == kind; });
    };
    CHECK(count(K::DialogueStarted) == 2);
    CHECK(count(K::DialogueEnded) == 2);
}

TEST_CASE("Choosing an option shown only when its condition holds", "[content][runner]")
{
    Fixture f;
    f.runner.run({addVar("gems", 5)});
    f.runner.startDialogue(kElder);
    f.runner.continueDialogue();
    REQUIRE(f.runner.dialogueLine()->choices.size() == 3);
    CHECK(f.runner.dialogueLine()->choices[1].text == "I have 5 gems already!");
    f.runner.choose(1);   // leads nowhere: the dialogue ends
    CHECK_FALSE(f.runner.inDialogue());
    f.runner.choose(7);   // nothing to choose: ignored
}

TEST_CASE("Actions the game carries out, and waiting", "[content][runner]")
{
    Fixture f;
    Action wait;
    wait.kind = Action::Kind::Wait;
    wait.seconds = 1.0f;
    Action off;
    off.kind = Action::Kind::SetActive;
    off.active = false;   // no entity: the object the actions belong to
    Action emit;
    emit.kind = Action::Kind::Emit;
    emit.text = "boom";
    Action scene;
    scene.kind = Action::Kind::ChangeScene;
    scene.asset = kMeadow;
    Action log;
    log.kind = Action::Kind::Log;
    log.text = "{hero} waited";

    f.runner.run({off, wait, emit, log, scene}, EntityId{0x42});
    CHECK(f.host.calls == std::vector<std::string>{"off 0000000000000042"});
    CHECK(f.runner.isRunningSequences());
    f.runner.update(0.6f);
    CHECK(f.host.calls.size() == 1);
    f.runner.update(0.6f);
    CHECK(f.host.calls == std::vector<std::string>{"off 0000000000000042", "emit boom", "scene 0000000000004001"});
    CHECK(f.logged("Ana waited"));
    CHECK_FALSE(f.runner.isRunningSequences());

    // Without a game, such actions are reported, not crashed on.
    ContentRunner alone(f.lib, f.log);
    alone.run({emit});
    CHECK(f.logged("\"emit\" needs a running game; ignored."));
}

TEST_CASE("Content that keeps triggering itself is stopped", "[content][runner]")
{
    // A dialogue whose first line starts... a quest whose reward starts the
    // same quest's dialogue again is fine (one dialogue at a time); actions
    // nesting deeper than the limit are cut off with an error.
    ContentLibrary lib = makeLibrary();
    Quest q;
    q.name = "Echo";
    Action again = questAction(Action::Kind::StartQuest, AssetId{0x2009});
    q.rewards = {again};
    lib.addQuest(AssetId{0x2009}, "content/echo.lquest", q);
    Logger log;
    ContentRunner runner(lib, log);
    CHECK(runner.startQuest(AssetId{0x2009}));   // no objectives: completes at once; restarting is refused
    CHECK(runner.state().questState(AssetId{0x2009}) == QuestState::Completed);
}
