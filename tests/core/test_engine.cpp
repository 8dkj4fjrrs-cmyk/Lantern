#include "lantern/core/Engine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using lantern::Engine;

TEST_CASE("A new engine starts in the Created state", "[core][engine]")
{
    Engine engine;
    CHECK(engine.state() == Engine::State::Created);
    CHECK_FALSE(engine.isRunning());
}

TEST_CASE("Engine config is kept as given", "[core][engine]")
{
    Engine engine({.applicationName = "Test Game"});
    CHECK(engine.config().applicationName == "Test Game");
}

TEST_CASE("initialize() then shutdown() walks through every state", "[core][engine]")
{
    Engine engine;

    REQUIRE(engine.initialize());
    CHECK(engine.state() == Engine::State::Running);

    engine.shutdown();
    CHECK(engine.state() == Engine::State::ShutDown);
}

TEST_CASE("initialize() twice fails with a helpful message", "[core][engine]")
{
    Engine engine;
    REQUIRE(engine.initialize());

    const lantern::Status second = engine.initialize();
    CHECK_FALSE(second.ok);
    CHECK(second.message.find("already running") != std::string::npos);
    CHECK(engine.isRunning()); // the failed call must not change state
}

TEST_CASE("An engine cannot be restarted after shutdown", "[core][engine]")
{
    Engine engine;
    REQUIRE(engine.initialize());
    engine.shutdown();

    const lantern::Status restart = engine.initialize();
    CHECK_FALSE(restart.ok);
    CHECK(engine.state() == Engine::State::ShutDown);
}

TEST_CASE("shutdown() is safe to call repeatedly or without initialize()", "[core][engine]")
{
    Engine neverStarted;
    neverStarted.shutdown();
    CHECK(neverStarted.state() == Engine::State::Created);

    Engine engine;
    REQUIRE(engine.initialize());
    engine.shutdown();
    engine.shutdown();
    CHECK(engine.state() == Engine::State::ShutDown);
}

TEST_CASE("Engine logs startup and shutdown through its own logger", "[core][engine]")
{
    std::vector<std::string> lines;
    {
        Engine engine({.applicationName = "Logged"});
        engine.logger().addSink([&lines](const lantern::LogMessage& m) { lines.push_back(m.text); });
        REQUIRE(engine.initialize());
        // No explicit shutdown: the destructor must do it.
    }

    REQUIRE(lines.size() == 2);
    CHECK(lines[0].find("Starting 'Logged'") != std::string::npos);
    CHECK(lines[1].find("Shut down 'Logged'") != std::string::npos);
}

TEST_CASE("Engine states have readable names", "[core][engine]")
{
    CHECK(lantern::toString(Engine::State::Created) == "Created");
    CHECK(lantern::toString(Engine::State::Running) == "Running");
    CHECK(lantern::toString(Engine::State::ShutDown) == "ShutDown");
}
