#include "Exporting.hpp"

#include "lantern/assets/AssetDatabase.hpp"
#include "lantern/content/ContentLibrary.hpp"
#include "lantern/core/Version.hpp"
#include "lantern/gameplay/Components.hpp"
#include "lantern/scene/Project.hpp"
#include "lantern/scene/SceneFile.hpp"

#include <cstdio>

namespace lantern::editor {

namespace {
constexpr const char* kCategory = "export";
}

ProjectCheck checkProject(const std::filesystem::path& folder, const ComponentRegistry& registry, const Logger& log)
{
    ProjectCheck check;
    const auto report = [&](const FileIssue& issue) {
        if (issue.isError()) {
            ++check.errors;
            log.error(kCategory, issue.toString());
        } else {
            ++check.warnings;
            log.warning(kCategory, issue.toString());
        }
    };
    std::string error;
    const auto project = Project::open(folder, &error);
    if (!project) {
        report({FileIssue::Severity::Error, folder.string(), error});
        return check;
    }
    check.startScene = project->settings().startScene;
    std::error_code ec;
    if (!std::filesystem::exists(project->absolute(check.startScene), ec)) {
        report({FileIssue::Severity::Error, "project.lantern",
                "The start scene " + check.startScene + " does not exist. Open a scene and choose Make Start Scene "
                "(select the scene row in the Hierarchy)."});
    }

    AssetDatabase assets(folder, AssetDatabase::Access::ReadOnly);
    for (const FileIssue& issue : assets.refresh()) {
        report(issue);
    }
    std::vector<FileIssue> issues;
    const ContentLibrary content = ContentLibrary::load(assets, &issues);
    for (FileIssue& issue : content.check()) {
        issues.push_back(std::move(issue));
    }
    for (const AssetRecord* scene : assets.all(AssetType::Scene)) {
        SceneLoadResult loaded = loadSceneFile(assets.absolutePath(*scene), registry);
        for (FileIssue& issue : loaded.issues) {
            issues.push_back(std::move(issue));
        }
        if (!loaded.ok()) {
            continue;
        }
        for (FileIssue& issue : checkAssetReferences(*loaded.scene, assets)) {
            issues.push_back(std::move(issue));
        }
        loaded.scene->forEach([&](const Entity& e, int) {
            const Trigger* t = e.get<Trigger>();
            if (t == nullptr) {
                return;
            }
            const std::string where = scene->path + ", object '" + e.name() + "', Trigger";
            if (t->reaction.condition) {
                content.checkCondition(*t->reaction.condition, where, issues);
            }
            content.checkActions(t->reaction.actions, where, issues);
            for (const Action& a : t->reaction.actions) {
                if ((a.kind == Action::Kind::SetActive || a.kind == Action::Kind::Destroy) && !a.entity.isNull()
                    && !loaded.scene->contains(a.entity)) {
                    issues.push_back({FileIssue::Severity::Error, where,
                                      "An action names object " + a.entity.toString() + ", which is not in this scene."});
                }
            }
        });
    }
    for (const FileIssue& issue : issues) {
        report(issue);
    }
    return check;
}

ExportRun exportProject(const std::filesystem::path& project, const std::filesystem::path& output,
                        const std::string& gameName, const LanternResources& resources,
                        const ComponentRegistry& registry, const Logger& log)
{
    ExportRun run;
    log.info(kCategory, "Checking " + project.string() + " before exporting...");
    run.check = checkProject(project, registry, log);
    if (run.check.errors > 0) {
        run.message = "The game was not exported: the check found " + std::to_string(run.check.errors)
                      + (run.check.errors == 1 ? " error" : " errors")
                      + " (each one is listed in the Console, or above when exporting from the command line). Fix "
                        "them, then export again.";
        log.error(kCategory, run.message);
        return run;
    }
    ExportOptions options;
    options.project = project;
    options.output = output;
    options.gameName = gameName;
    options.player = resources.player;
    options.licenses = resources.licenses;
    options.version = std::string(versionString());
    run.result = exportGame(options);
    if (!run.result.ok) {
        run.message = "The game was not exported: " + run.result.error;
        log.error(kCategory, run.message);
        return run;
    }
    char size[32];
    std::snprintf(size, sizeof size, "%.1f MB", static_cast<double>(run.result.bytes) / (1024.0 * 1024.0));
    run.ok = true;
    run.message = "Exported " + run.result.game.filename().string() + " (" + std::to_string(run.result.files)
                  + " files, " + size + ") to " + run.result.game.parent_path().string() + "."
                  + (run.check.warnings > 0 ? " The check found " + std::to_string(run.check.warnings)
                                                  + " warnings (see the Console)."
                                            : std::string());
    log.info(kCategory, run.message);
    return run;
}

} // namespace lantern::editor
