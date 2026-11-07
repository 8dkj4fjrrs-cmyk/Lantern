#include "rolo/content/Content.hpp"
#include "rolo/core/Names.hpp"

#include "Json.hpp"

#include <algorithm>
#include <set>

namespace rolo {

using content_detail::Json;

namespace {

// Collects problems while reading one file. Errors stop the file from
// loading (anything less could lose content when it is saved again);
// unknown fields are only warnings.
struct Reader {
    std::vector<FileIssue> issues;
    bool failed = false;

    void error(std::string where, std::string message)
    {
        issues.push_back({FileIssue::Severity::Error, std::move(where), std::move(message)});
        failed = true;
    }
    void warning(std::string where, std::string message)
    {
        issues.push_back({FileIssue::Severity::Warning, std::move(where), std::move(message)});
    }

    /// Warns about fields this version does not know (they are not kept).
    void unknownFields(const Json& j, std::initializer_list<std::string_view> known, const std::string& where)
    {
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (std::find(known.begin(), known.end(), it.key()) == known.end()) {
                warning(where, "Unknown field \"" + it.key() + "\" is ignored (and not kept when saving).");
            }
        }
    }

    std::string text(const Json& j, const char* key, const std::string& where, bool required = false)
    {
        if (!j.contains(key) || j[key].is_null()) {
            if (required) {
                error(where, std::string("\"") + key + "\" is missing.");
            }
            return {};
        }
        if (!j[key].is_string()) {
            error(where, std::string("\"") + key + "\" must be text.");
            return {};
        }
        return j[key].get<std::string>();
    }

    std::string id(const Json& j, const std::string& where, std::set<std::string>& seen)
    {
        const std::string value = text(j, "id", where, true);
        if (failed && value.empty()) {
            return value;
        }
        if (!isValidContentId(value)) {
            error(where, "\"id\" \"" + value + "\" must start with a letter or _ and contain only letters, digits "
                         "and _ (at most 64).");
        } else if (!seen.insert(value).second) {
            error(where, "Another item already has the id \"" + value + "\"; ids must be unique.");
        }
        return value;
    }

    Condition condition(const Json& j, const std::string& where)
    {
        std::string message;
        auto c = content_detail::conditionFromJson(j, message);
        if (!c) {
            error(where, "Condition: " + message + ".");
            return {};
        }
        return *c;
    }

    std::optional<Condition> optionalCondition(const Json& j, const char* key, const std::string& where)
    {
        if (!j.contains(key) || j[key].is_null()) {
            return std::nullopt;
        }
        return condition(j[key], where + ", " + key);
    }

    std::vector<Action> actions(const Json& j, const char* key, const std::string& where)
    {
        if (!j.contains(key) || j[key].is_null()) {
            return {};
        }
        std::string message;
        auto a = content_detail::actionsFromJson(j[key], message);
        if (!a) {
            error(where + ", " + key, message + ".");
            return {};
        }
        return *a;
    }

    /// Checks "format" and "version"; false (with an error) if this is not a
    /// readable file of that kind.
    bool header(const Json& j, std::string_view format)
    {
        if (!j.is_object() || !isFormatName(j.value("format", ""), format)) {
            error("file", "This is not a " + std::string(format) + " file (\"format\" should be \"" + std::string(format)
                              + "\").");
            return false;
        }
        if (!j.contains("version") || !j["version"].is_number_integer() || j["version"].get<int>() < 1) {
            error("file", "The file has no valid format \"version\".");
            return false;
        }
        if (j["version"].get<int>() > ContentFormat::kVersion) {
            error("file", "This file was saved by a newer version of Rolo. Update Rolo to open it.");
            return false;
        }
        return true;
    }
};

std::optional<Json> parseJson(std::string_view text, Reader& reader)
{
    try {
        return Json::parse(text.begin(), text.end());
    } catch (const nlohmann::json::parse_error& e) {
        reader.error("file", std::string("The file is not valid JSON (") + e.what() + ").");
        return std::nullopt;
    }
}

Json header(std::string_view format)
{
    Json j;
    j["format"] = format;
    j["version"] = ContentFormat::kVersion;
    return j;
}

template <class T>
ContentReadResult<T> finish(Reader& reader, T&& content)
{
    ContentReadResult<T> result;
    result.issues = std::move(reader.issues);
    if (!reader.failed) {
        result.content = std::forward<T>(content);
    }
    return result;
}

std::string inQuotes(std::string_view id)
{
    return "'" + std::string(id) + "'";
}

} // namespace

