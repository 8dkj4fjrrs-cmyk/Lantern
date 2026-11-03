// The Inspector's pages for RPG content: game variables, dialogues, quests,
// and the condition and actions of a Trigger.
//
// Content files are edited as forms. Conditions and actions are typed in
// their short written form (see lantern/content/Notation.hpp), checked as
// soon as a field is left; a mistake is shown under the field and nothing is
// changed. Every change is one undo step (ChangeAssetText for content files,
// ReplaceComponent for triggers) and is saved to the file at once.

#include "Panels.hpp"

#include "../model/Commands.hpp"

#include "lantern/gameplay/Components.hpp"
#include "lantern/ui/Context.hpp"
#include "lantern/ui/widgets/Menu.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace lantern::editor {

namespace {

constexpr float kRowHeight = 22.0f;

ui::Color toUi(Color c)
{
    return ui::Color{c.r, c.g, c.b, c.a};
}

Color fromUi(ui::Color c)
{
    return Color{c.r, c.g, c.b, c.a};
}

ui::TextField& textField(ui::Widget& parent, const std::string& text, const std::string& placeholder = {})
{
    auto& field = parent.emplace<ui::TextField>(placeholder);
    field.setText(text);
    field.setStretch(1.0f);
    field.setFixedSize({0.0f, kRowHeight});
    return field;
}

ui::Label& errorLabel(ui::Widget& parent)
{
    auto& label = parent.emplace<ui::Label>("", ui::Label::Style::Small);
    label.setColor(ui::Color::hex(0xF26B65));
    label.setWrap(true);
    label.setVisible(false);
    return label;
}

ui::Button& removeButton(ui::Widget& parent, std::function<void()> action, std::string tooltip)
{
    auto& b = parent.emplace<ui::Button>("");
    b.setIcon(ui::Icon::Close);
    b.setStyle(ui::Button::Style::Subtle);
    b.setFixedSize({24.0f, kRowHeight});
    b.setTooltip(std::move(tooltip));
    b.setOnClick(std::move(action));
    return b;
}

/// The next free id like "line_3" among `taken`.
std::string freeId(std::string_view stem, const std::set<std::string>& taken)
{
    for (int i = 1;; ++i) {
        std::string id = std::string(stem) + "_" + std::to_string(i);
        if (taken.count(id) == 0) {
            return id;
        }
    }
}

} // namespace

// --- Shared pieces ---------------------------------------------------------------------------------

ui::Widget& InspectorPanel::keyed(std::string key, ui::Widget& widget)
{
    m_keyed[std::move(key)] = &widget;
    return widget;
}

NotationNames InspectorPanel::notationNames() const
{
    return projectNotationNames(m_editor.assets(), &m_editor.document().scene());
}

void InspectorPanel::conditionField(ui::Widget& parent, const std::string& key, const std::optional<Condition>& value,
                                    const std::string& placeholder, std::function<void(std::optional<Condition>)> apply)
{
    auto& column = parent.emplace<ui::Column>(2.0f);
    column.setStretch(1.0f);
    const NotationNames names = notationNames();
    auto& field = textField(column, value ? formatCondition(*value, names) : std::string(), placeholder);
    field.setTooltip("A condition, e.g.  gems >= 3   met_elder   quest gems is completed   not (a or b)\n"
                     "Empty: " + placeholder + ".");
    keyed(key, field);
    auto& error = errorLabel(column);
    field.setOnCommit([this, &error, apply = std::move(apply)](const std::string& text) {
        std::string message;
        const bool empty = text.find_first_not_of(" \t") == std::string::npos;
        const auto parsed = empty ? std::optional<Condition>(Condition{}) : parseConditionNotation(text, notationNames(), &message);
        if (!parsed) {
            error.setText(message);
            error.setVisible(true);
            return;
        }
        error.setVisible(false);
        apply(empty ? std::nullopt : parsed);
    });
}

