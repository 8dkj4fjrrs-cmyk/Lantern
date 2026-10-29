#include "Commands.hpp"

#include <algorithm>

namespace lantern::editor {

// --- CommandStack ---------------------------------------------------------------------------

bool CommandStack::execute(Scene& scene, std::unique_ptr<Command> command)
{
    if (command == nullptr || !command->apply(scene)) {
        return false;
    }
    // Merge into the newest command if it is still the newest thing that
    // happened and the save point does not sit between them.
    const bool atTop = m_position == m_commands.size() && m_position > 0;
    const bool cleanHere = m_cleanPosition && *m_cleanPosition == m_position;
    if (m_mergeAllowed && atTop && !cleanHere && m_commands.back()->mergeWith(*command)) {
        return true;
    }

    // Anything that could have been redone is gone now.
    if (m_cleanPosition && *m_cleanPosition > m_position) {
        m_cleanPosition.reset();
    }
    m_commands.resize(m_position);
    m_commands.push_back(std::move(command));
    ++m_position;
    if (m_commands.size() > m_limit) {
        m_commands.erase(m_commands.begin());
        --m_position;
        if (m_cleanPosition) {
            if (*m_cleanPosition == 0) {
                m_cleanPosition.reset(); // the saved state can no longer be reached
            } else {
                --*m_cleanPosition;
            }
        }
    }
    m_mergeAllowed = true;
    return true;
}

std::string CommandStack::undoLabel() const
{
    return canUndo() ? m_commands[m_position - 1]->label() : std::string{};
}

std::string CommandStack::redoLabel() const
{
    return canRedo() ? m_commands[m_position]->label() : std::string{};
}

bool CommandStack::undo(Scene& scene)
{
    if (!canUndo()) {
        return false;
    }
    --m_position;
    m_commands[m_position]->revert(scene);
    m_mergeAllowed = false;
    return true;
}

bool CommandStack::redo(Scene& scene)
{
    if (!canRedo()) {
        return false;
    }
    if (!m_commands[m_position]->apply(scene)) {
        // The scene no longer allows it (should not happen): drop the redo history.
        m_commands.resize(m_position);
        return false;
    }
    ++m_position;
    m_mergeAllowed = false;
    return true;
}

void CommandStack::markClean() noexcept
{
    m_cleanPosition = m_position;
    m_mergeAllowed = false;
}

void CommandStack::clear() noexcept
{
    m_commands.clear();
    m_position = 0;
    m_cleanPosition = 0;
    m_mergeAllowed = false;
}

// --- CreateEntity ------------------------------------------------------------------------------

CreateEntity::CreateEntity(std::string name, EntityId parent, std::optional<std::size_t> index,
                           std::vector<std::unique_ptr<Component>> components)
    : m_name(std::move(name))
    , m_parent(parent)
    , m_index(index)
    , m_components(std::move(components))
{
}

bool CreateEntity::apply(Scene& scene)
{
    // Redo re-creates the entity with the same ID, so later commands that
    // refer to it still work.
    Entity* entity = scene.create(m_name, m_parent, m_index, m_id);
    if (entity == nullptr) {
        return false;
    }
    m_id = entity->id();
    for (const auto& c : m_components) {
        if (c->type().required) {
            // The entity already has one (its Transform): copy the given values in.
            for (const PropertyInfo& p : c->type().properties) {
                scene.setProperty(m_id, c->type().id, p.key, p.get(*c));
            }
        } else {
            scene.addComponent(m_id, c->clone());
        }
    }
    return true;
}

void CreateEntity::revert(Scene& scene)
{
    scene.remove(m_id);
}

// --- DeleteEntity -------------------------------------------------------------------------------

DeleteEntity::DeleteEntity(EntityId id)
    : m_id(id)
{
}

bool DeleteEntity::apply(Scene& scene)
{
    const Entity* entity = scene.find(m_id);
    if (entity == nullptr) {
        return false;
    }
    m_name = entity->name();
    m_parent = entity->parent();
    m_snapshot = scene.capture(m_id);
    return scene.remove(m_id);
}

void DeleteEntity::revert(Scene& scene)
{
    scene.restore(m_snapshot, m_parent, m_snapshot.index);
}

// --- DuplicateEntity -----------------------------------------------------------------------------

DuplicateEntity::DuplicateEntity(EntityId id)
    : m_source(id)
{
}

bool DuplicateEntity::apply(Scene& scene)
{
    if (m_copy.empty()) {
        const Entity* source = scene.find(m_source);
        if (source == nullptr) {
            return false;
        }
        m_name = source->name();
        m_parent = source->parent();
        m_copy = scene.withNewIds(scene.capture(m_source));
        m_copy.index += 1; // right after the original
    }
    return scene.restore(m_copy, m_parent, m_copy.index);
}

void DuplicateEntity::revert(Scene& scene)
{
    scene.remove(m_copy.rootId());
}

// --- RenameEntity / SetEntityActive ------------------------------------------------------------------

RenameEntity::RenameEntity(EntityId id, std::string name)
    : m_id(id)
    , m_name(std::move(name))
{
}

bool RenameEntity::apply(Scene& scene)
{
    const Entity* entity = scene.find(m_id);
    if (entity == nullptr || entity->name() == m_name) {
        return false;
    }
    m_previous = entity->name();
    return scene.rename(m_id, m_name);
}

void RenameEntity::revert(Scene& scene)
{
    scene.rename(m_id, m_previous);
}

SetEntityActive::SetEntityActive(EntityId id, bool active)
    : m_id(id)
    , m_active(active)
{
}

bool SetEntityActive::apply(Scene& scene)
{
    const Entity* entity = scene.find(m_id);
    if (entity == nullptr || entity->isActive() == m_active) {
        return false;
    }
    m_previous = entity->isActive();
    return scene.setActive(m_id, m_active);
}

void SetEntityActive::revert(Scene& scene)
{
    scene.setActive(m_id, m_previous);
}

// --- SetProperty -----------------------------------------------------------------------------------

SetProperty::SetProperty(EntityId id, std::string componentType, std::string key, PropertyValue value,
                         std::string label)
    : m_id(id)
    , m_type(std::move(componentType))
    , m_key(std::move(key))
    , m_value(std::move(value))
    , m_label(std::move(label))
{
}

bool SetProperty::apply(Scene& scene)
{
    const Entity* entity = scene.find(m_id);
    const Component* component = entity != nullptr ? entity->component(m_type) : nullptr;
    if (component == nullptr) {
        return false;
    }
    const std::optional<PropertyValue> current = component->get(m_key);
    const PropertyInfo* info = component->type().findProperty(m_key);
    if (!current || info == nullptr) {
        return false;
    }
    const std::optional<PropertyValue> wanted = info->coerce(m_value);
    if (!wanted || *wanted == *current) {
        return false; // nothing would change: no undo step
    }
    m_previous = current;
    m_value = *wanted;
    return scene.setProperty(m_id, m_type, m_key, m_value);
}

void SetProperty::revert(Scene& scene)
{
    if (m_previous) {
        scene.setProperty(m_id, m_type, m_key, *m_previous);
    }
}

bool SetProperty::mergeWith(const Command& next)
{
    const auto* other = dynamic_cast<const SetProperty*>(&next);
    if (other == nullptr || other->m_id != m_id || other->m_type != m_type || other->m_key != m_key) {
        return false;
    }
    m_value = other->m_value; // keep our m_previous: undo goes back to before the first edit
    return true;
}

// --- AddComponent / RemoveComponent ---------------------------------------------------------------------

AddComponent::AddComponent(EntityId id, std::unique_ptr<Component> component)
    : m_id(id)
    , m_component(std::move(component))
{
    if (m_component != nullptr) {
        m_typeId = m_component->type().id;
        m_name = m_component->type().displayName;
    }
}

bool AddComponent::apply(Scene& scene)
{
    return m_component != nullptr && scene.addComponent(m_id, m_component->clone()) != nullptr;
}

void AddComponent::revert(Scene& scene)
{
    scene.removeComponent(m_id, m_typeId);
}

RemoveComponent::RemoveComponent(EntityId id, std::string typeId)
    : m_id(id)
    , m_typeId(std::move(typeId))
{
}

bool RemoveComponent::apply(Scene& scene)
{
    const auto index = scene.componentIndex(m_id, m_typeId);
    if (!index) {
        return false;
    }
    m_removed = scene.removeComponent(m_id, m_typeId);
    if (m_removed == nullptr) {
        return false; // required components cannot be removed
    }
    m_index = *index;
    m_name = m_removed->type().displayName;
    return true;
}

void RemoveComponent::revert(Scene& scene)
{
    if (m_removed != nullptr) {
        scene.addComponent(m_id, std::move(m_removed), m_index);
    }
}

// --- CommandGroup / RenameScene ---------------------------------------------------------------------------

bool CommandGroup::apply(Scene& scene)
{
    m_applied.assign(m_commands.size(), false);
    bool any = false;
    for (std::size_t i = 0; i < m_commands.size(); ++i) {
        m_applied[i] = m_commands[i]->apply(scene);
        any = any || m_applied[i];
    }
    return any;
}

void CommandGroup::revert(Scene& scene)
{
    for (std::size_t i = m_commands.size(); i-- > 0;) {
        if (m_applied[i]) {
            m_commands[i]->revert(scene);
        }
    }
}

bool RenameScene::apply(Scene& scene)
{
    if (scene.name() == m_name) {
        return false;
    }
    m_previous = scene.name();
    scene.setName(m_name);
    return true;
}

void RenameScene::revert(Scene& scene)
{
    scene.setName(m_previous);
}

// --- MoveEntity -------------------------------------------------------------------------------------------

MoveEntity::MoveEntity(EntityId id, EntityId newParent, std::size_t index)
    : m_id(id)
    , m_newParent(newParent)
    , m_index(index)
{
}

bool MoveEntity::apply(Scene& scene)
{
    const Entity* entity = scene.find(m_id);
    if (entity == nullptr) {
        return false;
    }
    m_name = entity->name();
    m_oldParent = entity->parent();
    m_oldIndex = scene.indexOf(m_id).value_or(0);
    if (m_oldParent == m_newParent && m_oldIndex == m_index) {
        return false;
    }
    return scene.move(m_id, m_newParent, m_index);
}

void MoveEntity::revert(Scene& scene)
{
    scene.move(m_id, m_oldParent, m_oldIndex);
}

} // namespace lantern::editor