bool isValidContentId(std::string_view id)
{
    if (id.empty() || id.size() > 64) {
        return false;
    }
    const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    if (!letter(id.front())) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [&](char c) { return letter(c) || (c >= '0' && c <= '9'); });
}

const DialogueNode* Dialogue::findNode(std::string_view id) const
{
    const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const DialogueNode& n) { return n.id == id; });
    return it != nodes.end() ? &*it : nullptr;
}

const Speaker* Dialogue::findSpeaker(std::string_view id) const
{
    const auto it = std::find_if(speakers.begin(), speakers.end(), [&](const Speaker& s) { return s.id == id; });
    return it != speakers.end() ? &*it : nullptr;
}

const Objective* Quest::findObjective(std::string_view id) const
{
    const auto it = std::find_if(objectives.begin(), objectives.end(), [&](const Objective& o) { return o.id == id; });
    return it != objectives.end() ? &*it : nullptr;
}

// --- Variables ---------------------------------------------------------------------------------

ContentReadResult<VariableSet> parseVariables(std::string_view text)
{
    Reader r;
    VariableSet set;
    const auto j = parseJson(text, r);
    if (!j || !r.header(*j, ContentFormat::kVariables)) {
        return finish(r, std::move(set));
    }
    r.unknownFields(*j, {"format", "version", "variables"}, "file");
    if (!j->contains("variables") || !(*j)["variables"].is_array()) {
        r.error("file", "\"variables\" must be a list.");
        return finish(r, std::move(set));
    }
    std::set<std::string> seen;
    for (const Json& v : (*j)["variables"]) {
        const std::string where = "variable " + std::to_string(set.variables.size() + 1);
        if (!v.is_object()) {
            r.error(where, "Each variable must be an object.");
            continue;
        }
        VariableDefinition def;
        def.id = r.id(v, where, seen);
        const std::string at = def.id.empty() ? where : "variable " + inQuotes(def.id);
        r.unknownFields(v, {"id", "type", "initial", "description"}, at);
        const auto type = valueTypeFromName(r.text(v, "type", at, true));
        if (!type) {
            r.error(at, "\"type\" must be bool, int or text.");
            continue;
        }
        def.type = *type;
        def.description = r.text(v, "description", at);
        switch (def.type) {
        case ValueType::Bool: def.initial = false; break;
        case ValueType::Int: def.initial = 0; break;
        case ValueType::Text: def.initial = std::string(); break;
        }
        if (v.contains("initial")) {
            const auto value = content_detail::valueFromJson(v["initial"]);
            if (!value || typeOf(*value) != def.type) {
                r.error(at, "\"initial\" must be a " + std::string(valueTypeName(def.type)) + " value.");
                continue;
            }
            def.initial = *value;
        }
        set.variables.push_back(std::move(def));
    }
    return finish(r, std::move(set));
}

std::string serializeVariables(const VariableSet& set)
{
    Json j = header(ContentFormat::kVariables);
    Json list = Json::array();
    for (const VariableDefinition& v : set.variables) {
        Json item;
        item["id"] = v.id;
        item["type"] = valueTypeName(v.type);
        item["initial"] = content_detail::toJson(v.initial);
        if (!v.description.empty()) {
            item["description"] = v.description;
        }
        list.push_back(std::move(item));
    }
    j["variables"] = std::move(list);
    return j.dump(2) + "\n";
}

// --- Dialogue ------------------------------------------------------------------------------------

