// Lantern Editor — entry point.

#include "../model/CommandLine.hpp"
#include "../model/ConsoleLog.hpp"
#include "EditorApp.hpp"

#include "lantern/core/Engine.hpp"
#include "lantern/core/Version.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const lantern::editor::EditorParseResult parsed = lantern::editor::parseEditorCommandLine(args);
    if (!parsed.ok()) {
        std::cerr << "Error: " << parsed.error << "\n\n" << lantern::editor::editorHelpText();
        return 2;
    }
    if (parsed.options.showHelp) {
        std::cout << "Lantern Editor " << lantern::versionString() << "\n\n" << lantern::editor::editorHelpText();
        return 0;
    }
    if (parsed.options.showVersion) {
        std::cout << "Lantern Editor " << lantern::versionString() << " (backend: "
                  << lantern::runtimeBackendName() << ")\n";
        return 0;
    }

    // Declared before the engine so it outlives every message the engine logs.
    lantern::editor::ConsoleLog console;

    lantern::Engine engine({.applicationName = "Lantern Editor"});
    engine.logger().addSink(lantern::makeConsoleSink());   // terminal
    engine.logger().addSink(console.makeSink());           // Console panel

    if (const lantern::Status status = engine.initialize(); !status) {
        engine.logger().error("editor", "Engine failed to start: " + status.message);
        return 1;
    }

    lantern::editor::EditorApp editor(engine, console, parsed.options);
    const lantern::Status status = editor.run();
    if (!status) {
        engine.logger().error("editor", status.message);
    }

    engine.shutdown();
    return status ? 0 : 1;
}