void InspectorPanel::actionList(ui::Column& parent, const std::string& key, const std::vector<Action>& actions,
                                std::function<void(std::vector<Action>)> apply)
{
    const NotationNames names = notationNames();
    auto shared = std::make_shared<std::vector<Action>>(actions);
    auto applyShared = std::make_shared<std::function<void(std::vector<Action>)>>(std::move(apply));
    for (std::size_t i = 0; i < actions.size(); ++i) {
        auto& row = parent.emplace<ui::Row>(4.0f);
        auto& field = textField(row, formatAction(actions[i], names));
        keyed(key + "/" + std::to_string(i), field);
        removeButton(row, [shared, applyShared, i] {
            auto copy = *shared;
            copy.erase(copy.begin() + static_cast<std::ptrdiff_t>(i));
            (*applyShared)(std::move(copy));
        }, "Remove this action");
        auto& error = errorLabel(parent);
        field.setOnCommit([this, shared, applyShared, i, &error](const std::string& text) {
            std::string message;
            const auto parsed = parseActionNotation(text, notationNames(), &message);
            if (!parsed) {
                error.setText(message);
                error.setVisible(true);
                return;
            }
            error.setVisible(false);
            auto copy = *shared;
            copy[i] = *parsed;
            (*applyShared)(std::move(copy));
        });
    }
    auto& add = parent.emplace<ui::Row>(4.0f);
    auto& field = textField(add, "", "Add an action, e.g. add gems 1");
    field.setTooltip("Type an action and press Enter: set, add, startQuest, completeObjective, failQuest, "
                     "startDialogue, changeScene, setActive, destroy, emit, log, wait, saveGame, loadGame.");
    keyed(key + "/new", field);
    auto& error = errorLabel(parent);
    field.setOnCommit([this, shared, applyShared, &error](const std::string& text) {
        if (text.find_first_not_of(" \t") == std::string::npos) {
            error.setVisible(false);
            return;
        }
        std::string message;
        const auto parsed = parseActionNotation(text, notationNames(), &message);
        if (!parsed) {
            error.setText(message);
            error.setVisible(true);
            return;
        }
        auto copy = *shared;
        copy.push_back(*parsed);
        (*applyShared)(std::move(copy));
    });
}

// --- Content files -------------------------------------------------------------------------------

namespace {

// Reads a content file, changes it, and saves it as one undo step.
template <class T>
void editFile(EditorServices& editor, AssetId id, const std::string& label,
              ContentReadResult<T> (*parse)(std::string_view), std::string (*serialize)(const T&),
              const std::function<void(T&)>& change)
{
    AssetDatabase* db = editor.assets();
    const auto text = db != nullptr ? db->readText(id) : std::nullopt;
    auto read = text ? parse(*text) : ContentReadResult<T>{};
    if (!read.content) {
        editor.showMessage("Cannot Edit", "The file has errors that must be fixed first (see the Inspector).");
        return;
    }
    change(*read.content);
    editor.endEdit();
    editor.execute(std::make_unique<ChangeAssetText>(id, serialize(*read.content), label));
    editor.endEdit();
}

} // namespace

void InspectorPanel::buildContentAsset(ui::Column& content, const AssetRecord& asset)
{
    AssetDatabase* db = m_editor.assets();
    const auto text = db != nullptr ? db->readText(asset.id) : std::nullopt;
    if (!text) {
        return;
    }
    // Problems in this file, as the game will see them.
    std::vector<FileIssue> issues;
    const auto loadedIssues = [&](std::vector<FileIssue> fileIssues) {
        for (FileIssue& i : fileIssues) {
            i.where = i.where == "file" ? asset.path : asset.path + ", " + i.where;
            issues.push_back(std::move(i));
        }
    };
    std::optional<VariableSet> vars;
    std::optional<Dialogue> dialogue;
    std::optional<Quest> quest;
    switch (asset.type) {
    case AssetType::Variables: {
        auto r = parseVariables(*text);
        loadedIssues(r.issues);
        vars = std::move(r.content);
        break;
    }
    case AssetType::Dialogue: {
        auto r = parseDialogue(*text);
        loadedIssues(r.issues);
        dialogue = std::move(r.content);
        break;
    }
    case AssetType::Quest: {
        auto r = parseQuest(*text);
        loadedIssues(r.issues);
        quest = std::move(r.content);
        break;
    }
    default: return;
    }
    const ContentLibrary library = ContentLibrary::load(*db);
    for (FileIssue& i : library.check()) {
        if (i.where.rfind(asset.path, 0) == 0) {
            issues.push_back(std::move(i));
        }
    }
    if (!issues.empty()) {
        auto& box = content.emplace<ui::Section>(std::to_string(issues.size()) + (issues.size() == 1 ? " problem" : " problems"));
        for (const FileIssue& i : issues) {
            const std::string where = i.where.size() > asset.path.size() + 2 ? i.where.substr(asset.path.size() + 2) : std::string();
            auto& label = box.body().emplace<ui::Label>((where.empty() ? std::string() : where + ": ") + i.message,
                                                        ui::Label::Style::Small);
            label.setWrap(true);
            label.setColor(i.isError() ? ui::Color::hex(0xF26B65) : ui::Color::hex(0xE5B567));
        }
    }
    if (vars) {
        buildVariablesPage(content, asset.id, *vars);
    } else if (dialogue) {
        buildDialoguePage(content, asset.id, *dialogue);
    } else if (quest) {
        buildQuestPage(content, asset.id, *quest);
    } else {
        auto& note = content.emplace<ui::Label>("The file cannot be read until the errors above are fixed (in a text "
                                                "editor). Lantern does not change it meanwhile.",
                                                ui::Label::Style::Dim);
        note.setWrap(true);
    }
}

