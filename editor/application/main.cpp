// Lantern Editor — entry point.
//
// Milestone 0 scope: parse a few command-line flags, start the engine,
// report status, shut down. No window yet (that is Milestone 2).

#include "lantern/core/Engine.hpp"
#include "lantern/core/Version.hpp"

#include <iostream>
#include <span>
#include <string_view>

namespace {

void printHelp()
{
    std::cout << "Lantern Editor " << lantern::versionString() << "\n\n"
              << "Usage: lantern_editor [options]\n\n"
              << "Options:\n"
              << "  --help        Show this message\n"
              << "  --version     Print the version and exit\n"
              << "  --smoke-test  Start and stop the engine, then exit (used by CI)\n";
}

} // namespace

int main(int argc, char** argv)
{
    const std::span<char*> args(argv, static_cast<std::size_t>(argc));

    bool smokeTest = false;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        }
        if (arg == "--version") {
            std::cout << "Lantern Editor " << lantern::versionString() << '\n';
            return 0;
        }
        if (arg == "--smoke-test") {
            smokeTest = true;
            continue;
        }
        std::cerr << "Unknown option: " << arg << "\n\n";
        printHelp();
        return 2;
    }

    lantern::Engine engine({.applicationName = "Lantern Editor"});
    engine.logger().addSink(lantern::makeConsoleSink());

    if (const lantern::Status status = engine.initialize(); !status) {
        engine.logger().error("editor", "Engine failed to start: " + status.message);
        return 1;
    }

    if (smokeTest) {
        engine.logger().info("editor", "Smoke test passed: engine started.");
    } else {
        engine.logger().info("editor",
                             "The editor window is not implemented yet (planned for Milestone 2).");
    }

    engine.shutdown();
    return 0;
}
