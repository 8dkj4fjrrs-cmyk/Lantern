# ADR 0006: Assets, sprites, the Scene view and tilemaps (Milestone 4)

- **Status:** Accepted
- **Date:** 2026-10-31
- **Milestone:** 4

## Context

Milestone 4 asks for sprites that can be displayed and manipulated, asset
import and organisation, camera controls and viewport navigation, and basic
tilemap editing with collision data. The brief's asset rules also apply:
thumbnails and metadata, tracked references, detection of missing and
invalid files, project-relative paths and stable asset identifiers.

## Decisions

### 1. Asset IDs live in `.meta` files beside each asset

A new library, `lantern_assets` (`engine/assets`), keeps an `AssetDatabase`
of a project's images, tilesets, sounds and fonts. Each asset file gets a
`.meta` file next to it with a random 64-bit `AssetId` (same form as entity
IDs). Scenes store the ID, never a path.

- Renaming or moving inside the editor moves the `.meta` too, so references
  survive. Moving outside the editor works as long as the `.meta` travels
  with the file (as in Unity).
- Files copied in by hand get a `.meta` on the next refresh (every two
  seconds in the editor). A copied file *and* `.meta` (duplicate ID) is
  detected and the copy gets a new ID.
- A `.meta` without its file, an unreadable image, or a scene referring to
  an unknown ID is reported in the Console and shown in the editor; scenes
  still open, with a pink placeholder.

Alternatives: a single project-wide asset table (one file that every
change touches and that conflicts in version control); content hashes (an
edited image would become a different asset). Sidecar files avoid both.

The database checks images with **stb_image 2.30** (MIT/public domain, the
same stb commit as the font rasteriser), compiled privately because Axmol
links its own copy. `lantern_assets` has no backend and is tested in every
preset. Games open it **read-only** (no `.meta` files are written).

### 2. Sprites and tilemaps are components; sizes live in the scene

`lantern.Sprite` refers to an image by ID and stores its size in points
(set to the image's pixel size when chosen). `lantern.Tilemap` refers to a
tileset by ID and stores its cell size. Storing sizes in the scene keeps
picking, outlines and the Rect tool working from scene data alone, without
loading images, and lets art be swapped without moving everything.

Tilemap cells are sparse (only painted cells are stored), in named layers,
plus a separate set of painted **collision** cells. A cell blocks movement
if it is painted solid or a visible layer has a tile the **tileset** marks
solid (`isTileSolid`). Tilesets are small JSON assets (`.ltileset`) that
reference their image by ID.

Tile data does not fit the property model, so components gained an
optional extra-data hook (`saveData` / `loadData`, stored as `"data"`), and
the tilemap saves one line per row of tiles for readable diffs. Unknown
data stays the component's own business; the property model is unchanged.

### 3. The Scene view is a free camera over a pixel-sized render

The editor renders the world into an off-screen image the size of the
Scene panel **in real pixels** (Retina included), with its own projection,
rather than stretching the fixed game resolution. Zooming in stays sharp
and pixel art stays crisp. The camera (centre and zoom) belongs to the
editor; the game's view is drawn as a frame. The `lantern.Camera`
component decides what the player sees; the player applies it every frame.

### 4. Tools work on scene data, through undo

Move, Rotate, Scale and Rect tools, and the tile Brush, Eraser, Fill and
Picker, compute their results from scene data and change it only through
commands. New commands: `SetProperties` (several properties at once, e.g.
size and position for Rect), `PaintTiles` (a stroke's cells, merged into
one step), `ReplaceComponent` (layer changes). Each drag or stroke is one
undo step.

Tileset edits (tile size, solid tiles) are saved straight to the tileset
asset and are not part of a scene's undo history, because tilesets are
shared between scenes.

### 5. Import without native file dialogs

Files are imported by dropping them on the window (GLFW's drop event, UTF-8
paths converted per platform), with an Import dialog that takes a path, or
by copying them into the project folder. Native open dialogs can be added
later behind the same `importFiles()`.

## Consequences

- New library `lantern_assets`; the runtime links it and draws sprites and
  tiles from it. New third-party code: stb_image (notices updated).
- Scene files gain `lantern.Sprite`, `lantern.Camera` and `lantern.Tilemap`
  and component `"data"`; format version stays 1, since older files remain
  valid and unknown components are preserved.
- Collision is data only until Milestone 5 adds movement and collision
  queries to gameplay.
- Large maps: the renderer rebuilds all nodes when the scene changes, which
  is fine for editor-sized scenes; incremental updates can come later
  behind the same interface.
