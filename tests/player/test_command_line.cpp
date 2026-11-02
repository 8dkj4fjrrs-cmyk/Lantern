#include "CommandLine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string_view>
#include <vector>

using lantern::player::parseCommandLine;
using lantern::player::ParseResult;

namespace {
ParseResult parse(std::initializer_list<std::string_view> args)
{
    const std::vector<std::string_view> list(args);
    return parseCommandLine(list);
}
} // namespace

TEST_CASE("No arguments gives the defaults", "[player][cli]")
{
    const ParseResult r = parse({});
    REQUIRE(r.ok());
    CHECK(r.options.width == 1280);
    CHECK(r.options.height == 720);
    CHECK(r.options.maxFrames == 0);
    CHECK(r.options.showStats);
    CHECK(r.options.screenshotPath.empty());
}

TEST_CASE("Window options are read", "[player][cli]")
{
    const ParseResult r = parse({"--title", "My Game", "--width", "800", "--height", "600", "--no-stats"});
    REQUIRE(r.ok());
    CHECK(r.options.title == "My Game");
    CHECK(r.options.width == 800);
    CHECK(r.options.height == 600);
    CHECK_FALSE(r.options.showStats);
}

TEST_CASE("Frame limit and screenshot are read", "[player][cli]")
{
    const ParseResult r = parse({"--frames", "90", "--screenshot", "shot.png"});
    REQUIRE(r.ok());
    CHECK(r.options.maxFrames == 90);
    CHECK(r.options.screenshotPath == "shot.png");
}

TEST_CASE("A screenshot without a frame limit is rejected", "[player][cli]")
{
    const ParseResult r = parse({"--screenshot", "shot.png"});
    CHECK_FALSE(r.ok());
    CHECK(r.error.find("--frames") != std::string::npos);
}

TEST_CASE("Bad numbers are rejected with a helpful message", "[player][cli]")
{
    CHECK_FALSE(parse({"--frames", "zero"}).ok());
    CHECK_FALSE(parse({"--frames", "0"}).ok());
    CHECK_FALSE(parse({"--frames", "-5"}).ok());
    CHECK_FALSE(parse({"--width", "10"}).ok());       // below minimum
    CHECK_FALSE(parse({"--width", "800px"}).ok());    // trailing junk
    CHECK(parse({"--width", "abc"}).error.find("--width") != std::string::npos);
}

TEST_CASE("Options missing their value are rejected", "[player][cli]")
{
    const ParseResult r = parse({"--title"});
    CHECK_FALSE(r.ok());
    CHECK(r.error == "--title needs a value.");
}

TEST_CASE("Unknown options are rejected", "[player][cli]")
{
    const ParseResult r = parse({"--fullscreen"});
    CHECK_FALSE(r.ok());
    CHECK(r.error == "Unknown option: --fullscreen");
}

TEST_CASE("Help and version flags are recognised", "[player][cli]")
{
    CHECK(parse({"--help"}).options.showHelp);
    CHECK(parse({"-h"}).options.showHelp);
    CHECK(parse({"--version"}).options.showVersion);
}

TEST_CASE("A project or a scene can be chosen, not both", "[player][cli]")
{
    CHECK(parse({"--project", "My Game"}).options.projectPath == "My Game");
    CHECK(parse({"--scene", "a.lscene"}).options.scenePath == "a.lscene");
    const auto both = parse({"--project", "p", "--scene", "s"});
    CHECK_FALSE(both.ok());
    CHECK(both.error.find("not both") != std::string::npos);
}

TEST_CASE("Held keys and fixed steps for gameplay tests", "[player][cli]")
{
    const ParseResult r = parse({"--hold", "Right", "--hold", "space", "--fixed-step"});
    REQUIRE(r.ok());
    CHECK(r.options.heldKeys == std::vector<lantern::Key>{lantern::Key::Right, lantern::Key::Space});
    CHECK(r.options.fixedStep);

    const ParseResult bad = parse({"--hold", "Rigth"});
    CHECK_FALSE(bad.ok());
    CHECK(bad.error.find("Rigth") != std::string::npos);
    CHECK(lantern::player::keyFromName("left shift") == lantern::Key::LeftShift);
    CHECK_FALSE(lantern::player::keyFromName("Unknown"));
}

TEST_CASE("Tapped keys and the save folder", "[player][cli]")
{
    const ParseResult r = parse({"--tap", "E@120", "--tap", "down@5-40", "--save-dir", "saves"});
    REQUIRE(r.ok());
    REQUIRE(r.options.tappedKeys.size() == 2);
    using Tap = lantern::player::PlayerOptions::Tap;
    CHECK(r.options.tappedKeys[0] == Tap{lantern::Key::E, 120, 121});
    CHECK(r.options.tappedKeys[1] == Tap{lantern::Key::Down, 5, 40});
    CHECK_FALSE(parse({"--tap", "E@9-3"}).ok());
    CHECK(r.options.saveDir == "saves");
    CHECK_FALSE(parse({"--tap", "E"}).ok());
    CHECK_FALSE(parse({"--tap", "Nope@3"}).ok());
}
