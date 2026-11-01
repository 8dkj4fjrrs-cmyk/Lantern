# Writing gameplay in C++

Lantern games are written in C++. You write **behaviours**: small classes
attached to objects, like components, that run every frame while the game
plays. This guide shows how, using the sample game in
[`examples/sample_game`](../examples/sample_game) as the running example.

Design reasons: [ADR 0007](adr/0007-gameplay-and-play-mode.md).

## A first behaviour

```cpp
#include "lantern/gameplay/Behaviour.hpp"
#include "lantern/gameplay/Game.hpp"

struct Spinner : lantern::BehaviourBase<Spinner> {
    float speed = 90.0f;   // degrees per second

    void update(lantern::Game& game, float dt) override
    {
        game.transform(entity())->rotation += speed * dt;
    }

    static const lantern::ComponentType& componentType()
    {
        static const lantern::ComponentType type =
            lantern::ComponentTypeBuilder<Spinner>("mygame.Spinner", "Spinner")
                .category("Gameplay")
                .description("Turns the object round and round.")
                .property("speed", "Speed", &Spinner::speed, {.step = 15.0f, .unit = "°/s"})
                .build();
        return type;
    }
};
```

- `BehaviourBase<Spinner>` makes it a component Lantern can copy, save and
  show in the Inspector.
- `componentType()` describes it: an ID that is saved in scene files (choose
  it once and never change it, e.g. `"mygame.Spinner"`), the name shown in
  the editor, and its **properties**. Properties appear in the Inspector, are
  saved in the scene, and can be undone like any other edit. Supported types:
  `bool`, `int`, `float`, `Vec2`, `Color`, `std::string`, enums (a choice),
  and `AssetId` (an image, tileset or scene; set `.assetType`).
- `.category("Gameplay")` lists it under *Gameplay* in **Add Component**.

### When behaviours run

While the game runs, every active object's behaviours get:

| Function | When |
|---|---|
| `start(Game&)` | once, in the first frame the object is active |
| `update(Game&, float dt)` | every frame; `dt` is the frame's length in seconds |

Objects run in hierarchy order (parents first, top to bottom), and an
object's behaviours in the order they are listed. Frames are never longer
than 0.1 s (`Game::kMaxStep`), so a hiccup cannot make things jump through
walls.

`entity()` is the object the behaviour is on.

## What `Game` offers

`lantern::Game` is the game being played ([`Game.hpp`](../engine/gameplay/include/lantern/gameplay/Game.hpp)).

**Input** — `game.input()`:

```cpp
const lantern::Input& in = game.input();
in.isDown(lantern::Key::Space);       // held down
in.wasPressed(lantern::Key::Space);   // went down this frame (a tap)
in.wasReleased(lantern::Key::Space);  // went up this frame
in.moveDirection();                   // arrows or W A S D, length at most 1
in.isDown(lantern::MouseButton::Left);
in.pointer();                         // the mouse, in world coordinates
```

**Objects:**

```cpp
lantern::Transform* t = game.transform(entity());   // move, turn, scale
t->position = t->position + lantern::Vec2{0, 10};
game.get<lantern::Sprite>(entity())->flipX = true;  // any component
lantern::EntityId door = game.find("Door");         // by name
auto gems = game.findAll<Collectible>();            // every object with one
game.worldPosition(entity());                       // parents included
game.destroy(entity());                             // removed at the end of the frame
```

Changing a `Transform` is cheap (the object is just moved). Changing other
components (a sprite's image, a colour) redraws the object, so only do it
when the value actually changes.

**Collision** — needs a **Collider** component on the objects involved, or
solid tiles in a tilemap:

```cpp
game.move(entity(), {speed * dt, 0});      // moves, stopping at walls and solid tiles
game.overlapping(entity());                // objects whose colliders overlap this one's
game.isBlocked(lantern::Box::around(p, {16, 16}));
game.colliderBox(entity());                // the collider's box in the world
```

`move()` slides along walls (it moves along x, then y) and returns what it
hit. Colliders are axis-aligned boxes; a **Solid** collider blocks movement,
a non-solid one is an area that gameplay code notices (a door, a pick-up).
A tilemap cell blocks movement when it is painted on the *Collision* layer,
or holds a tile marked solid in the tileset (Tile Palette > *Edit solid
tiles*).

**Scenes:**

```cpp
game.changeScene(sceneAssetId);   // at the end of this frame
```

Scenes are assets with IDs, so give your behaviour an `AssetId` property
with `.assetType = "scene"` and choose the scene in the Inspector (see
`SceneDoor` in the sample). The new scene starts fresh: objects of the old
one, and their behaviours, are gone.

**Other:** `game.time()` (seconds of game time), `game.deltaTime()`,
`game.log().info("game", "...")` (shown in the editor's Console).

## Game modules: joining your code to Lantern

Behaviours live in a **game module**: a static library linked into the
editor and the player when Lantern is built. A module has one registration
function:

```cpp
// MyGame.cpp
#include "lantern/gameplay/GameModule.hpp"

LANTERN_GAME_MODULE(my_game)
{
    registry.add(Spinner::componentType());
    registry.add(PlayerController::componentType());
}
```

and a `CMakeLists.txt`:

```cmake
lantern_add_game_module(my_game SOURCES MyGame.cpp Spinner.cpp PlayerController.cpp)
```

Build Lantern with your module's folder added:

```sh
cmake --preset dev -DLANTERN_GAME_MODULE_DIRS=/path/to/my_game
cmake --build build/dev
```

The editor's Console then says `Game module: my_game` at start-up, your
behaviours appear under *Gameplay* in **Add Component**, and both Play mode
and the player run them. Several folders can be listed, separated by `;`.

The module name must use only letters, digits and underscores, and must
match in `LANTERN_GAME_MODULE(...)` and `lantern_add_game_module(...)`.

Scenes that use a behaviour from a module that is not linked still open:
the component is kept unchanged (and saved back), shown as unknown.

**Rebuilding is needed after changing C++ code.** Lantern does not load code
while the editor runs (no hot reload) yet: quit the editor, build, start it
again. Your scenes are not affected.

## Playing

- **In the editor:** press ▶ in the toolbar (or ⌘P / Ctrl+P). The game runs
  inside the Scene view, seen through the game camera; click it so it gets
  the keyboard. ⏸ pauses, ⏭ runs one frame, ■ stops. The game plays a copy
  of the scene: stopping always returns to the scene exactly as it was.
  Scene edits are refused while playing.
- **In the player:** `lantern_player --project path/to/project` plays the
  project's start scene in its own window.

## Errors

If a behaviour throws a C++ exception, the error is shown in the Console
(with the behaviour's and object's names), that behaviour is switched off
for the rest of the run, and the rest of the game keeps going. A crash, such
as using a null pointer, cannot be caught this way and closes the program,
editor included: save before playing new code.

## The sample game

[`examples/sample_game`](../examples/sample_game) has four behaviours:
`PlayerController` (walking with collision), `SceneDoor` (changes scene when
the player walks in), `Collectible` (picked up on touch) and `Spinner`. Its
project (`examples/sample_game/project`) has a meadow and a cave. It is
built in by default (`LANTERN_BUILD_SAMPLE_GAME`):

```sh
./build/dev/bin/lantern_player/lantern_player --project examples/sample_game/project
```

(On macOS the program is inside `lantern_player.app/Contents/MacOS/`.)
Arrow keys or W A S D walk; walk right along the path to reach the cave.