void InspectorPanel::buildVariablesPage(ui::Column& content, AssetId id, const VariableSet& vars)
{
    const auto edit = [this, id](const std::string& label, std::function<void(VariableSet&)> change) {
        editFile<VariableSet>(m_editor, id, label, &parseVariables, &serializeVariables, change);
    };
    auto& section = content.emplace<ui::Section>("Game Variables");
    auto& intro = section.body().emplace<ui::Label>("Values the game remembers, e.g. how many gems the player has. Text "
                                                    "shows a value with {name}.",
                                                    ui::Label::Style::Faint);
    intro.setWrap(true);
    for (std::size_t i = 0; i < vars.variables.size(); ++i) {
        const VariableDefinition& v = vars.variables[i];
        auto& row = section.body().emplace<ui::PropertyField>(v.id);
        auto& type = row.addValue(std::make_unique<ui::ChoiceField>(std::vector<std::string>{"bool", "int", "text"}));
        type.setSelected(static_cast<int>(v.type));
        type.setFixedSize({64.0f, kRowHeight});
        type.setTooltip("bool: true or false · int: a whole number · text");
        type.setOnChoose([edit, i](int choice) {
            edit("Change Variable Type", [i, choice](VariableSet& s) {
                VariableDefinition& d = s.variables[i];
                d.type = static_cast<ValueType>(choice);
                d.initial = d.type == ValueType::Bool ? Value{false} : d.type == ValueType::Int ? Value{0} : Value{std::string()};
            });
        });
        if (v.type == ValueType::Bool) {
            auto& check = row.addValue(std::make_unique<ui::Checkbox>("starts true", std::get<bool>(v.initial)));
            check.setStretch(1.0f);
            check.setOnChange([edit, i](bool on) {
                edit("Change Starting Value", [i, on](VariableSet& s) { s.variables[i].initial = on; });
            });
        } else if (v.type == ValueType::Int) {
            auto& number = row.addValue(std::make_unique<ui::NumberField>());
            number.setInteger(true);
            number.setValue(static_cast<float>(std::get<int>(v.initial)));
            number.setStretch(1.0f);
            keyed("var/" + v.id, number);
            auto value = std::make_shared<int>(std::get<int>(v.initial));
            number.setOnValue([value](float f) { *value = static_cast<int>(std::lround(f)); });
            number.setOnEditEnd([edit, i, value] {
                edit("Change Starting Value", [i, value](VariableSet& s) { s.variables[i].initial = *value; });
            });
        } else {
            auto& field = row.addValue(std::make_unique<ui::TextField>("starting text"));
            field.setText(std::get<std::string>(v.initial));
            field.setStretch(1.0f);
            keyed("var/" + v.id, field);
            field.setOnCommit([edit, i](const std::string& text) {
                edit("Change Starting Value", [i, text](VariableSet& s) { s.variables[i].initial = text; });
            });
        }
        removeButton(row, [edit, i] { edit("Remove Variable", [i](VariableSet& s) { s.variables.erase(s.variables.begin() + static_cast<std::ptrdiff_t>(i)); }); },
                     "Remove this variable (content that uses it will report it as missing)");
        row.setTooltip(v.description.empty() ? "Starting value for a new game" : v.description);
    }
    auto& add = section.body().emplace<ui::Row>(4.0f);
    auto& name = textField(add, "", "new_variable");
    keyed("var/new", name);
    auto& error = errorLabel(section.body());
    name.setOnCommit([this, edit, &error](const std::string& text) {
        if (text.empty()) {
            return;
        }
        if (!isValidContentId(text)) {
            error.setText("Use letters, digits and _, starting with a letter or _ (e.g. gems, met_elder).");
            error.setVisible(true);
            return;
        }
        ContentLibrary library = m_editor.assets() != nullptr ? ContentLibrary::load(*m_editor.assets()) : ContentLibrary{};
        if (library.variable(text) != nullptr) {
            error.setText("There is already a variable called " + text + ".");
            error.setVisible(true);
            return;
        }
        edit("Add Variable", [text](VariableSet& s) { s.variables.push_back({text, ValueType::Int, 0, ""}); });
    });
    name.setTooltip("Type the new variable's name and press Enter, e.g. gems or met_elder");
}

