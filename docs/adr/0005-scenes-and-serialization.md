# ADR 0005: Scenes, components, files and undo (Milestone 3)

- **Status:** Accepted
- **Date:** 2026-10-29
- **Milestone:** 3

## Context

Until Milestone 2 the world was built directly from Axmol nodes in code, and
the editor could only look at it. Milestone 3 asks for entities through our
own engine layer, stable IDs, component editing, scenes that can be created,
loaded, saved and validated, serialization tests, and basic undo and redo.

## Decisions

### 1. Scenes are Rolo data; Axmol only draws them

A new library, `rolo_scene` (`engine/scene`), holds scenes as plain C++
data: a tree of entities, each with an ID, a name, an active switch and
components. It has no Axmol and no UI, so it is unit-tested in every preset.

The runtime draws a scene through `SceneRenderer` (`engine/runtime/src`),
which creates and updates Axmol nodes from the data whenever the scene's
`revision()` changes. Game code and the editor only ever see `rolo::Scene`.
The hard-coded start world and the Axmol-node outline used by the Milestone 2
editor are gone; the start scene is now `makeStartScene()`, built from
Shape components.

### 2. Simple components, described once

No ECS framework. An entity owns a short list of components, at most one of
each type, and always a `Transform`. Each component type describes its
properties once (key, label, kind, limits, help) with `ComponentTypeBuilder`.
That single description drives:

- the Inspector (any component gets fields without editor code for it),
- saving and loading (properties by key, choices by name),
- undo (`SetProperty` works on any property),
- validation (values are coerced and clamped the same way everywhere).

Version 1 has `rolo.Transform` and `rolo.Shape`. Sprites, cameras and
tilemaps follow in Milestone 4, gameplay components in Milestone 5, using the
same mechanism.

### 3. Stable 64-bit IDs

Every entity has a random 64-bit ID, written as 16 hex digits. IDs are never
reused or renumbered, survive renames, moves and save/load, and are what
undo, the editor selection and (later) other files and saved games refer to.
Duplicating an entity gives the copy fresh IDs. Random IDs, rather than a
counter, mean entities created on two machines (or merged in version control)
do not clash. The generator can be seeded for repeatable tests.

### 4. JSON files with versions, using nlohmann/json

Projects (`project.rolo`) and scenes (`.lscene`) are JSON: readable,
diff-friendly, editable by hand in an emergency. nlohmann/json 3.12.0 (MIT)
reads and writes them. It is linked privately into `rolo_scene` and never
appears in a public header, so it could be replaced.

Every file states `format` and `version`. Older versions are upgraded step
by step on load; newer ones are refused with a clear message. Loading is
forgiving where it is safe (unknown properties skipped, bad values replaced
by defaults, unknown component types **kept and written back**) and strict
where guessing would lose data (missing or duplicate IDs, broken parent
links). Every problem is reported with where it is and what to do.

Saving is deterministic (same scene, same bytes) and atomic (temporary file,
then rename). Paths inside a project are relative and use forward slashes.
Editor state (selection, history, layout) is never written into scenes.

The full format: [docs/formats/scene.md](../formats/scene.md).

### 5. Undo and redo: commands

Every change the editor makes is a `Command` with `apply` and `revert`
(`editor/model/Commands.hpp`): create, delete, duplicate, rename, set
active, move, set property, add and remove component, rename scene, and
groups of these. Commands store IDs and copied values, never pointers, so
they stay valid across any sequence of undo and redo. A delete keeps a
snapshot of the whole subtree and restores it with the same IDs.

Rapid edits of one property (dragging a number, typing) merge into one undo
step until the edit ends. The `CommandStack` remembers which step was saved,
which gives the unsaved-changes marker and the "Save Changes?" prompt on
quit, close, or switching scenes.

Commands live in `rolo_editor_core` (no UI), so undo is unit-tested
without a window, and the brief's rule that editor operations go through
undo is now enforced by structure: panels call the editor's `execute()`,
never `Scene` setters directly.

### 6. Projects without native file dialogs

New Project and Open Project use Rolo's own dialogs: a name plus a
location (default `Documents/Rolo Projects`), and a list of recently
opened projects plus a field for a project folder's path. This avoids per-OS file-dialog code for
now; native dialogs can be added behind the same commands later.

## Consequences

- The player can play a project (`--project`) or a scene file (`--scene`).
- The Inspector, Hierarchy and Scene view work on scene data, with stable
  selection by ID.
- Viewport manipulation (move, rotate, scale gizmos) and asset import remain
  Milestone 4, as the brief orders them; their buttons stay disabled with
  tooltips.
- A new dependency, nlohmann/json, is listed in `THIRD_PARTY_NOTICES.md`.
