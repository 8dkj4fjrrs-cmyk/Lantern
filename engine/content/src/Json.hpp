#pragma once

// Internal to rolo_content: JSON reading and writing shared by the
// content formats (conditions and actions appear inside dialogues, quests,
// triggers and so on).

#include "rolo/content/Logic.hpp"
#include "rolo/scene/Types.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace rolo::content_detail {

using Json = nlohmann::ordered_json;

/// Reads a condition or action from JSON; on failure returns nothing and
/// sets `error` to a message that says what is wrong ("compare needs ...").
std::optional<Condition> conditionFromJson(const Json& j, std::string& error);
std::optional<Action> actionFromJson(const Json& j, std::string& error);
std::optional<std::vector<Action>> actionsFromJson(const Json& j, std::string& error);
Json toJson(const Condition& condition);
Json toJson(const Action& action);
Json toJson(const std::vector<Action>& actions);

std::optional<Value> valueFromJson(const Json& j);
Json toJson(const Value& value);

/// An asset ID field ("5a4d1e0000000010"); null/missing gives the null ID.
std::optional<AssetId> assetIdFromJson(const Json& j);
inline Json toJson(AssetId id)
{
    return id.isNull() ? Json() : Json(id.toString());
}

} // namespace rolo::content_detail
