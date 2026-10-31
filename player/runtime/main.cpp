// Lantern Player — runs a Lantern game in a window.
//
// Shows a Lantern scene: a project's start scene (--project), one scene file
// (--scene), or Lantern's own start scene. Gameplay code arrives in
// Milestone 5; for now the player draws the scene as it was saved.

#include "CommandLine.hpp"

#include "lantern/core/Engine.hpp"
#include "lantern/core/Version.hpp"
#include "lantern/runtime/Application.hpp"
#include "lantern/scene/Project.hpp"

#include <memory>
#include "lantern/scene/SceneFile.hpp"

#include <filesystem>
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

    // Find the scene to play.
    std::filesystem::path scenePath = opt.scenePath;
    std::string title = opt.title;
    if (!opt.projectPath.empty()) {
        std::string error;
        const auto project = lantern::Project::open(opt.projectPath, &error);
        if (!project) {
            engine.logger().error("player", error);
            return 1;
        }
        scenePath = project->absolute(project->settings().startScene);
        if (title == lantern::player::PlayerOptions{}.title) {
            title = project->settings().name;
        }
    }
    lantern::PresentationSettings presentation;
    if (!scenePath.empty()) {
        const lantern::ComponentRegistry registry = lantern::ComponentRegistry::withBuiltins();
        lantern::SceneLoadResult loaded = lantern::loadSceneFile(scenePath, registry);
        for (const lantern::FileIssue& issue : loaded.issues) {
            if (issue.isError()) {
                engine.logger().error("scene", issue.toString());
            } else {
                engine.logger().warning("scene", issue.toString());
            }
        }
        if (!loaded.ok()) {
            engine.logger().error("player", "Could not open the scene " + scenePath.string() + ".");
            return 1;
        }
        engine.logger().info("player", "Playing scene '" + loaded.scene->name() + "' (" + scenePath.string() + ")");
        presentation.scene = std::make_shared<const lantern::Scene>(std::move(*loaded.scene));
    }

    // The project's images and tilesets: from --project, or the project
    // folder the scene file is in (searching upwards for project.lantern).
    std::filesystem::path projectFolder = opt.projectPath;
    if (projectFolder.empty() && !scenePath.empty()) {
        std::error_code ec;
        for (std::filesystem::path dir = std::filesystem::absolute(scenePath, ec).parent_path(); !dir.empty();
             dir = dir.parent_path()) {
            if (std::filesystem::exists(dir / lantern::ProjectFormat::kFileName, ec)) {
                projectFolder = dir;
                break;
            }
            if (dir == dir.parent_path()) {
                break;
            }
        }
    }
    std::unique_ptr<lantern::AssetDatabase> assets;
    if (!projectFolder.empty()) {
        assets = std::make_unique<lantern::AssetDatabase>(projectFolder, lantern::AssetDatabase::Access::ReadOnly);
        for (const lantern::FileIssue& issue : assets->refresh()) {
            engine.logger().warning("assets", issue.where + ": " + issue.message);
        }
        if (presentation.scene != nullptr) {
            for (const lantern::FileIssue& issue : lantern::checkAssetReferences(*presentation.scene, *assets)) {
                engine.logger().warning("assets", issue.where + ": " + issue.message);
            }
        }
    }

    lantern::Application app(engine, {.title = title, .width = opt.width, .height = opt.height},
                             std::move(presentation));
    app.setAssets(assets.get());
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
