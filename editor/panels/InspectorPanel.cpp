#include "Panels.hpp"

#include "lantern/ui/Context.hpp"
#include "lantern/ui/widgets/Dialog.hpp"

#include <cstdio>
#include <string>

namespace lantern::editor {

namespace {

template <class... Args>
std::string format(const char* pattern, Args... args)
{
    char buffer[96];
    std::snprintf(buffer, sizeof buffer, pattern, args...);
    return buffer;
}

std::string displayName(const SceneNodeInfo& node)
{
    return node.name.empty() ? "(unnamed)" : node.name;
}

ui::ValueBox& addBox(ui::PropertyField& field, const char* suffix = "")
{
    auto& box = field.addValue(std::make_unique<ui::ValueBox>());
    box.setSuffix(suffix);
    return box;
}

ui::PropertyField& addField(ui::Section& section, const char* label)
{
    return section.body().emplace<ui::PropertyField>(label);
}

constexpr const char* kReadOnly = "Live value from the running scene. Editing arrives in Milestone 3.";

} // namespace

InspectorPanel::InspectorPanel(EditorState& state)
    : ui::Column(0.0f)
    , m_state(state)
{
    auto empty = std::make_unique<ui::EmptyState>(ui::Icon::Sliders, "Nothing selected",
                                                  "Select an object in the Hierarchy to see its properties.");
    m_empty = &addChild(std::move(empty));

    auto scroll = std::make_unique<ui::ScrollView>();
    scroll->setStretch(1.0f);
    auto& content = scroll->setContent(std::make_unique<ui::Column>(2.0f, ui::Insets{10.0f, 8.0f, 10.0f, 12.0f}));
    m_details = &addChild(std::move(scroll));
    Fields& f = m_fields;

    // Header: is the object on, and its name.
    auto& header = content.emplace<ui::Row>(6.0f, ui::Insets{0.0f, 0.0f, 0.0f, 6.0f});
    header.setCrossAlign(ui::CrossAlign::Center);
    f.active = &header.emplace<ui::ValueBox>("", ui::ValueBox::Kind::Check);
    f.active->setTooltip("Whether the object is shown. " + std::string(kReadOnly));
    f.name = &header.emplace<ui::ValueBox>();
    f.name->setTooltip(kReadOnly);

    // Node: where the object is and how it is transformed.
    f.node = &content.emplace<ui::Section>("Node");
    f.node->setHelpAction([this] {
        showHelp("Node",
                 "Every object in a scene is a node. Position is measured in points from its parent's "
                 "origin. Rotation is in degrees, clockwise. Scale multiplies its size: 1 is normal "
                 "size. Size is the object's own width and height, before scaling.");
    });
    {
        auto& position = addField(*f.node, "Position");
        f.position = {&addBox(position, "X"), &addBox(position, "Y")};
        auto& rotation = addField(*f.node, "Rotation");
        f.rotation = &addBox(rotation, "°");
        auto& scale = addField(*f.node, "Scale");
        f.scale = {&addBox(scale, "X"), &addBox(scale, "Y")};
        auto& size = addField(*f.node, "Size");
        f.size = {&addBox(size, "W"), &addBox(size, "H")};
    }

    // Rendering: what kind of object it is and how it is drawn.
    f.rendering = &content.emplace<ui::Section>("Rendering");
    f.rendering->setHelpAction([this] {
        showHelp("Rendering",
                 "Type is the kind of object, such as a shape or an image. Objects with a higher draw "
                 "order are drawn on top of their siblings. A hidden object is not drawn, and neither "
                 "are its children.");
    });
    {
        f.kind = &addBox(addField(*f.rendering, "Type"));
        f.drawOrder = &addBox(addField(*f.rendering, "Draw Order"));
        auto& visible = addField(*f.rendering, "Visible");
        f.visible = &visible.addValue(std::make_unique<ui::ValueBox>("", ui::ValueBox::Kind::Check));
    }

    // Hierarchy: where the object sits in the scene tree.
    f.hierarchy = &content.emplace<ui::Section>("Hierarchy");
    {
        f.parent = &addBox(addField(*f.hierarchy, "Parent"));
        f.children = &addBox(addField(*f.hierarchy, "Children"));
        f.sessionId = &addBox(addField(*f.hierarchy, "Session ID"));
        f.sessionId->setTooltip("A number that identifies the object while the editor runs. Saved scenes "
                                "use permanent IDs (Milestone 3).");
    }
    for (ui::Section* section : {f.node, f.rendering, f.hierarchy}) {
        for (const auto& row : section->body().children()) {
            if (row->tooltip().empty()) {
                row->setTooltip(kReadOnly);
            }
        }
    }

    // Add Component, like other editors; components arrive with scene editing.
    auto& addRow = content.emplace<ui::Row>(0.0f, ui::Insets{0.0f, 14.0f, 0.0f, 6.0f});
    addRow.emplace<ui::Spacer>();
    auto& add = addRow.emplace<ui::Button>("Add Component");
    add.setFixedSize({180.0f, 0.0f});
    add.setEnabled(false);
    add.setTooltip(kComingWithEditing);
    addRow.emplace<ui::Spacer>();

    auto& note = content.emplace<ui::Label>("Values update live while the scene runs. Editing, with undo, arrives "
                                            "in Milestone 3.",
                                            ui::Label::Style::Faint);
    note.setWrap(true);
    note.setAlign(ui::TextAlign::Center);
}

void InspectorPanel::showHelp(const char* title, const char* text)
{
    ui::Context* ctx = context();
    if (ctx == nullptr) {
        return;
    }
    auto dialog = std::make_unique<ui::Dialog>(title, 380.0f);
    dialog->body().emplace<ui::Label>(text).setWrap(true);
    dialog->addButton("OK", ui::Button::Style::Primary);
    ui::showDialog(*ctx, std::move(dialog));
}

void InspectorPanel::update(UiFrame& frame)
{
    const std::vector<SceneNodeInfo>& nodes = frame.sceneOutline();
    const SceneNodeInfo* selected = nullptr;
    const SceneNodeInfo* parent = nullptr;
    for (const SceneNodeInfo& node : nodes) {
        if (node.id == m_state.selectedNodeId) {
            selected = &node;
        }
    }
    if (selected != nullptr && selected->parentId != 0) {
        for (const SceneNodeInfo& node : nodes) {
            if (node.id == selected->parentId) {
                parent = &node;
            }
        }
    }

    m_empty->setVisible(selected == nullptr);
    m_details->setVisible(selected != nullptr);
    if (selected == nullptr) {
        return;
    }

    const auto d = [](float v) { return static_cast<double>(v); };
    Fields& f = m_fields;
    f.active->setChecked(selected->visible);
    f.name->setText(displayName(*selected));
    f.position[0]->setText(format("%.1f", d(selected->x)));
    f.position[1]->setText(format("%.1f", d(selected->y)));
    f.rotation->setText(format("%.1f", d(selected->rotationDegrees)));
    f.scale[0]->setText(format("%.3f", d(selected->scaleX)));
    f.scale[1]->setText(format("%.3f", d(selected->scaleY)));
    f.size[0]->setText(format("%.0f", d(selected->width)));
    f.size[1]->setText(format("%.0f", d(selected->height)));
    f.rendering->setToggle(selected->visible);
    f.kind->setText(selected->kind);
    f.drawOrder->setText(std::to_string(selected->drawOrder));
    f.visible->setChecked(selected->visible);
    f.parent->setText(parent != nullptr ? displayName(*parent) : "—");
    f.children->setText(std::to_string(selected->childCount));
    f.sessionId->setText(std::to_string(selected->id));
}

} // namespace lantern::editor