void InspectorPanel::buildQuestPage(ui::Column& content, AssetId id, const Quest& quest)
{
    const auto edit = [this, id](const std::string& label, std::function<void(Quest&)> change) {
        editFile<Quest>(m_editor, id, label, &parseQuest, &serializeQuest, change);
    };
    auto& main = content.emplace<ui::Section>("Details");
    const auto textRow = [&](ui::Section& s, const char* label, const std::string& value, const std::string& key,
                             std::function<void(Quest&, const std::string&)> set) {
        auto& row = s.body().emplace<ui::PropertyField>(label);
        auto& field = row.addValue(std::make_unique<ui::TextField>());
        field.setText(value);
        field.setStretch(1.0f);
        keyed(key, field);
        field.setOnCommit([edit, set, label](const std::string& text) {
            edit(std::string("Change Quest ") + label, [&](Quest& q) { set(q, text); });
        });
    };
    textRow(main, "Name", quest.name, "q/name", [](Quest& q, const std::string& t) { q.name = t; });
    textRow(main, "Description", quest.description, "q/desc", [](Quest& q, const std::string& t) { q.description = t; });
    auto& startRow = main.body().emplace<ui::PropertyField>("Starts When");
    conditionField(startRow, "q/start", quest.startWhen, "only by a startQuest action",
                   [edit](std::optional<Condition> c) { edit("Change Start Condition", [&](Quest& q) { q.startWhen = c; }); });
    auto& failRow = main.body().emplace<ui::PropertyField>("Fails When");
    conditionField(failRow, "q/fail", quest.failWhen, "never by itself",
                   [edit](std::optional<Condition> c) { edit("Change Fail Condition", [&](Quest& q) { q.failWhen = c; }); });

    // Prerequisites.
    auto& pre = content.emplace<ui::Section>("Prerequisites");
    AssetDatabase* db = m_editor.assets();
    const NotationNames names = notationNames();
    for (std::size_t i = 0; i < quest.prerequisites.size(); ++i) {
        auto& row = pre.body().emplace<ui::Row>(4.0f);
        const auto name = names.assetName ? names.assetName(quest.prerequisites[i]) : std::nullopt;
        auto& label = row.emplace<ui::Label>(name ? *name : quest.prerequisites[i].toString() + " (missing)");
        label.setStretch(1.0f);
        removeButton(row, [edit, i] {
            edit("Remove Prerequisite", [i](Quest& q) { q.prerequisites.erase(q.prerequisites.begin() + static_cast<std::ptrdiff_t>(i)); });
        }, "Remove this prerequisite");
    }
    if (db != nullptr) {
        std::vector<std::string> options{"Add a quest that must be completed first…"};
        auto ids = std::make_shared<std::vector<AssetId>>();
        for (const AssetRecord* a : db->all(AssetType::Quest)) {
            if (a->id != id && std::find(quest.prerequisites.begin(), quest.prerequisites.end(), a->id) == quest.prerequisites.end()) {
                options.push_back(names.assetName ? names.assetName(a->id).value_or(a->path) : a->path);
                ids->push_back(a->id);
            }
        }
        auto& choose = pre.body().emplace<ui::ChoiceField>(std::move(options));
        choose.setStretch(1.0f);
        choose.setOnChoose([edit, ids](int index) {
            if (index > 0) {
                const AssetId chosen = (*ids)[static_cast<std::size_t>(index - 1)];
                edit("Add Prerequisite", [chosen](Quest& q) { q.prerequisites.push_back(chosen); });
            }
        });
    }

    // Objectives.
    auto& objectives = content.emplace<ui::Section>("Objectives");
    std::set<std::string> taken;
    for (std::size_t i = 0; i < quest.objectives.size(); ++i) {
        const Objective& o = quest.objectives[i];
        taken.insert(o.id);
        auto& head = objectives.body().emplace<ui::Row>(4.0f, ui::Insets{0.0f, 6.0f, 0.0f, 0.0f});
        auto& id_label = head.emplace<ui::Label>(o.id, ui::Label::Style::Strong);
        id_label.setStretch(1.0f);
        id_label.setTooltip("The objective's id: used by conditions and the completeObjective action");
        auto& optional = head.emplace<ui::Checkbox>("Optional", o.optional);
        optional.setOnChange([edit, i](bool on) { edit("Change Objective", [i, on](Quest& q) { q.objectives[i].optional = on; }); });
        removeButton(head, [edit, i] {
            edit("Remove Objective", [i](Quest& q) { q.objectives.erase(q.objectives.begin() + static_cast<std::ptrdiff_t>(i)); });
        }, "Remove this objective");
        auto& textRowField = objectives.body().emplace<ui::PropertyField>("Text");
        auto& text = textRowField.addValue(std::make_unique<ui::TextField>("What the player sees, e.g. Find 3 gems ({gems}/3)"));
        text.setText(o.text);
        text.setStretch(1.0f);
        keyed("o/" + o.id + "/text", text);
        text.setOnCommit([edit, i](const std::string& t) { edit("Change Objective Text", [&](Quest& q) { q.objectives[i].text = t; }); });
        auto& done = objectives.body().emplace<ui::PropertyField>("Done When");
        conditionField(done, "o/" + o.id + "/when", o.completeWhen, "only by a completeObjective action",
                       [edit, i](std::optional<Condition> c) {
                           edit("Change Objective Condition", [&](Quest& q) { q.objectives[i].completeWhen = c; });
                       });
    }
    auto& add = objectives.body().emplace<ui::Flow>(6.0f, 6.0f, ui::Insets{0.0f, 6.0f, 0.0f, 0.0f});
    add.emplace<ui::Button>("Add Objective", [edit, taken] {
        const std::string newId = freeId("objective", taken);
        edit("Add Objective", [newId](Quest& q) { q.objectives.push_back({newId, "", std::nullopt, false}); });
    });

    // Rewards.
    auto& rewards = content.emplace<ui::Section>("Rewards (when completed)");
    actionList(rewards.body(), "q/rewards", quest.rewards,
               [edit](std::vector<Action> a) { edit("Change Rewards", [&](Quest& q) { q.rewards = std::move(a); }); });
}

