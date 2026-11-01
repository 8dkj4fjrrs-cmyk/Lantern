# ADR 0007: C++ gameplay, game modules and play mode (Milestone 5)

- **Status:** Accepted
- **Date:** 2026-11-01
- **Milestone:** 5

## Context

Milestone 5 asks for a registration mechanism for native C++ gameplay
behaviours, a sample game module that is compiled and linked, play, pause
and stop in the editor with safe restoration of the edited scene, input,
collision queries and basic scene transitions. The brief says to start with
statically linked modules and to avoid hot reload, plugins and scripting
languages, and asks for a simple component model without an ECS framework.

## Decisions

### 1. Behaviours are components with code

`lantern::Behaviour` (new library `lantern_gameplay`, `engine/gameplay`)
derives from `Component` and adds `start(Game&)` and `update(Game&, dt)`.
Behaviours describe their properties with the existing
`ComponentTypeBuilder`, so the Inspector, scene files, undo and unknown-type
preservation work for them with no extra code. `ComponentType` gained a
`category` that groups the Add Component menu.

Alternatives: a separate script slot per object (two parallel systems to
save, inspect and undo); an ECS (the brief asks not to without a need).

### 2. Game modules are static libraries with one registration function

`LANTERN_GAME_MODULE(name) { registry.add(...); }` defines the function;
`lantern_add_game_module()` builds the library; `lantern_link_game_modules()`
links modules into an executable and **generates** the code that calls each
module's function (`registerGameModules()`). The editor, the player and the
tests call it at start-up. `LANTERN_GAME_MODULE_DIRS` adds outside folders.

Self-registration through static constructors was rejected: linkers drop
unreferenced objects from static libraries, so registrations would vanish
silently on some platforms. A generated, explicit call cannot be dropped.
Dynamic libraries and hot reload are left for later, as the brief says.

### 3. The game always plays a copy

`Game::start(scene)` copies the scene; behaviours change only the copy.
Stopping drops it. This makes "stop restores the edited scene" true by
construction rather than by undoing changes, and covers changes made by
gameplay code the editor cannot know about. While playing, the editor
refuses scene edits (with a Console message) instead of letting edits go
to a scene the user is not looking at.

### 4. The Scene view shows the game while playing

Play mode reuses the Scene view, seen through the game camera with the
rest dimmed, instead of opening a second window or panel: one rendering
path, and the game appears where the user is looking. The editor camera is
saved and restored. The view passes keys and the mouse to the game while
it has keyboard focus, except ⌘/Ctrl shortcuts. Lantern UI gained key
release events and modifier keys for this.

### 5. Input, collision and scene changes are engine types

`lantern::Input` uses Lantern's own `Key` names; the runtime (Axmol
keyboard and mouse) and the editor (Lantern UI events) both translate into
it, so game code never sees backend types.

Collision is axis-aligned boxes: `Collider` components and solid tilemap
cells (painted collision or solid tileset tiles). `Game::move` sweeps along
x then y, stopping at the first solid box, with a small touching tolerance
so rounding cannot let an object slip into a wall it rests against. This is
what top-down games need; a physics engine can be added behind a component
later if a game needs one.

Scene changes go through a `SceneLoader` callback, and scenes became assets
(`.lscene` files get `.meta` IDs) so gameplay refers to them by ID.

### 6. Errors in gameplay code are contained where possible

Exceptions thrown by a behaviour are caught, logged with the behaviour and
object names, and switch that behaviour off for the rest of the run.
Crashes cannot be contained in-process; running the game in a separate
process would allow that and is left for later.

### 7. Moving things does not rebuild the scene

A running game moves objects every frame. `Scene` now has a
`contentRevision()` that ignores Transform changes; when only that stays the
same, the renderer moves its existing nodes instead of rebuilding them.

## Consequences

- New library `lantern_gameplay` (no backend; tested in every preset); the
  runtime links it for `Input`. No new third-party code.
- Scene files gain `lantern.Collider` and behaviour components; format
  version stays 1 (older files remain valid). `.meta` files may now have
  type `"scene"`.
- The editor and player must be rebuilt to pick up changed gameplay code.
- A crashing behaviour closes the editor; documented in docs/gameplay.md.
