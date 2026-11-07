// Copyright (c) 2026 Oscar Hird. MIT licence: see LICENSE.

#include "support/TempFolder.hpp"
#include "rolo/assets/Animation.hpp"
#include "rolo/assets/AssetDatabase.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace rolo;
namespace fs = std::filesystem;

namespace {

AnimationClip clip(std::vector<int> frames, float fps, bool loop)
{
    AnimationClip c;
    c.id = "walk";
    c.frames = std::move(frames);
    c.fps = fps;
    c.loop = loop;
    return c;
}

} // namespace

TEST_CASE("A looping clip cycles through its frames", "[animation]")
{
    const AnimationClip walk = clip({4, 5, 6}, 10.0f, true);
    CHECK(walk.duration() == Catch::Approx(0.3));
    CHECK(walk.frameAt(0.0) == 4);
    CHECK(walk.frameAt(0.099) == 4);
    CHECK(walk.frameAt(0.1) == 5);
    CHECK(walk.frameAt(0.25) == 6);
    CHECK(walk.frameAt(0.31) == 4);       // round again
    CHECK(walk.frameAt(99.95) == 4);       // 999.5 frames in: 333 rounds
    CHECK(walk.frameAt(-1.0) == 4);       // before the start: first frame
    CHECK_FALSE(walk.finishedAt(1000.0));
}

TEST_CASE("A clip that does not loop stays on its last frame and finishes", "[animation]")
{
    const AnimationClip attack = clip({1, 2}, 4.0f, false);
    CHECK(attack.frameAt(0.3) == 2);
    CHECK(attack.frameAt(9.0) == 2);
    CHECK_FALSE(attack.finishedAt(0.49));
    CHECK(attack.finishedAt(0.5));

    const AnimationClip empty = clip({}, 8.0f, true);
    CHECK(empty.frameAt(1.0) == -1);
}

TEST_CASE("Frames are found in the sheet like tiles", "[animation]")
{
    AnimationSet set;
    set.frameWidth = 16;
    set.frameHeight = 24;
    set.margin = 1;
    set.spacing = 2;
    // 1 + 16 + 2 + 16 + 2 + 16 + 1 = 54 pixels: three columns.
    CHECK(set.columns(54) == 3);
    CHECK(set.rows(1 + 24 + 2 + 24 + 1) == 2);
    const TileRect r = set.frameRect(4, 54);   // second row, second column
    CHECK(r.x == 1 + 18);
    CHECK(r.y == 1 + 26);
    CHECK(r.width == 16);
    CHECK(r.height == 24);
}

TEST_CASE("Animation sets are saved and read back", "[animation]")
{
    AnimationSet set;
    set.image = AssetId::parse("5a4d1e0000000007").value();
    set.frameWidth = 16;
    set.frameHeight = 24;
    set.clips.push_back(clip({1, 0, 2, 0}, 8.0f, true));
    set.clips.push_back(clip({3}, 1.0f, false));
    set.clips.back().id = "idle_down";

    std::string error;
    const auto back = parseAnimationSet(serializeAnimationSet(set), &error);
    INFO(error);
    REQUIRE(back.has_value());
    CHECK(*back == set);
    REQUIRE(back->findClip("idle_down") != nullptr);
    // Speeds are written as typed.
    const std::string text = serializeAnimationSet(set);
    CHECK(text.find("\"fps\": 8,") != std::string::npos);
    CHECK(text.find("8.0") == std::string::npos);
    CHECK_FALSE(back->findClip("run"));
}

TEST_CASE("Broken animation files are refused with a reason", "[animation]")
{
    const std::string head = R"({"format": "rolo.animations", "version": 1, "image": "5a4d1e0000000007", )"
                             R"("frameSize": [16, 16], )";
    std::string error;
    CHECK_FALSE(parseAnimationSet(R"({"format": "rolo.scene", "version": 1})", &error));
    CHECK_FALSE(error.empty());
    CHECK_FALSE(parseAnimationSet(head + R"("clips": [{"id": "a b", "frames": [0]}]})", &error));
    CHECK_FALSE(parseAnimationSet(head + R"("clips": [{"id": "a", "frames": [0]}, {"id": "a", "frames": [1]}]})", &error));
    CHECK(error.find("a") != std::string::npos);
    CHECK_FALSE(parseAnimationSet(head + R"("clips": [{"id": "a", "frames": [0], "fps": 0}]})", &error));
    CHECK_FALSE(parseAnimationSet(head + R"("clips": [{"id": "a", "frames": [-1]}]})", &error));
    CHECK(parseAnimationSet(head + R"("clips": []})", &error).has_value());
}

TEST_CASE("The asset database creates and reads animation sets", "[animation][assets]")
{
    const fs::path root = rolo::test::uniqueTempFolder("animation-db");
    fs::remove_all(root);
    fs::create_directories(root);
    std::ofstream(root / "project.rolo") << R"({"format": "rolo.project", "version": 1, "name": "T"})";
    {
        AssetDatabase db(root);
        const AssetId image = *db.import(fs::path(ROLO_TEST_DATA_DIR) / "tiles_16px.png");
        std::string error;
        const auto id = db.createAnimationSet(image, {16, 16}, "Hero", &error);
        INFO(error);
        REQUIRE(id.has_value());
        const AssetRecord* record = db.find(*id);
        REQUIRE(record != nullptr);
        CHECK(record->type == AssetType::Animation);
        REQUIRE(record->animation.has_value());
        CHECK(record->animation->image == image);
        REQUIRE(record->animation->findClip("idle") != nullptr);

        // Looking again at an unchanged project keeps the definition (regression:
        // refresh() dropped it, so animations vanished a moment after opening).
        CHECK(db.refresh().empty());
        REQUIRE(db.find(*id)->animation.has_value());
        CHECK(db.find(*id)->animation->findClip("idle") != nullptr);

        AssetDatabase reopened(root);
        CHECK(reopened.refresh().empty());
        REQUIRE(reopened.find(*id) != nullptr);
        CHECK(reopened.find(*id)->animation.has_value());
    }
    fs::remove_all(root);
}

TEST_CASE("An Animator's frame follows the clock, its speed and its clip", "[animation]")
{
    AnimationSet set;
    set.clips.push_back(clip({0, 1, 2, 3}, 4.0f, true));        // "walk"
    set.clips.push_back(clip({7}, 1.0f, true));
    set.clips.back().id = "idle";

    Animator animator;
    animator.clip = "walk";
    animator.startTime = 10.0;
    CHECK(animatorFrame(set, animator, 10.0) == 0);
    CHECK(animatorFrame(set, animator, 10.5) == 2);
    CHECK(animatorFrame(set, animator, 9.0) == 0);       // before it started

    animator.speed = 2.0f;
    CHECK(animatorFrame(set, animator, 10.25) == 2);

    animator.playing = false;                            // stopped: the first frame
    CHECK(animatorFrame(set, animator, 10.75) == 0);

    animator.playing = true;
    animator.clip = "idle";
    CHECK(animatorFrame(set, animator, 12.0) == 7);
    animator.clip.clear();                               // none named: the first clip
    CHECK(animatorClip(set, animator) == &set.clips.front());
    animator.clip = "run";                               // a clip the set lacks
    CHECK(animatorClip(set, animator) == nullptr);
    CHECK(animatorFrame(set, animator, 12.0) == -1);
}