ContentReadResult<Dialogue> parseDialogue(std::string_view text)
{
    Reader r;
    Dialogue d;
    const auto j = parseJson(text, r);
    if (!j || !r.header(*j, ContentFormat::kDialogue)) {
        return finish(r, std::move(d));
    }
    r.unknownFields(*j, {"format", "version", "speakers", "start", "nodes"}, "file");

    std::set<std::string> speakerIds;
    if (j->contains("speakers")) {
        if (!(*j)["speakers"].is_array()) {
            r.error("file", "\"speakers\" must be a list.");
        } else {
            for (const Json& s : (*j)["speakers"]) {
                const std::string where = "speaker " + std::to_string(d.speakers.size() + 1);
                if (!s.is_object()) {
                    r.error(where, "Each speaker must be an object.");
                    continue;
                }
                Speaker speaker;
                speaker.id = r.id(s, where, speakerIds);
                const std::string at = "speaker " + inQuotes(speaker.id);
                r.unknownFields(s, {"id", "name", "color", "portrait"}, at);
                speaker.name = r.text(s, "name", at);
                if (s.contains("color")) {
                    const auto color = s["color"].is_string() ? Color::fromHex(s["color"].get<std::string>()) : std::nullopt;
                    if (!color) {
                        r.error(at, "\"color\" must be \"#rrggbb\" or \"#rrggbbaa\".");
                    } else {
                        speaker.color = *color;
                    }
                }
                if (s.contains("portrait")) {
                    const auto portrait = content_detail::assetIdFromJson(s["portrait"]);
                    if (!portrait) {
                        r.error(at, "\"portrait\" must be an image asset ID or null.");
                    } else {
                        speaker.portrait = *portrait;
                    }
                }
                d.speakers.push_back(std::move(speaker));
            }
        }
    }

    d.start = r.text(*j, "start", "file");
    if (!j->contains("nodes") || !(*j)["nodes"].is_array()) {
        r.error("file", "\"nodes\" must be a list.");
        return finish(r, std::move(d));
    }
    std::set<std::string> nodeIds;
    for (const Json& n : (*j)["nodes"]) {
        const std::string where = "node " + std::to_string(d.nodes.size() + 1);
        if (!n.is_object()) {
            r.error(where, "Each node must be an object.");
            continue;
        }
        DialogueNode node;
        node.id = r.id(n, where, nodeIds);
        const std::string at = "node " + inQuotes(node.id);
        r.unknownFields(n, {"id", "speaker", "text", "actions", "choices", "branches", "next"}, at);
        node.speaker = r.text(n, "speaker", at);
        node.text = r.text(n, "text", at);
        node.actions = r.actions(n, "actions", at);
        node.next = r.text(n, "next", at);
        if (n.contains("choices")) {
            if (!n["choices"].is_array()) {
                r.error(at, "\"choices\" must be a list.");
            } else {
                std::set<std::string> choiceIds;
                for (const Json& c : n["choices"]) {
                    const std::string cwhere = at + ", choice " + std::to_string(node.choices.size() + 1);
                    if (!c.is_object()) {
                        r.error(cwhere, "Each choice must be an object.");
                        continue;
                    }
                    DialogueChoice choice;
                    choice.id = r.id(c, cwhere, choiceIds);
                    const std::string cat = at + ", choice " + inQuotes(choice.id);
                    r.unknownFields(c, {"id", "text", "condition", "actions", "next"}, cat);
                    choice.text = r.text(c, "text", cat);
                    if (auto cond = r.optionalCondition(c, "condition", cat)) {
                        choice.condition = std::move(*cond);
                    }
                    choice.actions = r.actions(c, "actions", cat);
                    choice.next = r.text(c, "next", cat);
                    node.choices.push_back(std::move(choice));
                }
            }
        }
        if (n.contains("branches")) {
            if (!n["branches"].is_array()) {
                r.error(at, "\"branches\" must be a list.");
            } else {
                for (const Json& b : n["branches"]) {
                    const std::string bat = at + ", branch " + std::to_string(node.branches.size() + 1);
                    if (!b.is_object() || !b.contains("condition")) {
                        r.error(bat, "Each branch needs a \"condition\" and a \"next\" node.");
                        continue;
                    }
                    r.unknownFields(b, {"condition", "next"}, bat);
                    DialogueBranch branch;
                    branch.condition = r.condition(b["condition"], bat);
                    branch.next = r.text(b, "next", bat);
                    node.branches.push_back(std::move(branch));
                }
            }
        }
        d.nodes.push_back(std::move(node));
    }
    return finish(r, std::move(d));
}

