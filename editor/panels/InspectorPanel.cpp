#include "Panels.hpp"

#include "lantern/ui/Context.hpp"
#include "lantern/ui/widgets/Menu.hpp"

#include <cctype>
#include <string>

namespace lantern::editor {

namespace {

ui::Color toUi(Color c)
{
    return ui::Color{c.r, c.g, c.b, c.a};
}

Color fromUi(ui::Color c)
{
    return Color{c.r, c.g, c.b, c.a};
}

std::string capitalized(std::string text)
{
    if (!text.empty()) {
        text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
    }
    return text;
}

std::string componentTypeId(const Component& c)
{
    if (const auto* unknown = dynamic_cast<const UnknownComponent*>(&c)) {
        return unknown->originalTypeId();
    }
    return c.type().id;
}

// The current value of a property in the open scene, if it still exists.
std::optional<PropertyValue> currentValue(const Scene& scene, EntityId id, const std::string& typeId,
                                          const std::string& key)
{
    const Entity* e = scene.find(id);
    const Component* c = e != nullptr ? e->component(typeId) : nullptr;
    return c != nullptr ? c->get(key) : std::nullopt;
}

ui::ValueBox& readOnlyRow(ui::Section& section, const char* label, const std::string& tooltip)
{
    auto& row = section.body().emplace<ui::PropertyField>(label);
    auto& box = row.addValue(std::make_unique<ui::ValueBox>());
    box.setTooltip(tooltip);
    return box;
}

} // namespace

InspectorPanel::InspectorPanel(EditorServices& editor)
    : ui::Column(0.0f)
    , m_editor(editor)
{
    m_empty = &emplace<ui::EmptyState>(ui::Icon::Sliders, "Nothing selected",
                                       "Select an object in the Hierarchy or the Scene to see and edit its "
                                       "properties.");
    auto scroll = std::make_unique<ui::ScrollView>();
    scroll->setStretch(1.0f);
    m_details = &addChild(std::move(scroll));
}

std::string InspectorPanel::signature() const
{
    // The panel is rebuilt when this changes: another object, another scene,
    // or components added or removed.
    const Scene& scene = m_editor.document().scene();
    std::string sig = std::to_string(m_editor.documentGeneration()) + "|";
    if (m_editor.isSceneSelected()) {
        return sig + "scene";
    }
    const Entity* e = scene.find(m_editor.selection());
    if (e == nullptr) {
        return sig + "none";
    }
    sig += e->id().toString();
    for (const auto& c : e->components()) {
        sig += "|" + componentTypeId(*c);
    }
    return sig;
}

void InspectorPanel::update(UiFrame&)
{
    if (const std::string sig = signature(); sig != m_builtFor) {
        m_builtFor = sig;
        rebuild();
    }
    for (const auto& refresh : m_refresh) {
        refresh();
    }
}

void InspectorPanel::focusName()
{
    if (m_name != nullptr && context() != nullptr) {
        context()->setFocus(m_name);
        m_name->state().selectAll();
    }
}

void InspectorPanel::rebuild()
{
    m_refresh.clear();
    m_name = nullptr;
    const Scene& scene = m_editor.document().scene();
    const Entity* entity = scene.find(m_editor.selection());
    const bool showScene = m_editor.isSceneSelected();
    m_empty->setVisible(entity == nullptr && !showScene);
    m_details->setVisible(entity != nullptr || showScene);

    auto content = std::make_unique<ui::Column>(2.0f, ui::Insets{10.0f, 8.0f, 10.0f, 12.0f});
    if (showScene) {
        buildScene(*content);
    } else if (entity != nullptr) {
        buildEntity(*content, *entity);
    }
    m_details->setContent(std::move(content));
}

// --- The scene itself --------------------------------------------------------------------------

void InspectorPanel::buildScene(ui::Column& content)
{
    auto& section = content.emplace<ui::Section>("Scene");
    auto& nameRow = section.body().emplace<ui::PropertyField>("Name");
    auto& name = nameRow.addValue(std::make_unique<ui::TextField>(""));
    name.setStretch(1.0f);
    name.setFixedSize({0.0f, 22.0f});
    name.setSelectAllOnFocus(true);
    name.setOnCommit([this](const std::string& text) { m_editor.execute(std::make_unique<RenameScene>(text)); });
    m_name = &name;

    auto& objects = readOnlyRow(section, "Objects", "How many objects the scene contains");
    auto& file = readOnlyRow(section, "File", "Where the scene is saved, inside the project");
    m_refresh.push_back([this, &name, &objects, &file] {
        const SceneDocument& doc = m_editor.document();
        if (!name.isEditing() && name.text() != doc.scene().name()) {
            name.setText(doc.scene().name());
        }
        objects.setText(std::to_string(doc.scene().size()));
        std::string where = "Not saved yet";
        if (!doc.isUntitled()) {
            const Project* project = m_editor.project();
            const auto rel = project != nullptr ? project->relative(doc.path()) : std::nullopt;
            where = rel ? *rel : doc.path().string();
        }
        file.setText(where + (doc.isDirty() ? "  (unsaved changes)" : ""));
    });
}

// --- An entity ---------------------------------------------------------------------------------

void InspectorPanel::buildEntity(ui::Column& content, const Entity& entity)
{
    const EntityId id = entity.id();

    // Header: on/off, and the name.
    auto& header = content.emplace<ui::Row>(6.0f, ui::Insets{0.0f, 0.0f, 0.0f, 6.0f});
    header.setCrossAlign(ui::CrossAlign::Center);
    auto& active = header.emplace<ui::Checkbox>("", entity.isActive(), [this, id](bool on) {
        m_editor.execute(std::make_unique<SetEntityActive>(id, on));
        m_editor.endEdit();
    });
    active.setTooltip("Shown in the game. Hidden objects hide everything inside them too.");
    auto& name = header.emplace<ui::TextField>("Name");
    name.setStretch(1.0f);
    name.setFixedSize({0.0f, 24.0f});
    name.setSelectAllOnFocus(true);
    name.setOnCommit([this, id](const std::string& text) {
        m_editor.execute(std::make_unique<RenameEntity>(id, text));
        m_editor.endEdit();
    });
    m_name = &name;
    m_refresh.push_back([this, id, &active, &name] {
        const Entity* e = m_editor.document().scene().find(id);
        if (e == nullptr) {
            return;
        }
        active.setChecked(e->isActive());
        if (!name.isEditing() && name.text() != e->name()) {
            name.setText(e->name());
        }
    });

    for (const auto& component : entity.components()) {
        buildComponent(content, entity, *component);
    }

    // Add Component.
    auto& addRow = content.emplace<ui::Row>(0.0f, ui::Insets{0.0f, 14.0f, 0.0f, 8.0f});
    addRow.emplace<ui::Spacer>();
    auto& add = addRow.emplace<ui::Button>("Add Component");
    add.setFixedSize({180.0f, 0.0f});
    add.setOnClick([this, &add, id] { openAddComponentMenu(add, id); });
    addRow.emplace<ui::Spacer>();

    // The permanent ID, for reference.
    auto& idRow = content.emplace<ui::Row>(6.0f, ui::Insets{0.0f, 4.0f, 0.0f, 0.0f});
    idRow.emplace<ui::Spacer>();
    auto& idLabel = idRow.emplace<ui::Label>("ID " + id.toString(), ui::Label::Style::Small);
    idLabel.setColor(ui::Color::hex(0x6E6E6E));
    idLabel.setTooltip("This object's permanent ID. Scenes, and later saved games and quests, refer to objects "
                       "by ID, so renaming never breaks anything.");
    idRow.emplace<ui::Spacer>();
}

void InspectorPanel::buildComponent(ui::Column& content, const Entity& entity, const Component& component)
{
    const EntityId id = entity.id();
    const ComponentType& type = component.type();
    const std::string typeId = componentTypeId(component);
    const auto* unknown = dynamic_cast<const UnknownComponent*>(&component);

    auto& section = content.emplace<ui::Section>(unknown != nullptr ? typeId : type.displayName);
    std::string help = type.description;
    for (const PropertyInfo& p : type.properties) {
        if (!p.options.help.empty()) {
            help += "\n\n" + p.label + ": " + p.options.help;
        }
    }
    if (!help.empty()) {
        const std::string title = type.displayName;
        section.setHelpAction([this, title, help] { m_editor.showMessage(title, help); });
    }
    section.setMoreAction([this, &section, id, typeId] { openComponentMenu(section, id, typeId); });

    if (unknown != nullptr) {
        auto& note = section.body().emplace<ui::Label>(
            "This component type is not known to this version of Lantern. Its data is kept unchanged and saved "
            "with the scene.",
            ui::Label::Style::Faint);
        note.setWrap(true);
        return;
    }
    for (const PropertyInfo& info : type.properties) {
        addPropertyEditor(section, id, typeId, info);
    }
}

void InspectorPanel::addPropertyEditor(ui::Section& section, EntityId id, const std::string& typeId,
                                       const PropertyInfo& info)
{
    auto& row = section.body().emplace<ui::PropertyField>(info.label);
    if (!info.options.help.empty()) {
        row.setTooltip(info.options.help);
    }
    const std::string key = info.key;
    const std::string label = info.label;
    auto set = [this, id, typeId, key, label](PropertyValue value) {
        m_editor.execute(std::make_unique<SetProperty>(id, typeId, key, std::move(value), label));
    };
    auto get = [this, id, typeId, key] { return currentValue(m_editor.document().scene(), id, typeId, key); };
    auto end = [this] { m_editor.endEdit(); };

    const auto numberField = [&](const std::string& suffix) -> ui::NumberField& {
        auto& field = row.addValue(std::make_unique<ui::NumberField>());
        field.setStep(info.options.step);
        field.setRange(info.options.min, info.options.max);
        field.setSuffix(suffix);
        field.setOnEditEnd(end);
        return field;
    };

    switch (info.kind) {
    case PropertyKind::Float:
    case PropertyKind::Int: {
        auto& field = numberField(info.options.unit);
        const bool integer = info.kind == PropertyKind::Int;
        field.setInteger(integer);
        field.setOnValue([set, integer](float v) {
            set(integer ? PropertyValue(static_cast<int>(std::lround(v))) : PropertyValue(v));
        });
        m_refresh.push_back([get, &field] {
            if (const auto v = get()) {
                if (const float* f = std::get_if<float>(&*v)) {
                    field.setValue(*f);
                } else if (const int* i = std::get_if<int>(&*v)) {
                    field.setValue(static_cast<float>(*i));
                }
            }
        });
        break;
    }
    case PropertyKind::Vec2: {
        const bool isSize = key == "size";
        auto& x = numberField(isSize ? "W" : "X");
        auto& y = numberField(isSize ? "H" : "Y");
        x.setOnValue([set, get](float v) {
            if (const auto cur = get(); cur && std::holds_alternative<Vec2>(*cur)) {
                set(Vec2{v, std::get<Vec2>(*cur).y});
            }
        });
        y.setOnValue([set, get](float v) {
            if (const auto cur = get(); cur && std::holds_alternative<Vec2>(*cur)) {
                set(Vec2{std::get<Vec2>(*cur).x, v});
            }
        });
        m_refresh.push_back([get, &x, &y] {
            if (const auto v = get(); v && std::holds_alternative<Vec2>(*v)) {
                x.setValue(std::get<Vec2>(*v).x);
                y.setValue(std::get<Vec2>(*v).y);
            }
        });
        break;
    }
    case PropertyKind::Bool: {
        auto& box = row.addValue(std::make_unique<ui::Checkbox>(""));
        box.setOnChange([set, end](bool on) {
            set(on);
            end();
        });
        m_refresh.push_back([get, &box] {
            if (const auto v = get(); v && std::holds_alternative<bool>(*v)) {
                box.setChecked(std::get<bool>(*v));
            }
        });
        break;
    }
    case PropertyKind::Color: {
        auto& field = row.addValue(std::make_unique<ui::ColorField>());
        field.setOnValue([set](ui::Color c) { set(fromUi(c)); });
        field.setOnEditEnd(end);
        m_refresh.push_back([get, &field] {
            if (const auto v = get(); v && std::holds_alternative<Color>(*v)) {
                field.setValue(toUi(std::get<Color>(*v)));
            }
        });
        break;
    }
    case PropertyKind::Choice: {
        std::vector<std::string> options;
        for (const std::string& choice : info.options.choices) {
            options.push_back(capitalized(choice));
        }
        auto& field = row.addValue(std::make_unique<ui::ChoiceField>(std::move(options)));
        field.setOnChoose([set, end](int index) {
            set(index);
            end();
        });
        m_refresh.push_back([get, &field] {
            if (const auto v = get(); v && std::holds_alternative<int>(*v)) {
                field.setSelected(std::get<int>(*v));
            }
        });
        break;
    }
    case PropertyKind::String: {
        auto& field = row.addValue(std::make_unique<ui::TextField>(""));
        field.setStretch(1.0f);
        field.setFixedSize({0.0f, 22.0f});
        field.setOnCommit([set, end](const std::string& text) {
            set(text);
            end();
        });
        m_refresh.push_back([get, &field] {
            if (const auto v = get(); v && std::holds_alternative<std::string>(*v) && !field.isEditing()) {
                if (field.text() != std::get<std::string>(*v)) {
                    field.setText(std::get<std::string>(*v));
                }
            }
        });
        break;
    }
    }
}

// --- Menus -------------------------------------------------------------------------------------

void InspectorPanel::openComponentMenu(ui::Section& section, EntityId id, const std::string& typeId)
{
    // Look the component up now: it may have changed since the panel was built.
    const Entity* entity = m_editor.document().scene().find(id);
    const Component* found = entity != nullptr ? entity->component(typeId) : nullptr;
    if (found == nullptr) {
        return;
    }
    const Component& component = *found;
    const ComponentType& type = component.type();
    const bool required = type.required;
    const bool unknown = dynamic_cast<const UnknownComponent*>(&component) != nullptr;

    ui::MenuItem reset;
    reset.label = "Reset to Defaults";
    reset.isEnabled = [unknown] { return !unknown; };
    reset.action = [this, id, typeId, &type] {
        if (!type.create) {
            return;
        }
        const std::unique_ptr<Component> defaults = type.create();
        auto group = std::make_unique<CommandGroup>("Reset " + type.displayName);
        for (const PropertyInfo& p : type.properties) {
            group->add(std::make_unique<SetProperty>(id, typeId, p.key, p.get(*defaults), p.label));
        }
        m_editor.execute(std::move(group));
        m_editor.endEdit();
    };

    ui::MenuItem remove;
    remove.label = "Remove Component";
    remove.isEnabled = [required] { return !required; };
    remove.action = [this, id, typeId] {
        m_editor.execute(std::make_unique<RemoveComponent>(id, typeId));
        m_editor.endEdit();
    };

    std::vector<ui::MenuItem> items{std::move(reset), ui::MenuItem::divider(), std::move(remove)};
    const ui::Rect anchor = section.moreButton();
    openMenuAt(m_editor.ui(), ui::Rect{anchor.right() - 180.0f, anchor.y, 180.0f, anchor.height}, std::move(items));
}

void InspectorPanel::openAddComponentMenu(ui::Button& button, EntityId id)
{
    const Entity* entity = m_editor.document().scene().find(id);
    if (entity == nullptr) {
        return;
    }
    std::vector<ui::MenuItem> items;
    for (const ComponentType* type : m_editor.registry().types()) {
        if (type->required || entity->component(type->id) != nullptr || !type->create) {
            continue;
        }
        ui::MenuItem item;
        item.label = type->displayName;
        item.action = [this, id, type] {
            m_editor.execute(std::make_unique<AddComponent>(id, type->create()));
            m_editor.endEdit();
        };
        items.push_back(std::move(item));
    }
    if (items.empty()) {
        ui::MenuItem none;
        none.label = "Every component is already added";
        none.isEnabled = [] { return false; };
        items.push_back(std::move(none));
    }
    items.push_back(ui::MenuItem::divider());
    ui::MenuItem more;
    more.label = "Sprite, Camera, Tilemap: coming in Milestone 4";
    more.isEnabled = [] { return false; };
    items.push_back(std::move(more));
    openMenuAt(m_editor.ui(), button.bounds(), std::move(items));
}

} // namespace lantern::editor
