# Lantern file formats: projects and scenes

Lantern saves projects and scenes as UTF-8 JSON text. The files are meant to
be readable, to diff cleanly in version control, and to keep working when
Lantern changes. This page describes format **version 1** of both.

Code: `engine/scene` (`SceneFile.hpp`, `Project.hpp`). Design reasons:
[ADR 0005](../adr/0005-scenes-and-serialization.md).

## A project

A project is a folder:

```
My Game/
  project.lantern      project settings
  scenes/
    main.lscene        one file per scene
```

`project.lantern`:

```json
{
  "format": "lantern.project",
  "version": 1,
  "name": "My Game",
  "startScene": "scenes/main.lscene"
}
```

| Key | Meaning |
|---|---|
| `format` | Always `"lantern.project"`. |
| `version` | Format version (an integer). |
| `name` | The game's name. |
| `startScene` | The scene the player opens first. |

**Paths inside a project are always relative to the project folder and use
forward slashes** (`scenes/main.lscene`), on every operating system. Absolute
paths and `..` that would leave the project are refused. A project can
therefore be moved, zipped, shared or put in git.

## A scene (`.lscene`)

```json
{
  "format": "lantern.scene",
  "version": 1,
  "name": "Main",
  "entities": [
    {
      "id": "43e4ca317c1f4e40",
      "name": "Lantern",
      "active": true,
      "components": [
        { "type": "lantern.Transform", "position": [640, 360], "rotation": 0, "scale": [1, 1] }
      ]
    },
    {
      "id": "c79279ca12809364",
      "parent": "43e4ca317c1f4e40",
      "name": "Flame",
      "active": true,
      "components": [
        { "type": "lantern.Transform", "position": [0, -8], "rotation": 0, "scale": [1, 1] },
        { "type": "lantern.Shape", "kind": "ellipse", "size": [44, 44], "color": "#ffb84dff",
          "filled": true, "lineWidth": 2 }
      ]
    }
  ]
}
```

### Entities

| Key | Meaning |
|---|---|
| `id` | The entity's permanent ID: 16 hexadecimal digits, unique in the scene. Never reused or changed. Other files (and later, saved games) refer to entities by this ID, not by name. |
| `parent` | ID of the parent entity. Missing for top-level entities. |
| `name` | Any text; names do not have to be unique. |
| `active` | `false` hides the entity and everything inside it. Optional, default `true`. |
| `components` | The entity's components (below). Every entity has a `lantern.Transform`. |

Entities are listed **parents before children**, and siblings in their order.
That order is also the drawing order: later siblings are drawn on top, and
children on top of their parent.

### Coordinates

World units are points. The origin is the **bottom-left** corner of the game
view and y points **up** (the convention of the runtime). Positions are
relative to the parent. Rotation is in degrees, **clockwise**. Scale 1 is
normal size.

### Components (version 1)

**`lantern.Transform`**: where the entity is. Every entity has exactly one.

| Key | Type | Default |
|---|---|---|
| `position` | `[x, y]` | `[0, 0]` |
| `rotation` | number, degrees clockwise | `0` |
| `scale` | `[x, y]` | `[1, 1]` |

**`lantern.Shape`**: a rectangle or ellipse centred on the entity.

| Key | Type | Default |
|---|---|---|
| `kind` | `"rectangle"` or `"ellipse"` | `"rectangle"` |
| `size` | `[width, height]`, not negative | `[100, 100]` |
| `color` | `"#rrggbb"` or `"#rrggbbaa"` | `"#ffffffff"` |
| `filled` | `true` / `false` | `true` |
| `lineWidth` | outline thickness when not filled | `2` |

Sprites, cameras and tilemaps arrive in Milestone 4, and gameplay components
in Milestone 5.

## How Lantern reads files

Loading is forgiving when that is safe, and strict when guessing would lose
or scramble data.

| Situation | What happens |
|---|---|
| A property or field that is not known | Skipped, with a warning. |
| A value of the wrong type or out of range | Replaced by the default (or clamped), with a warning. |
| A component type that is not known (from a newer Lantern, or a game module that is not loaded) | **Kept unchanged** and written back on save, with a warning. Nothing is lost. |
| An entity without a Transform | Gets the default Transform (position 0, 0). |
| A missing, malformed or duplicate `id` | **Error**: the file is not loaded. |
| A `parent` that is not an entity listed earlier in the file | **Error**. (Requiring parents first also makes loops impossible.) |
| `version` newer than this Lantern understands | **Error** asking you to update Lantern. |
| Not JSON, or `format` is not `lantern.scene` | **Error**. |

Every warning and error says where the problem is (entity name and ID,
component, property) and, where possible, what to do. The editor shows them
in the Console; the player prints them.

## How Lantern writes files

- Keys always appear in the same order, numbers are written as short as
  possible, short lists stay on one line: saving an unchanged scene gives an
  identical file, and a small change gives a small diff.
- Files are written **atomically**: to a temporary file first, then renamed
  over the old one. A crash or full disk never leaves a half-written scene.
- Editor state is never saved in a scene: no selection, no undo history, no
  panel layout, no runtime objects.

## Versions and migration

Each file states its format version. When a format changes, its version
number goes up, and Lantern gets an *upgrade step* that turns a version N
document into version N + 1 (`kUpgrades` in `SceneFile.cpp`). Older files are
upgraded step by step when they are read, and saved in the newest version.
Files from a **newer** Lantern are refused rather than half-read.