std::string serializeDialogue(const Dialogue& d)
{
    using content_detail::toJson;
    Json j = header(ContentFormat::kDialogue);
    Json speakers = Json::array();
    for (const Speaker& s : d.speakers) {
        Json item;
        item["id"] = s.id;
        item["name"] = s.name;
        item["color"] = s.color.toHex();
        item["portrait"] = toJson(s.portrait);
        speakers.push_back(std::move(item));
    }
    j["speakers"] = std::move(speakers);
    j["start"] = d.start;
    Json nodes = Json::array();
    for (const DialogueNode& n : d.nodes) {
        Json item;
        item["id"] = n.id;
        if (!n.speaker.empty()) {
            item["speaker"] = n.speaker;
        }
        if (!n.text.empty()) {
            item["text"] = n.text;
        }
        if (!n.actions.empty()) {
            item["actions"] = toJson(n.actions);
        }
        if (!n.choices.empty()) {
            Json choices = Json::array();
            for (const DialogueChoice& c : n.choices) {
                Json choice;
                choice["id"] = c.id;
                choice["text"] = c.text;
                if (!c.condition.isAlways()) {
                    choice["condition"] = toJson(c.condition);
                }
                if (!c.actions.empty()) {
                    choice["actions"] = toJson(c.actions);
                }
                choice["next"] = c.next.empty() ? Json() : Json(c.next);
                choices.push_back(std::move(choice));
            }
            item["choices"] = std::move(choices);
        }
        if (!n.branches.empty()) {
            Json branches = Json::array();
            for (const DialogueBranch& b : n.branches) {
                branches.push_back(Json{{"condition", toJson(b.condition)}, {"next", b.next}});
            }
            item["branches"] = std::move(branches);
        }
        if (n.choices.empty()) {
            item["next"] = n.next.empty() ? Json() : Json(n.next);
        }
        nodes.push_back(std::move(item));
    }
    j["nodes"] = std::move(nodes);
    return j.dump(2) + "\n";
}

// --- Quest ---------------------------------------------------------------------------------------

ContentReadResult<Quest> parseQuest(std::string_view text)
{
    Reader r;
    Quest q;
    const auto j = parseJson(text, r);
    if (!j || !r.header(*j, ContentFormat::kQuest)) {
        return finish(r, std::move(q));
    }
    r.unknownFields(*j, {"format", "version", "name", "description", "prerequisites", "startWhen", "objectives",
                         "failWhen", "rewards"},
                    "file");
    q.name = r.text(*j, "name", "file");
    q.description = r.text(*j, "description", "file");
    if (j->contains("prerequisites")) {
        if (!(*j)["prerequisites"].is_array()) {
            r.error("file", "\"prerequisites\" must be a list of quest IDs.");
        } else {
            for (const Json& p : (*j)["prerequisites"]) {
                const auto id = content_detail::assetIdFromJson(p);
                if (!id || id->isNull()) {
                    r.error("prerequisites", "Each prerequisite must be a quest's asset ID (16 hex digits).");
                } else {
                    q.prerequisites.push_back(*id);
                }
            }
        }
    }
    q.startWhen = r.optionalCondition(*j, "startWhen", "file");
    q.failWhen = r.optionalCondition(*j, "failWhen", "file");
    q.rewards = r.actions(*j, "rewards", "file");
    if (!j->contains("objectives") || !(*j)["objectives"].is_array()) {
        r.error("file", "\"objectives\" must be a list.");
        return finish(r, std::move(q));
    }
    std::set<std::string> ids;
    for (const Json& o : (*j)["objectives"]) {
        const std::string where = "objective " + std::to_string(q.objectives.size() + 1);
        if (!o.is_object()) {
            r.error(where, "Each objective must be an object.");
            continue;
        }
        Objective objective;
        objective.id = r.id(o, where, ids);
        const std::string at = "objective " + inQuotes(objective.id);
        r.unknownFields(o, {"id", "text", "completeWhen", "optional"}, at);
        objective.text = r.text(o, "text", at);
        objective.completeWhen = r.optionalCondition(o, "completeWhen", at);
        if (o.contains("optional")) {
            if (!o["optional"].is_boolean()) {
                r.error(at, "\"optional\" must be true or false.");
            } else {
                objective.optional = o["optional"].get<bool>();
            }
        }
        q.objectives.push_back(std::move(objective));
    }
    return finish(r, std::move(q));
}

std::string serializeQuest(const Quest& q)
{
    using content_detail::toJson;
    Json j = header(ContentFormat::kQuest);
    j["name"] = q.name;
    j["description"] = q.description;
    Json prerequisites = Json::array();
    for (AssetId id : q.prerequisites) {
        prerequisites.push_back(id.toString());
    }
    j["prerequisites"] = std::move(prerequisites);
    j["startWhen"] = q.startWhen ? toJson(*q.startWhen) : Json();
    Json objectives = Json::array();
    for (const Objective& o : q.objectives) {
        Json item;
        item["id"] = o.id;
        item["text"] = o.text;
        item["completeWhen"] = o.completeWhen ? toJson(*o.completeWhen) : Json();
        item["optional"] = o.optional;
        objectives.push_back(std::move(item));
    }
    j["objectives"] = std::move(objectives);
    j["failWhen"] = q.failWhen ? toJson(*q.failWhen) : Json();
    j["rewards"] = toJson(q.rewards);
    return j.dump(2) + "\n";
}

} // namespace rolo
