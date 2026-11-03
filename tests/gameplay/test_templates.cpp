// The New Project wizard's templates (templates/) and example games: listed,
// copied into a new project as the wizard does, and played.

#include "support/PlayProject.hpp"
#include "LanternResources.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace lantern;
using namespace lantern::editor;
namespace fs = std::filesystem;

namespace {

const fs::path kSource = LANTERN_SOURCE_DIR;

ProjectTemplate find(const std::vector<ProjectTemplate>& all, std::string_view id)
{
    const auto it = std::find_if(all.begin(), all.end(), [&](const ProjectTemplate& t) { return t.id == id; });
    REQUIRE(it != all.end());
    return *it;
}

struct NewProject {
    fs::path folder = test::uniqueTempFolder("from-template") / "My Game";
    ~NewProject()
    {
        std::error_code ec;
        fs::remove_all(folder.parent_path(), ec);
    }
};

std::string all(const std::vector<std::string>& lines)
{
    std::string out;
    for (const std::string& l : lines) {
        out += l + "\n";
    }
    return out;
}

} // namespace

TEST_CASE("The wizard offers the templates and the example game", "[templates]")
{
    const auto r = findLanternResources("/nowhere/lantern_editor", HostOs::Linux, kSource, {});
    const auto list = listProjectTemplates(r);
    REQUIRE(list.size() == 3);
    CHECK(list[0].name == "Empty Project");
    CHECK(list[1].name == "Top-Down Starter");
    CHECK(list[2].name == "Example: The Lantern Keeper");
    CHECK(list[2].example);
}

TEST_CASE("A project made from the Top-Down Starter plays without any C++", "[templates]")
{
    const auto list = listProjectTemplates(findLanternResources("/x/lantern_editor", HostOs::Linux, kSource, {}));
    NewProject made;
    std::string error;
    REQUIRE(createProjectFromTemplate(find(list, "topdown"), made.folder, "My Game", &error));

    test::PlayProject p(made.folder);
    REQUIRE(p.start());
    Game& game = *p.game;
    CHECK(game.scene().name() == "Meadow");
    CHECK(game.content().state().questState(p.assets->findByPath("content/hello.lquest")->id) == QuestState::Active);

    // Walking: the hero plays its walk clip and the camera stays on the map.
    game.input().press(Key::Right);
    p.frames(20);
    CHECK(game.currentClip(p.player()) == "walk_right");
    game.input().release(Key::Right);
    p.frames(1);
    CHECK(game.currentClip(p.player()) == "idle_right");
    CHECK(game.worldPosition(game.find("Camera")).x == Catch::Approx(640.0f));

    // Talking to the friend finishes the first quest.
    const EntityId friendArea = game.scene().childrenOf(game.find("Friend")).front();
    p.stand(game.colliderBox(friendArea)->center());
    p.tap(Key::E);
    REQUIRE(game.isInDialogue());
    for (int i = 0; i < 6 && game.isInDialogue(); ++i) {
        p.tap(Key::Down);   // choose "Bye!"
        p.tap(Key::Down);
        p.tap(Key::E);
    }
    CHECK_FALSE(game.isInDialogue());
    CHECK(game.content().state().questState(p.assets->findByPath("content/hello.lquest")->id) == QuestState::Completed);

    // The path east leads to the grove, arriving at "From Meadow"; and back.
    p.stand(game.colliderBox(game.find("Path to the Grove"))->center());
    p.frames(2);
    REQUIRE(game.scene().name() == "Grove");
    CHECK(game.worldPosition(p.player()).x == Catch::Approx(game.worldPosition(game.find("From Meadow")).x));
    p.stand(game.colliderBox(game.find("Path to the Meadow"))->center());
    p.frames(2);
    REQUIRE(game.scene().name() == "Meadow");
    CHECK(game.worldPosition(p.player()).x == Catch::Approx(game.worldPosition(game.find("From Grove")).x));

    INFO(all(p.errors));
    CHECK(p.errors.empty());
}

TEST_CASE("Projects made from the other templates open and start cleanly", "[templates]")
{
    const auto list = listProjectTemplates(findLanternResources("/x/lantern_editor", HostOs::Linux, kSource, {}));
    for (const char* id : {"empty", "lantern_keeper"}) {
        INFO(id);
        NewProject made;
        std::string error;
        REQUIRE(createProjectFromTemplate(find(list, id), made.folder, "My Game", &error));
        test::PlayProject p(made.folder);
        REQUIRE(p.start());
        INFO(all(p.errors));
        CHECK(p.errors.empty());
    }
}
