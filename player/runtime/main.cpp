// Lantern Player — runs a Lantern game in a window.
//
// Plays a Lantern scene: a project's start scene (--project), one scene file
// (--scene), or Lantern's own start scene. The game's behaviours (C++ code
// from the game modules linked into this program) run every frame; doors and
// other gameplay can change to other scenes of the project.

#include "CommandLine.hpp"

#include "lantern/core/Engine.hpp"
#include "lantern/core/Version.hpp"
#include "lantern/gameplay/Game.hpp"
#include "lantern/gameplay/GameModule.hpp"
#include "lantern/runtime/Application.hpp"
#include "lantern/scene/Project.hpp"
#include "lantern/scene/SceneFile.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace {

// Reads a scene file, reporting what it finds wrong to the log.
std::optional<lantern::Scene> loadScene(const std::filesystem::path& path, const lantern::ComponentRegistry& registry,
                                        const lantern::Logger& log)
{
    lantern::SceneLoadResult loaded = lantern::loadSceneFile(path, registry);
    for (const lantern::FileIssue& issue : loaded.issues) {
        if (issue.isError()) {
            log.error("scene", issue.toString());
        } else {
            log.warning("scene", issue.toString());
        }
    }
    if (!loaded.ok()) {
        return std::nullopt;
    }
    return std::move(*loaded.scene);
}

} // namespace

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
    // The component types this game can use: Lantern's own, plus the
    // behaviours of the game modules linked into the player.
    lantern::ComponentRegistry registry = lantern::ComponentRegistry::withBuiltins();
    lantern::registerGameModules(registry, engine.logger());

    std::optional<lantern::Scene> firstScene;
    if (!scenePath.empty()) {
        firstScene = loadScene(scenePath, registry, engine.logger());
        if (!firstScene) {
            engine.logger().error("player", "Could not open the scene " + scenePath.string() + ".");
            return 1;
        }
        engine.logger().info("player", "Playing scene '" + firstScene->name() + "' (" + scenePath.string() + ")");
    }

    // The project's images, tilesets and scenes: from --project, or the
    // project folder the scene file is in (searching upwards for project.lantern).
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
        if (firstScene) {
            for (const lantern::FileIssue& issue : lantern::checkAssetReferences(*firstScene, *assets)) {
                engine.logger().warning("assets", issue.where + ": " + issue.message);
            }
        }
    }

    // The game: plays the scene, loading other scenes of the project when
    // gameplay asks for them.
    lantern::Game game(engine.logger(), assets.get(),
                       [&](lantern::AssetId id, std::string& error) -> std::optional<lantern::Scene> {
                           const lantern::AssetRecord* record = assets != nullptr ? assets->find(id) : nullptr;
                           if (record == nullptr || record->type != lantern::AssetType::Scene) {
                               error = "it is not a scene of this project";
                               return std::nullopt;
                           }
                           auto scene = loadScene(assets->absolutePath(*record), registry, engine.logger());
                           if (!scene) {
                               error = record->path + " could not be read";
                           }
                           return scene;
                       });
    lantern::PresentationSettings presentation;
    if (firstScene) {
        game.start(*firstScene);
        presentation.scene = game.sharedScene();
        for (const lantern::Key key : opt.heldKeys) {
            game.input().press(key);
        }
    }

    lantern::Application app(engine, {.title = title, .width = opt.width, .height = opt.height},
                             std::move(presentation));
    app.setAssets(assets.get());
    if (game.isRunning()) {
        app.setGameInput(&game.input());
        std::shared_ptr<const lantern::Scene> shown = game.sharedScene();
        app.setFrameCallback([&, shown](const lantern::FrameInfo& frame) mutable {
            const float dt = opt.fixedStep ? 1.0f / 60.0f : static_cast<float>(frame.deltaSeconds);
            game.update(dt);
            if (game.sharedScene() != shown) {   // the game changed scene
                shown = game.sharedScene();
                app.showScene(shown);
            }
        });
    }
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
