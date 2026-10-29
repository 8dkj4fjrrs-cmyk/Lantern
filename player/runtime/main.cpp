// Lantern Player — runs a Lantern game in a window.
//
// Milestone 1: starts the engine, opens a window showing the start scene,
// and shuts everything down cleanly. Game content arrives in later milestones.

#include "CommandLine.hpp"

#include "lantern/core/Engine.hpp"
#include "lantern/core/Version.hpp"
#include "lantern/runtime/Application.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    // Copy the arguments (minus the program name) into a safe, sized list.
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const lantern::player::ParseResult parsed = lantern::player::parseCommandLine(args);
    if (!parsed.ok()) {
        std::cerr << "Error: " << parsed.error << "\n\n" << lantern::player::helpText();
        return 2;
    }
    const lantern::player::PlayerOptions& opt = parsed.options;
    if (opt.showHelp) {
        std::cout << lantern::player::helpText();
        return 0;
    }
    if (opt.showVersion) {
        std::cout << "Lantern Player " << lantern::versionString() << " (backend: "
                  << lantern::runtimeBackendName() << ")\n";
        return 0;
    }

    lantern::Engine engine({.applicationName = opt.title});
    engine.logger().addSink(lantern::makeConsoleSink());

    if (const lantern::Status status = engine.initialize(); !status) {
        engine.logger().error("player", "Engine failed to start: " + status.message);
        return 1;
    }

    lantern::Application app(engine, {.title = opt.title, .width = opt.width, .height = opt.height});
    const lantern::Status runStatus = app.run({
        .maxFrames = opt.maxFrames,
        .screenshotPath = opt.screenshotPath,
        .showStats = opt.showStats,
    });

    if (!runStatus) {
        engine.logger().error("player", runStatus.message);
    }

    engine.shutdown();
    return runStatus ? 0 : 1;
}
