#pragma once

// GameState: the player's progress, separate from the game's content.
//
// Content (variables, dialogues, quests) says what *can* happen; GameState
// records what *has* happened in one playthrough: variable values, how far
// each quest is, objects removed for good (a collected gem stays collected),
// one-time triggers that already fired, the current scene and where the
// player stands. It is exactly what a save game holds, and nothing else: no
// scene content, no editor state, no pointers.
//
// Save file (.lsave), versioned JSON:
//
//     { "format": "lantern.save", "version": 1, "game": "Lantern Sample",
//       "scene": "5a4d1e0000000004", "player": [212, 480],
//       "variables": { "gems": 2, "met_elder": true },
//       "quests": { "5a4d1e0000000010": { "state": "active", "objectives": ["talk"] } },
//       "removed": { "5a4d1e0000000004": ["e17e000000000002"] },
//       "fired": { "5a4d1e0000000004": ["e17e000000000009"] } }

#include "lantern/content/Logic.hpp"
#include "lantern/scene/SceneFile.hpp"   // FileIssue
#include "lantern/scene/Types.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace lantern {

class ContentLibrary;
struct SaveLoadResult;

struct QuestProgress {
    QuestState state = QuestState::NotStarted;
    std::set<std::string> objectivesDone;
    friend bool operator==(const QuestProgress&, const QuestProgress&) = default;
};

// Containers are ordered (std::map, std::set) so a save file lists things in
// the same order every time, and two saves of the same progress are equal.
class GameState {
public:
    /// A new game: every variable at its starting value, no quest started.
    static GameState newGame(const ContentLibrary& content);

    // --- Variables ---------------------------------------------------------------------------
    std::optional<Value> variable(std::string_view id) const;
    void setVariable(std::string_view id, Value value);
    const std::map<std::string, Value, std::less<>>& variables() const noexcept { return m_variables; }

    // --- Quests ------------------------------------------------------------------------------
    QuestState questState(AssetId quest) const;
    bool isObjectiveDone(AssetId quest, std::string_view objective) const;
    QuestProgress& progress(AssetId quest) { return m_quests[quest]; }
    const std::map<AssetId, QuestProgress>& quests() const noexcept { return m_quests; }

    // --- Scenes ------------------------------------------------------------------------------
    AssetId scene;                      // the scene being played (null: not saved as an asset)
    std::optional<Vec2> playerPosition; // where the player stood when saved
    /// Objects removed for good in a scene (e.g. collected), and one-time
    /// triggers that have fired: a scene loaded again leaves them out.
    std::set<EntityId>& removed(AssetId inScene) { return m_removed[inScene]; }
    std::set<EntityId>& fired(AssetId inScene) { return m_fired[inScene]; }
    bool isRemoved(AssetId inScene, EntityId id) const;
    bool hasFired(AssetId inScene, EntityId id) const;

    friend bool operator==(const GameState&, const GameState&) = default;

private:
    friend std::string serializeSave(const GameState&, std::string_view);
    friend SaveLoadResult parseSave(std::string_view, const ContentLibrary&);
    std::map<std::string, Value, std::less<>> m_variables;
    std::map<AssetId, QuestProgress> m_quests;
    std::map<AssetId, std::set<EntityId>> m_removed;
    std::map<AssetId, std::set<EntityId>> m_fired;
};

namespace SaveFormat {
inline constexpr std::string_view kName = "lantern.save";
inline constexpr int kVersion = 1;
inline constexpr std::string_view kExtension = ".lsave";
} // namespace SaveFormat

/// A save file's text. `gameName` is written for people reading the file.
std::string serializeSave(const GameState& state, std::string_view gameName);

struct SaveLoadResult {
    std::optional<GameState> state;
    std::vector<FileIssue> issues;
    bool ok() const noexcept { return state.has_value(); }
};
/// Reads a save file against the game's current content. Progress the
/// content no longer has (a removed variable or quest) is dropped with a
/// warning; a variable whose type changed starts from its starting value;
/// variables added since the save get their starting values. So an old save
/// keeps working after the game is updated.
SaveLoadResult parseSave(std::string_view json, const ContentLibrary& content);

} // namespace lantern