void InspectorPanel::buildDialoguePage(ui::Column& content, AssetId id, const Dialogue& d)
{
    const auto edit = [this, id](const std::string& label, std::function<void(Dialogue&)> change) {
        editFile<Dialogue>(m_editor, id, label, &parseDialogue, &serializeDialogue, change);
    };
    // Speakers.
    auto& speakers = content.emplace<ui::Section>("Speakers");
    std::set<std::string> speakerIds;
    for (std::size_t i = 0; i < d.speakers.size(); ++i) {
        const Speaker& s = d.speakers[i];
        speakerIds.insert(s.id);
        auto& row = speakers.body().emplace<ui::PropertyField>(s.id);
        auto& name = row.addValue(std::make_unique<ui::TextField>("Name shown to players"));
        name.setText(s.name);
        name.setStretch(1.0f);
        keyed("sp/" + s.id, name);
        name.setOnCommit([edit, i](const std::string& t) { edit("Change Speaker", [&](Dialogue& x) { x.speakers[i].name = t; }); });
        auto& color = row.addValue(std::make_unique<ui::ColorField>());
        color.setValue(toUi(s.color));
        color.setFixedSize({70.0f, kRowHeight});
        auto picked = std::make_shared<Color>(s.color);
        color.setOnValue([picked](ui::Color c) { *picked = fromUi(c); });
        color.setOnEditEnd([edit, i, picked] { edit("Change Speaker Colour", [&](Dialogue& x) { x.speakers[i].color = *picked; }); });
        removeButton(row, [edit, i] {
            edit("Remove Speaker", [i](Dialogue& x) { x.speakers.erase(x.speakers.begin() + static_cast<std::ptrdiff_t>(i)); });
        }, "Remove this speaker");
    }
    auto& addSpeaker = speakers.body().emplace<ui::Flow>(6.0f, 6.0f);
    addSpeaker.emplace<ui::Button>("Add Speaker", [edit, speakerIds] {
        const std::string newId = freeId("speaker", speakerIds);
        edit("Add Speaker", [newId](Dialogue& x) { x.speakers.push_back({newId, "Someone", Color{255, 255, 255, 255}, {}}); });
    });

    // Where it starts.
    std::vector<std::string> nodeNames;
    std::set<std::string> nodeIds;
    for (const DialogueNode& n : d.nodes) {
        nodeNames.push_back(n.id);
        nodeIds.insert(n.id);
    }
    // A choice of node, with an "end" option first.
    const auto nodeChoice = [&](ui::Widget& parent, const std::string& current, const char* endLabel,
                                std::function<void(std::string)> set) {
        std::vector<std::string> options{endLabel};
        options.insert(options.end(), nodeNames.begin(), nodeNames.end());
        auto& choice = parent.emplace<ui::ChoiceField>(std::move(options));
        choice.setStretch(1.0f);
        const auto it = std::find(nodeNames.begin(), nodeNames.end(), current);
        choice.setSelected(it == nodeNames.end() ? 0 : static_cast<int>(it - nodeNames.begin()) + 1);
        auto names = std::make_shared<std::vector<std::string>>(nodeNames);
        choice.setOnChoose([names, set](int index) {
            set(index <= 0 ? std::string() : (*names)[static_cast<std::size_t>(index - 1)]);
        });
        return &choice;
    };
    auto& top = content.emplace<ui::Section>("Conversation");
    auto& startRow = top.body().emplace<ui::PropertyField>("Starts At");
    nodeChoice(startRow, d.start, "(nothing)", [edit](std::string node) {
        edit("Change Start", [&](Dialogue& x) { x.start = node; });
    });
    auto& tip = top.body().emplace<ui::Label>("Lines are said by a speaker and lead to the next line, or offer choices. "
                                              "Branches have no text: they go to the first line whose condition is true.",
                                              ui::Label::Style::Faint);
    tip.setWrap(true);

    // Speakers as a choice.
    std::vector<std::string> speakerOptions{"(narration)"};
    for (const Speaker& s : d.speakers) {
        speakerOptions.push_back(s.name.empty() ? s.id : s.name + "  (" + s.id + ")");
    }

    for (std::size_t n = 0; n < d.nodes.size(); ++n) {
        const DialogueNode& node = d.nodes[n];
        const bool branch = node.text.empty() && !node.branches.empty();
        auto& section = content.emplace<ui::Section>(node.id + (branch ? "  ·  branch" : "") + (node.id == d.start ? "  ·  start" : ""));
        section.setMoreAction([this, edit, n, &section, nodeId = node.id] {
            ui::MenuItem remove;
            remove.label = "Delete " + nodeId;
            remove.action = [edit, n] { edit("Delete Line", [n](Dialogue& x) { x.nodes.erase(x.nodes.begin() + static_cast<std::ptrdiff_t>(n)); }); };
            ui::MenuItem up;
            up.label = "Move Up";
            up.isEnabled = [n] { return n > 0; };
            up.action = [edit, n] { edit("Move Line", [n](Dialogue& x) { std::swap(x.nodes[n], x.nodes[n - 1]); }); };
            openMenuAt(m_editor.ui(), section.moreButton(), {std::move(up), ui::MenuItem::divider(), std::move(remove)});
        });
        const std::string key = "n/" + node.id;
        if (!branch) {
            auto& speakerRow = section.body().emplace<ui::PropertyField>("Speaker");
            auto& speaker = speakerRow.addValue(std::make_unique<ui::ChoiceField>(speakerOptions));
            speaker.setStretch(1.0f);
            int selected = 0;
            for (std::size_t s = 0; s < d.speakers.size(); ++s) {
                if (d.speakers[s].id == node.speaker) {
                    selected = static_cast<int>(s) + 1;
                }
            }
            speaker.setSelected(selected);
            auto speakerIdsList = std::make_shared<std::vector<std::string>>();
            for (const Speaker& s : d.speakers) {
                speakerIdsList->push_back(s.id);
            }
            speaker.setOnChoose([edit, n, speakerIdsList](int index) {
                const std::string chosen = index <= 0 ? std::string() : (*speakerIdsList)[static_cast<std::size_t>(index - 1)];
                edit("Change Speaker", [&](Dialogue& x) { x.nodes[n].speaker = chosen; });
            });
            auto& textRow = section.body().emplace<ui::PropertyField>("Says");
            auto& text = textRow.addValue(std::make_unique<ui::TextField>("What is said; {gems} shows a variable"));
            text.setText(node.text);
            text.setStretch(1.0f);
            keyed(key + "/text", text);
            text.setOnCommit([edit, n](const std::string& t) { edit("Change Line", [&](Dialogue& x) { x.nodes[n].text = t; }); });
            if (node.choices.empty()) {
                auto& nextRow = section.body().emplace<ui::PropertyField>("Then");
                nodeChoice(nextRow, node.next, "(end of dialogue)", [edit, n](std::string next) {
                    edit("Change Next Line", [&](Dialogue& x) { x.nodes[n].next = next; });
                });
            }
            // Choices.
            std::set<std::string> choiceIds;
            for (std::size_t c = 0; c < node.choices.size(); ++c) {
                const DialogueChoice& choice = node.choices[c];
                choiceIds.insert(choice.id);
                const std::string ckey = key + "/c/" + choice.id;
                auto& head = section.body().emplace<ui::Row>(4.0f, ui::Insets{0.0f, 6.0f, 0.0f, 0.0f});
                auto& label = head.emplace<ui::Label>("Choice " + std::to_string(c + 1), ui::Label::Style::Strong);
                label.setStretch(1.0f);
                removeButton(head, [edit, n, c] {
                    edit("Remove Choice", [&](Dialogue& x) { x.nodes[n].choices.erase(x.nodes[n].choices.begin() + static_cast<std::ptrdiff_t>(c)); });
                }, "Remove this choice");
                auto& ctext = section.body().emplace<ui::PropertyField>("Player Says");
                auto& field = ctext.addValue(std::make_unique<ui::TextField>("The answer the player picks"));
                field.setText(choice.text);
                field.setStretch(1.0f);
                keyed(ckey + "/text", field);
                field.setOnCommit([edit, n, c](const std::string& t) {
                    edit("Change Choice", [&](Dialogue& x) { x.nodes[n].choices[c].text = t; });
                });
                auto& shown = section.body().emplace<ui::PropertyField>("Shown When");
                conditionField(shown, ckey + "/when", choice.condition.isAlways() ? std::nullopt : std::optional<Condition>(choice.condition),
                               "always", [edit, n, c](std::optional<Condition> cond) {
                                   edit("Change Choice Condition", [&](Dialogue& x) { x.nodes[n].choices[c].condition = cond.value_or(Condition{}); });
                               });
                auto& next = section.body().emplace<ui::PropertyField>("Then");
                nodeChoice(next, choice.next, "(end of dialogue)", [edit, n, c](std::string to) {
                    edit("Change Choice", [&](Dialogue& x) { x.nodes[n].choices[c].next = to; });
                });
                auto& actionsLabel = section.body().emplace<ui::Label>("When chosen:", ui::Label::Style::Dim);
                static_cast<void>(actionsLabel);
                actionList(section.body(), ckey + "/actions", choice.actions, [edit, n, c](std::vector<Action> a) {
                    edit("Change Choice Actions", [&](Dialogue& x) { x.nodes[n].choices[c].actions = std::move(a); });
                });
            }
            auto& buttons = section.body().emplace<ui::Flow>(6.0f, 6.0f, ui::Insets{0.0f, 6.0f, 0.0f, 0.0f});
            buttons.emplace<ui::Button>("Add Choice", [edit, n, choiceIds] {
                const std::string newId = freeId("choice", choiceIds);
                edit("Add Choice", [&](Dialogue& x) {
                    DialogueChoice choice;
                    choice.id = newId;
                    x.nodes[n].choices.push_back(choice);
                    x.nodes[n].next.clear();
                });
            });
        } else {
            for (std::size_t b = 0; b < node.branches.size(); ++b) {
                const DialogueBranch& br = node.branches[b];
                auto& when = section.body().emplace<ui::PropertyField>("If");
                conditionField(when, key + "/b" + std::to_string(b), br.condition, "always",
                               [edit, n, b](std::optional<Condition> cond) {
                                   edit("Change Branch", [&](Dialogue& x) { x.nodes[n].branches[b].condition = cond.value_or(Condition{}); });
                               });
                removeButton(when, [edit, n, b] {
                    edit("Remove Branch", [&](Dialogue& x) { x.nodes[n].branches.erase(x.nodes[n].branches.begin() + static_cast<std::ptrdiff_t>(b)); });
                }, "Remove this branch");
                auto& go = section.body().emplace<ui::PropertyField>("Go To");
                nodeChoice(go, br.next, "(choose a line)", [edit, n, b](std::string to) {
                    edit("Change Branch", [&](Dialogue& x) { x.nodes[n].branches[b].next = to; });
                });
            }
            auto& otherwise = section.body().emplace<ui::PropertyField>("Otherwise");
            nodeChoice(otherwise, node.next, "(end of dialogue)", [edit, n](std::string to) {
                edit("Change Branch", [&](Dialogue& x) { x.nodes[n].next = to; });
            });
            auto& buttons = section.body().emplace<ui::Flow>(6.0f, 6.0f, ui::Insets{0.0f, 6.0f, 0.0f, 0.0f});
            buttons.emplace<ui::Button>("Add Branch", [edit, n] {
                edit("Add Branch", [&](Dialogue& x) { x.nodes[n].branches.push_back({Condition{}, ""}); });
            });
        }
        auto& actionsLabel = section.body().emplace<ui::Label>(branch ? "When reached:" : "When said:", ui::Label::Style::Dim);
        static_cast<void>(actionsLabel);
        actionList(section.body(), key + "/actions", node.actions, [edit, n](std::vector<Action> a) {
            edit("Change Line Actions", [&](Dialogue& x) { x.nodes[n].actions = std::move(a); });
        });
    }

    auto& add = content.emplace<ui::Flow>(6.0f, 6.0f, ui::Insets{0.0f, 10.0f, 0.0f, 4.0f});
    add.emplace<ui::Button>("Add Line", [edit, nodeIds] {
        const std::string newId = freeId("line", nodeIds);
        edit("Add Line", [newId](Dialogue& x) {
            DialogueNode node;
            node.id = newId;
            node.speaker = x.speakers.empty() ? std::string() : x.speakers.front().id;
            node.text = "...";
            if (x.start.empty()) {
                x.start = newId;
            }
            x.nodes.push_back(node);
        });
    }).setStyle(ui::Button::Style::Primary);
    add.emplace<ui::Button>("Add Branch", [edit, nodeIds] {
        const std::string newId = freeId("branch", nodeIds);
        edit("Add Branch", [newId](Dialogue& x) {
            DialogueNode node;
            node.id = newId;
            node.branches.push_back({Condition{}, x.nodes.empty() ? std::string() : x.nodes.front().id});
            x.nodes.push_back(node);
        });
    });
}

