// Copyright (c) 2026 Oscar Hird. MIT licence: see LICENSE.

#include "support/TempFolder.hpp"

#include "rolo/assets/Animation.hpp"
#include "rolo/assets/AssetDatabase.hpp"
#include "rolo/core/Log.hpp"
#include "rolo/gameplay/Game.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace rolo;

namespace {

// A project with a 16 px sheet and an animation set: "walk" (looping) and
// "attack" (two frames at 10 fps, once).
struct AnimatedProject {
    std::filesystem::path folder = test::uniqueTempFolder("animation-play");
    std::unique_ptr<AssetDatabase> db;
    AssetId set;

    AnimatedProject()
    {
        std::filesystem::create_directories(folder);
        std::ofstream(folder / "project.rolo") << R"({"format": "rolo.project", "version": 1, "name": "T"})";
        db = std::make_unique<AssetDatabase>(folder);
        const AssetId image = *db->import(std::filesystem::path(ROLO_TEST_DATA_DIR) / "tiles_16px.png");
        set = *db->createAnimationSet(image, {16, 16}, "Hero");
        AnimationSet changed = *db->find(set)->animation;
        changed.clips = {AnimationClip{"walk", {0, 1, 2}, 10.0f, true}, AnimationClip{"attack", {3, 4}, 10.0f, false}};
        REQUIRE(db->writeText(set, serializeAnimationSet(changed)));
        REQUIRE(db->find(set)->animation->findClip("attack") != nullptr);
    }
    ~AnimatedProject()
    {
        db.reset();
        std::error_code ec;
        std::filesystem::remove_all(folder, ec);
    }
};

} // namespace

TEST_CASE("Gameplay switches clips without rebuilding the scene", "[gameplay][animation]")
{
    AnimatedProject project;
    Logger log;
    Scene scene;
    const EntityId hero = scene.create("Hero")->id();
    auto animator = std::make_unique<Animator>();
    animator->animations = project.set;
    animator->clip = "walk";
    scene.addComponent(hero, std::move(animator));

    Game game(log, project.db.get());
    game.start(scene);
    game.update(0.05f);
    game.update(0.05f);
    CHECK(game.currentClip(hero) == "walk");
    CHECK(game.isAnimationPlaying(hero));
    CHECK_FALSE(game.isAnimationFinished(hero));   // looping clips never finish

    const auto content = game.scene().contentRevision();
    const auto revision = game.scene().revision();
    REQUIRE(game.play(hero, "attack"));
    CHECK(game.scene().contentRevision() == content);   // only "moved": no rebuild
    CHECK(game.scene().revision() > revision);
    const Animator& playing = *game.scene().find(hero)->get<Animator>();
    CHECK(playing.clip == "attack");
    CHECK(playing.startTime == Catch::Approx(game.time()));

    // Asking again for the clip that plays lets it carry on.
    game.update(0.1f);
    REQUIRE(game.play(hero, "attack"));
    CHECK(game.scene().find(hero)->get<Animator>()->startTime == Catch::Approx(0.1));
    CHECK_FALSE(game.isAnimationFinished(hero));
    game.update(0.1f);
    CHECK(game.isAnimationFinished(hero));          // two frames at 10 fps: 0.2 s

    REQUIRE(game.play(hero, "attack", /*restart=*/true));
    CHECK_FALSE(game.isAnimationFinished(hero));

    game.stopAnimation(hero);
    CHECK_FALSE(game.isAnimationPlaying(hero));
    CHECK_FALSE(game.isAnimationFinished(hero));

    const EntityId plain = game.scene().create("Rock")->id();
    CHECK_FALSE(game.play(plain, "walk"));
    CHECK(game.currentClip(plain).empty());
}

TEST_CASE("Clips start when the scene starts", "[gameplay][animation]")
{
    Logger log;
    Scene scene;
    const EntityId hero = scene.create("Hero")->id();
    auto animator = std::make_unique<Animator>();
    animator->startTime = 42.0;   // whatever the edited scene had
    scene.addComponent(hero, std::move(animator));
    Game game(log, nullptr);
    game.start(scene);
    CHECK(game.scene().find(hero)->get<Animator>()->startTime == 0.0);
}
