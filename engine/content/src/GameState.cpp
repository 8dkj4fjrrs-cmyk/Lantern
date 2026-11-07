#include "rolo/content/GameState.hpp"
#include "rolo/core/Names.hpp"

#include "rolo/content/ContentLibrary.hpp"

#include "Json.hpp"

namespace rolo {

using content_detail::Json;

GameState GameState::newGame(const ContentLibrary& content)
{
    GameState state;
    for (const VariableDefinition* v : content.variables()) {
        state.m_variables.emplace(v->id, v->initial);
    }
    return state;
}

std::optional<Value> GameState::variable(std::string_view id) const
{
    const auto it = m_variables.find(id);
    return it != m_variables.end() ? std::optional<Value>(it->second) : std::nullopt;
}

void GameState::setVariable(std::string_view id, Value value)
{
    const auto it = m_variables.find(id);
    if (it != m_variables.end()) {
        it->second = std::move(value);
    } else {
        m_variables.emplace(std::string(id), std::move(value));
    }
}

QuestState GameState::questState(AssetId quest) const
{
    const auto it = m_quests.find(quest);
    return it != m_quests.end() ? it->second.state : QuestState::NotStarted;
}

bool GameState::isObjectiveDone(AssetId quest, std::string_view objective) const
{
    const auto it = m_quests.find(quest);
    return it != m_quests.end() && it->second.objectivesDone.count(std::string(objective)) > 0;
}

bool GameState::isRemoved(AssetId inScene, EntityId id) const
{
    const auto it = m_removed.find(inScene);
    return it != m_removed.end() && it->second.count(id) > 0;
}

bool GameState::hasFired(AssetId inScene, EntityId id) const
{
    const auto it = m_fired.find(inScene);
    return it != m_fired.end() && it->second.count(id) > 0;
}

// --- Save files ----------------------------------------------------------------------------------

namespace {

Json entityLists(const std::map<AssetId, std::set<EntityId>>& lists)
{
    Json j = Json::object();
    for (const auto& [scene, ids] : lists) {
        if (ids.empty()) {
            continue;
        }
        Json list = Json::array();
        for (EntityId id : ids) {
            list.push_back(id.toString());
        }
        j[scene.toString()] = std::move(list);
    }
    return j;
}

void readEntityLists(const Json& j, const char* key, std::map<AssetId, std::set<EntityId>>& out,
                     std::vector<FileIssue>& issues)
{
    if (!j.contains(key)) {
        return;
    }
    if (!j[key].is_object()) {
        issues.push_back({FileIssue::Severity::Warning, key, "Expected scenes and their objects; ignored."});
        return;
    }
    for (auto it = j[key].begin(); it != j[key].end(); ++it) {
        const auto scene = AssetId::parse(it.key());
        if (!scene || !it.value().is_array()) {
            issues.push_back({FileIssue::Severity::Warning, std::string(key) + ", " + it.key(), "Not a scene ID; ignored."});
            continue;
        }
        for (const Json& e : it.value()) {
            const auto id = e.is_string() ? EntityId::parse(e.get<std::string>()) : std::nullopt;
            if (id) {
                out[*scene].insert(*id);
            }
        }
    }
}

} // namespace

std::string serializeSave(const GameState& state, std::string_view gameName)
{
    Json j;
    j["format"] = SaveFormat::kName;
    j["version"] = SaveFormat::kVersion;
    j["game"] = gameName;
    j["scene"] = content_detail::toJson(state.scene);
    j["player"] = state.playerPosition ? Json::array({state.playerPosition->x, state.playerPosition->y}) : Json();
    Json variables = Json::object();
    for (const auto& [id, value] : state.m_variables) {
        variables[id] = content_detail::toJson(value);
    }
    j["variables"] = std::move(variables);
    Json quests = Json::object();
    for (const auto& [id, progress] : state.m_quests) {
        if (progress.state == QuestState::NotStarted && progress.objectivesDone.empty()) {
            continue;
        }
        Json done = Json::array();
        for (const std::string& o : progress.objectivesDone) {
            done.push_back(o);
        }
        quests[id.toString()] = Json{{"state", questStateName(progress.state)}, {"objectives", std::move(done)}};
    }
    j["quests"] = std::move(quests);
    j["removed"] = entityLists(state.m_removed);
    j["fired"] = entityLists(state.m_fired);
    return j.dump(2) + "\n";
}

SaveLoadResult parseSave(std::string_view text, const ContentLibrary& content)
{
    SaveLoadResult result;
    auto& issues = result.issues;
    const auto fail = [&](std::string message) {
        issues.push_back({FileIssue::Severity::Error, "save", std::move(message)});
        return std::move(result);
    };
    Json j;
    try {
        j = Json::parse(text.begin(), text.end());
    } catch (const nlohmann::json::parse_error&) {
        return fail("The save file is damaged (not valid JSON).");
    }
    if (!j.is_object() || !isFormatName(j.value("format", ""), SaveFormat::kName)) {
        return fail("This is not a Rolo save file.");
    }
    if (!j.contains("version") || !j["version"].is_number_integer() || j["version"].get<int>() < 1) {
        return fail("The save file has no valid format version.");
    }
    if (j["version"].get<int>() > SaveFormat::kVersion) {
        return fail("This save was made by a newer version of the game.");
    }

    // Start from a new game, so variables added since the save have their
    // starting values, then put back what was saved.
    GameState state = GameState::newGame(content);
    if (j.contains("scene")) {
        if (const auto scene = content_detail::assetIdFromJson(j["scene"])) {
            state.scene = *scene;
        }
    }
    if (j.contains("player") && j["player"].is_array() && j["player"].size() == 2 && j["player"][0].is_number()
        && j["player"][1].is_number()) {
        state.playerPosition = Vec2{j["player"][0].get<float>(), j["player"][1].get<float>()};
    }
    if (j.contains("variables") && j["variables"].is_object()) {
        for (auto it = j["variables"].begin(); it != j["variables"].end(); ++it) {
            const VariableDefinition* def = content.variable(it.key());
            const auto value = content_detail::valueFromJson(it.value());
            if (def == nullptr) {
                issues.push_back({FileIssue::Severity::Warning, "variable '" + it.key() + "'",
                                  "The game no longer has this variable; its saved value is dropped."});
            } else if (!value || typeOf(*value) != def->type) {
                issues.push_back({FileIssue::Severity::Warning, "variable '" + it.key() + "'",
                                  "The saved value is not a " + std::string(valueTypeName(def->type))
                                      + " value; the variable starts from its starting value."});
            } else {
                state.setVariable(it.key(), *value);
            }
        }
    }
    if (j.contains("quests") && j["quests"].is_object()) {
        for (auto it = j["quests"].begin(); it != j["quests"].end(); ++it) {
            const auto id = AssetId::parse(it.key());
            const Quest* quest = id ? content.quest(*id) : nullptr;
            if (quest == nullptr) {
                issues.push_back({FileIssue::Severity::Warning, "quest " + it.key(),
                                  "The game no longer has this quest; its progress is dropped."});
                continue;
            }
            const Json& q = it.value();
            const auto stateName = q.is_object() ? q.value("state", "") : std::string();
            const auto questState = questStateFromName(stateName);
            if (!questState) {
                issues.push_back({FileIssue::Severity::Warning, "quest " + it.key(), "Unknown quest state; ignored."});
                continue;
            }
            QuestProgress& progress = state.progress(*id);
            progress.state = *questState;
            if (q.contains("objectives") && q["objectives"].is_array()) {
                for (const Json& o : q["objectives"]) {
                    if (o.is_string() && quest->findObjective(o.get<std::string>()) != nullptr) {
                        progress.objectivesDone.insert(o.get<std::string>());
                    }
                }
            }
        }
    }
    readEntityLists(j, "removed", state.m_removed, issues);
    readEntityLists(j, "fired", state.m_fired, issues);
    result.state = std::move(state);
    return result;
}

} // namespace rolo