// --- Triggers in scenes ------------------------------------------------------------------------------

void InspectorPanel::buildTriggerEditor(ui::Section& section, EntityId id)
{
    const Entity* entity = m_editor.document().scene().find(id);
    const Trigger* trigger = entity != nullptr ? entity->get<Trigger>() : nullptr;
    if (trigger == nullptr) {
        return;
    }
    const auto change = [this, id](const std::string& label, std::function<void(Trigger&)> edit) {
        const Entity* e = m_editor.document().scene().find(id);
        const Trigger* t = e != nullptr ? e->get<Trigger>() : nullptr;
        if (t == nullptr) {
            return;
        }
        auto copy = std::make_unique<Trigger>(*t);
        edit(*copy);
        m_editor.endEdit();
        m_editor.execute(std::make_unique<ReplaceComponent>(id, std::move(copy), label));
        m_editor.endEdit();
    };
    auto& when = section.body().emplace<ui::PropertyField>("Only If");
    conditionField(when, "t/" + id.toString() + "/if", trigger->reaction.condition,
                   trigger->on == TriggerOn::ConditionMet ? "never (Condition Met needs a condition)" : "always",
                   [change](std::optional<Condition> c) { change("Change Trigger Condition", [&](Trigger& t) { t.reaction.condition = c; }); });
    auto& label = section.body().emplace<ui::Label>("Actions:", ui::Label::Style::Dim);
    static_cast<void>(label);
    actionList(section.body(), "t/" + id.toString() + "/actions", trigger->reaction.actions,
               [change](std::vector<Action> a) { change("Change Trigger Actions", [&](Trigger& t) { t.reaction.actions = std::move(a); }); });
}

} // namespace lantern::editor
