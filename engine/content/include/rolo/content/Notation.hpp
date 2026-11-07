#pragma once

// A short written form of conditions and actions, for typing them in the
// editor. Files always store the JSON form (Logic.hpp); this is only how
// they are shown and typed:
//
//   conditions                          actions (one per line)
//   always                              set met_elder true
//   met_elder                           add gems 1
//   not met_elder                       startQuest gems
//   gems >= 3                           completeObjective gems return
//   hero == "Ana"                       failQuest gems
//   quest gems is completed             startDialogue elder
//   objective gems collect done         changeScene cave
//   gems >= 3 and (met_elder or rich)   setActive Door off      (an object by name, or "this")
//                                       destroy this
//                                       emit door_opened
//                                       log "Gems: {gems}"
//                                       wait 1.5
//                                       saveGame quick / loadGame quick
//
// Assets (quests, dialogues, scenes) are named by file name without the
// extension ("gems" for content/gems.lquest), or by project path when two
// share a name. Objects are named by their name in the scene (or their ID).
// There are no loops, functions or arithmetic: each line is exactly one
// condition tree or one action, checked as it is read.

#include "rolo/content/Logic.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace rolo {

class AssetDatabase;
class Scene;

/// How names in the written form map to IDs. Any member may be empty
/// (names then cannot be used, and IDs are written as hex).
struct NotationNames {
    /// The name to write for an asset ("gems"), or nothing if unknown.
    std::function<std::optional<std::string>(AssetId)> assetName;
    /// The asset a name refers to; `kind` is "quest", "dialogue" or "scene".
    /// Nothing (with a reason in `error`) if there is none or it is ambiguous.
    std::function<std::optional<AssetId>(std::string_view name, std::string_view kind, std::string& error)> findAsset;
    /// The same for objects in the scene being edited.
    std::function<std::optional<std::string>(EntityId)> entityName;
    std::function<std::optional<EntityId>(std::string_view name, std::string& error)> findEntity;
};

std::string formatCondition(const Condition& condition, const NotationNames& names = {});
std::string formatAction(const Action& action, const NotationNames& names = {});

/// Reads the written form; nothing, with a message in `error` that says what
/// was expected, if it cannot be read. An empty condition is "always".
std::optional<Condition> parseConditionNotation(std::string_view text, const NotationNames& names = {},
                                                std::string* error = nullptr);
std::optional<Action> parseActionNotation(std::string_view text, const NotationNames& names = {},
                                          std::string* error = nullptr);

/// The usual names for a project: assets by file name ("gems", or the path
/// when two files share a name) and objects of `scene` by their name (when
/// unique). Either may be null; neither is owned, and both must outlive the
/// result. Used by the editor's fields and by Lua scripts.
NotationNames projectNotationNames(const AssetDatabase* assets, const Scene* scene);

} // namespace rolo
