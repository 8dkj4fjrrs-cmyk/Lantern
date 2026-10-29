#pragma once

// Undo and redo.
//
// Every change the editor makes to a scene is a Command: an object that knows
// how to do the change (apply) and how to take it back (revert). The
// CommandStack runs commands and remembers them:
//
//     stack.execute(scene, std::make_unique<RenameEntity>(id, "Hero"));
//     stack.undo(scene);   // the old name is back
//     stack.redo(scene);   // "Hero" again
//
// Commands store IDs and copies of values, never pointers into the scene, so
// they stay valid however the scene changes between undo and redo.

#include "lantern/scene/Scene.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lantern::editor {

class Command {
public:
    virtual ~Command() = default;

    /// Shown in the Edit menu: "Undo <label>".
    virtual std::string label() const = 0;
    /// Makes the change. Returns false if it cannot be made (nothing is changed then).
    virtual bool apply(Scene& scene) = 0;
    /// Takes the change back. Only called after a successful apply().
    virtual void revert(Scene& scene) = 0;

    /// Lets a quick series of small edits become one undo step (e.g. dragging
    /// a number). Called on the newest command with the next one; return true
    /// after taking over `next`'s result.
    virtual bool mergeWith(const Command& /*next*/) { return false; }
};

class CommandStack {
public:
    explicit CommandStack(std::size_t limit = 500)
        : m_limit(limit)
    {
    }

    /// Applies a command and records it (dropping anything that could be redone).
    bool execute(Scene& scene, std::unique_ptr<Command> command);

    bool canUndo() const noexcept { return m_position > 0; }
    bool canRedo() const noexcept { return m_position < m_commands.size(); }
    std::string undoLabel() const;
    std::string redoLabel() const;
    bool undo(Scene& scene);
    bool redo(Scene& scene);

    /// Remembers the current point as "saved".
    void markClean() noexcept;
    /// True if the scene is as it was when last saved (or opened).
    bool isClean() const noexcept { return m_cleanPosition && *m_cleanPosition == m_position; }

    /// Stops the next execute() from merging into the newest command (e.g.
    /// when a drag ends, or the user moves to another field).
    void breakMerge() noexcept { m_mergeAllowed = false; }

    void clear() noexcept;
    std::size_t size() const noexcept { return m_commands.size(); }

private:
    std::vector<std::unique_ptr<Command>> m_commands;
    std::size_t m_position = 0;                  // commands before this are applied
    std::optional<std::size_t> m_cleanPosition = 0;
    std::size_t m_limit;
    bool m_mergeAllowed = false;
};

// --- The editor's commands -------------------------------------------------------------------

/// Creates a new entity (with the given components besides its Transform).
class CreateEntity final : public Command {
public:
    CreateEntity(std::string name, EntityId parent, std::optional<std::size_t> index,
                 std::vector<std::unique_ptr<Component>> components = {});
    std::string label() const override { return "Create " + m_name; }
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;
    /// The new entity's ID (valid after the first apply; kept for redo).
    EntityId id() const noexcept { return m_id; }

private:
    std::string m_name;
    EntityId m_parent;
    std::optional<std::size_t> m_index;
    std::vector<std::unique_ptr<Component>> m_components;
    EntityId m_id;
};

/// Deletes an entity and everything inside it; undo puts it all back with the same IDs.
class DeleteEntity final : public Command {
public:
    explicit DeleteEntity(EntityId id);
    std::string label() const override { return "Delete " + m_name; }
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;

private:
    EntityId m_id;
    std::string m_name;
    EntitySnapshot m_snapshot;
    EntityId m_parent;
};

/// Copies an entity (and its children) right after the original, with new IDs.
class DuplicateEntity final : public Command {
public:
    explicit DuplicateEntity(EntityId id);
    std::string label() const override { return "Duplicate " + m_name; }
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;
    EntityId copyId() const noexcept { return m_copy.rootId(); }

private:
    EntityId m_source;
    std::string m_name;
    EntitySnapshot m_copy;   // made on first apply, reused on redo (same IDs)
    EntityId m_parent;
};

class RenameEntity final : public Command {
public:
    RenameEntity(EntityId id, std::string name);
    std::string label() const override { return "Rename"; }
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;

private:
    EntityId m_id;
    std::string m_name;
    std::string m_previous;
};

class SetEntityActive final : public Command {
public:
    SetEntityActive(EntityId id, bool active);
    std::string label() const override { return m_active ? "Show" : "Hide"; }
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;

private:
    EntityId m_id;
    bool m_active;
    bool m_previous = true;
};

/// Changes one component property. Consecutive changes to the same property
/// merge into one undo step while merging is allowed (see CommandStack::breakMerge).
class SetProperty final : public Command {
public:
    SetProperty(EntityId id, std::string componentType, std::string key, PropertyValue value, std::string label);
    std::string label() const override { return "Change " + m_label; }
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;
    bool mergeWith(const Command& next) override;

private:
    EntityId m_id;
    std::string m_type;
    std::string m_key;
    PropertyValue m_value;
    std::optional<PropertyValue> m_previous;
    std::string m_label;
};

class AddComponent final : public Command {
public:
    AddComponent(EntityId id, std::unique_ptr<Component> component);
    std::string label() const override { return "Add " + m_name; }
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;

private:
    EntityId m_id;
    std::unique_ptr<Component> m_component;   // a copy is added on each apply
    std::string m_typeId;
    std::string m_name;
};

class RemoveComponent final : public Command {
public:
    RemoveComponent(EntityId id, std::string typeId);
    std::string label() const override { return "Remove " + m_name; }
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;

private:
    EntityId m_id;
    std::string m_typeId;
    std::string m_name;
    std::unique_ptr<Component> m_removed;
    std::size_t m_index = 0;
};

/// Several commands as one undo step (e.g. "Reset" sets every property).
class CommandGroup final : public Command {
public:
    explicit CommandGroup(std::string label)
        : m_label(std::move(label))
    {
    }
    void add(std::unique_ptr<Command> command) { m_commands.push_back(std::move(command)); }
    bool empty() const noexcept { return m_commands.empty(); }
    std::string label() const override { return m_label; }
    /// Applies the parts that make a change; succeeds if any did.
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;

private:
    std::string m_label;
    std::vector<std::unique_ptr<Command>> m_commands;
    std::vector<bool> m_applied;
};

class RenameScene final : public Command {
public:
    explicit RenameScene(std::string name)
        : m_name(std::move(name))
    {
    }
    std::string label() const override { return "Rename Scene"; }
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;

private:
    std::string m_name;
    std::string m_previous;
};

/// Moves an entity to another parent and/or position among its siblings.
class MoveEntity final : public Command {
public:
    MoveEntity(EntityId id, EntityId newParent, std::size_t index);
    std::string label() const override { return "Move " + m_name; }
    bool apply(Scene& scene) override;
    void revert(Scene& scene) override;

private:
    EntityId m_id;
    EntityId m_newParent;
    std::size_t m_index;
    EntityId m_oldParent;
    std::size_t m_oldIndex = 0;
    std::string m_name;
};

} // namespace lantern::editor
