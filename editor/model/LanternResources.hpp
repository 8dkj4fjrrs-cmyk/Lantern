#pragma once

// Lantern's own files that the editor uses: project templates, the example
// games, license texts, the documentation, and the game runtime that
// exported games are made from.
//
// They are found from where the editor program is:
//
//   an installed copy (a download)       Lantern-<version>/lantern_editor
//                                        Lantern-<version>/templates, examples, licenses, docs
//                                        Lantern-<version>/runtime/lantern_player
//   the same on macOS                    Lantern Editor.app/Contents/Resources/lantern/...
//                                        .../lantern/runtime/lantern_player.app
//   a build folder (developing Lantern)  the source folder's templates/, examples/, ...
//                                        and the lantern_player built beside the editor
//
// See docs/releasing.md for how an installed copy is made.

#include "EditorPaths.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace lantern::editor {

struct LanternResources {
    std::filesystem::path root;        // the folder they were found in (for messages)
    std::filesystem::path templates;   // project templates, one folder each
    std::filesystem::path examples;    // example games
    std::filesystem::path licenses;    // license texts shipped with every game
    std::filesystem::path docs;
    std::filesystem::path player;      // lantern_player(.exe), or lantern_player.app on macOS; empty if unknown
    bool installed = false;            // false: running from a build folder
};

/// Finds the resources for an editor program at `executable`. `sourceDir`
/// and `devPlayer` describe the build it came from (used when the program
/// is not in an installed copy).
LanternResources findLanternResources(const std::filesystem::path& executable, HostOs os,
                                      const std::filesystem::path& sourceDir,
                                      const std::filesystem::path& devPlayer);

/// A starting point for a new project: a template or an example game.
struct ProjectTemplate {
    std::string id;            // the folder's name, e.g. "topdown"
    std::string name;          // shown in the wizard, e.g. "Top-Down Starter"
    std::string description;
    std::filesystem::path project;   // the project folder to copy
    bool example = false;      // an example game rather than a template
    int order = 100;           // position in the list (lower first)
};

/// The templates and example games in `resources`. Each is a folder holding
/// `template.json` ({"name", "description", "order"}) and the `project/`
/// folder to copy. Sorted: templates first, then examples, each by order.
std::vector<ProjectTemplate> listProjectTemplates(const LanternResources& resources);

/// Makes a new project in `folder` (which must not exist, or be empty) as a
/// copy of the template's project, named `name`. Nothing (and `error` set)
/// if it could not.
bool createProjectFromTemplate(const ProjectTemplate& from, const std::filesystem::path& folder,
                               const std::string& name, std::string* error = nullptr);

} // namespace lantern::editor
