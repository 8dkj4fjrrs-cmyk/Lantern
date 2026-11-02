#pragma once

// The RPG content of a project, as plain data: game variables, dialogues
// and quests. Each lives in its own asset file (with a .meta ID like any
// asset), in versioned JSON:
//
//     variables.lvars     the game's variables and their starting values
//     elder.ldialogue     a conversation: speakers, lines and choices
//     gems.lquest         a quest: objectives, conditions and rewards
//
// These are *definitions*, authored in the editor and shipped with the game.
// What happens while playing (variable values, quest progress) is GameState,
// which save games hold (see GameState.hpp).
//
// Identifiers inside a file (a variable, a dialogue line, an objective) are
// short names chosen when they are created, such as "gems" or "ask_help":
// letters, digits and underscores, starting with a letter or underscore.
// They are saved and referred to, so they are never changed afterwards; the
// text shown to players can change freely. Files refer to each other by
// asset ID. Full description: docs/formats/content.md.

#include "lantern/content/Logic.hpp"
#include "lantern/scene/SceneFile.hpp"   // FileIssue
#include "lantern/scene/Types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lantern {

/// True for a valid identifier: [A-Za-z_][A-Za-z0-9_]*, at most 64 characters.
bool isValidContentId(std::string_view id);

// --- Variables (.lvars) ----------------------------------------------------------------------

struct VariableDefinition {
    std::string id;            // e.g. "gems"
    ValueType type = ValueType::Bool;
    Value initial = false;     // the value a new game starts with (of `type`)
    std::string description;   // for the editor

    friend bool operator==(const VariableDefinition&, const VariableDefinition&) = default;
};

struct VariableSet {
    std::vector<VariableDefinition> variables;
    friend bool operator==(const VariableSet&, const VariableSet&) = default;
};

// --- Dialogue (.ldialogue) -------------------------------------------------------------------

struct Speaker {
    std::string id;            // e.g. "elder"
    std::string name;          // shown to players, e.g. "Village Elder"
    Color color{255, 255, 255, 255};   // the name's colour in the dialogue box
    AssetId portrait;          // an image, or none

    friend bool operator==(const Speaker&, const Speaker&) = default;
};

struct DialogueChoice {
    std::string id;
    std::string text;          // what the player says
    Condition condition;       // shown only when true
    std::vector<Action> actions;   // run when chosen
    std::string next;          // the node to go to; empty = the dialogue ends

    friend bool operator==(const DialogueChoice&, const DialogueChoice&) = default;
};

/// A branch of a branch node: go to `next` if `condition` is true.
struct DialogueBranch {
    Condition condition;
    std::string next;
    friend bool operator==(const DialogueBranch&, const DialogueBranch&) = default;
};

/// One step of a dialogue. A *line* has text (said by `speaker`, which may
/// be empty for narration) and either `choices` or a `next` node. A *branch*
/// has no text: it goes to the first branch whose condition is true, else to
/// `next`. Both run `actions` when they are reached.
struct DialogueNode {
    std::string id;
    std::string speaker;
    std::string text;          // may contain {variable}
    std::vector<Action> actions;
    std::vector<DialogueChoice> choices;
    std::vector<DialogueBranch> branches;
    std::string next;          // empty = the dialogue ends (lines without choices, branches)

    bool isBranch() const noexcept { return text.empty() && !branches.empty(); }
    friend bool operator==(const DialogueNode&, const DialogueNode&) = default;
};

struct Dialogue {
    std::vector<Speaker> speakers;
    std::string start;         // the first node
    std::vector<DialogueNode> nodes;

    const DialogueNode* findNode(std::string_view id) const;
    const Speaker* findSpeaker(std::string_view id) const;
    friend bool operator==(const Dialogue&, const Dialogue&) = default;
};

// --- Quests (.lquest) ------------------------------------------------------------------------

struct Objective {
    std::string id;
    std::string text;          // shown to players, e.g. "Find 3 gems ({gems}/3)"
    std::optional<Condition> completeWhen;   // completes by itself when true; none = only by an action
    bool optional = false;     // not needed to complete the quest

    friend bool operator==(const Objective&, const Objective&) = default;
};

struct Quest {
    std::string name;          // shown to players
    std::string description;
    std::vector<AssetId> prerequisites;   // quests that must be completed first
    std::optional<Condition> startWhen;   // starts by itself when true; none = only by an action
    std::vector<Objective> objectives;    // the quest completes when every required one is complete
    std::optional<Condition> failWhen;    // fails when true; none = never fails by itself
    std::vector<Action> rewards;   // run when completed

    const Objective* findObjective(std::string_view id) const;
    friend bool operator==(const Quest&, const Quest&) = default;
};

// --- Reading and writing ---------------------------------------------------------------------

namespace ContentFormat {
inline constexpr std::string_view kVariables = "lantern.variables";
inline constexpr std::string_view kDialogue = "lantern.dialogue";
inline constexpr std::string_view kQuest = "lantern.quest";
inline constexpr int kVersion = 1;
inline constexpr std::string_view kVariablesExtension = ".lvars";
inline constexpr std::string_view kDialogueExtension = ".ldialogue";
inline constexpr std::string_view kQuestExtension = ".lquest";
} // namespace ContentFormat

/// The result of reading a content file: the content (if it could be read)
/// and every problem found in the file itself. References to other files are
/// checked by ContentLibrary.
template <class T>
struct ContentReadResult {
    std::optional<T> content;
    std::vector<FileIssue> issues;
    bool ok() const noexcept { return content.has_value(); }
};

ContentReadResult<VariableSet> parseVariables(std::string_view json);
ContentReadResult<Dialogue> parseDialogue(std::string_view json);
ContentReadResult<Quest> parseQuest(std::string_view json);
std::string serializeVariables(const VariableSet& variables);
std::string serializeDialogue(const Dialogue& dialogue);
std::string serializeQuest(const Quest& quest);

} // namespace lantern
