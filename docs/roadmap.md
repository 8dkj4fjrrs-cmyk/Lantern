# Lantern roadmap

Lantern is built milestone by milestone; each one leaves the engine
building, tested and documented. Milestones 0–8 come from the original
project brief. Items marked *added* were agreed later and are recorded here
so the plan stays in one place.

| # | Milestone | Status |
|---|---|---|
| 0 | Technical verification and foundation | Done (0.1) — [ADR 0001](adr/0001-runtime-foundation.md) |
| 1 | Runtime foundation: window, start-up, logging | Done — [ADR 0002](adr/0002-runtime-module-and-axmol-integration.md) |
| 2 | Minimal custom editor: docking, viewport, panels | Done — [ADR 0003](adr/0003-editor-architecture.md), [ADR 0004](adr/0004-lantern-ui-toolkit.md) |
| 3 | Scenes and serialization, stable IDs, undo/redo | Done — [ADR 0005](adr/0005-scenes-and-serialization.md) |
| 4 | Sprites, assets, Scene view camera, tilemaps and collision data | Done (0.4) — [ADR 0006](adr/0006-assets-sprites-and-tilemaps.md) |
| 5 | C++ gameplay behaviours, game modules, play mode, input, collision queries, scene changes | Done (0.5) — [ADR 0007](adr/0007-gameplay-and-play-mode.md) |
| 6 | RPG content systems: dialogue, quests, conditions, actions, events, content validation, save games | Done (0.6) — [ADR 0008](adr/0008-rpg-content.md) |
| 6.5 | *Added:* flipbook (sprite-sheet) animation | Done (0.6.5) — [ADR 0009](adr/0009-flipbook-animation.md) |
| 7 | Complete sample game: a small top-down RPG built from a fresh project | Next |
| 8 | Public alpha: project wizard, packaging and export, clean builds, onboarding, release | Planned |
| — | *Added, after the alpha:* timeline animation editor | Later |

## Milestone 6.5 — Flipbook animation (*added*)

Agreed on 2026-11-01, so that the sample RPG (Milestone 7) has animated
characters instead of sliding still images. It comes before Milestone 7 and
builds on what exists: image assets and their IDs, the way tilesets cut an
image into frames, the Inspector, and play mode.

Scope:
- **Animation clips as assets**: a sprite sheet cut into frames (like a
  tileset), named clips such as `idle`, `walk-left`, `walk-right`, each a
  list of frames with a speed (frames per second) and whether it loops.
- **An Animation editor panel**: choose frames by clicking the sheet,
  reorder them, set the speed and looping, and preview the clip playing.
- **An Animator component**: which clip set an object uses and which clip
  it starts with; the Scene view and play mode show it moving.
- **Gameplay API**: `play("walk-left")`, `stop()`, `isPlaying()`, and a way
  to notice a non-looping clip has finished (e.g. an attack).
- Versioned file format, undo for every edit, tests, and the sample game's
  hero walking in four directions.

Out of scope here: keyframing other properties (the timeline editor),
state machines, and skeletal animation.

Delivered in 0.6.5 as planned. The API names became `game.play(entity,
clip)`, `game.stopAnimation(entity)`, `game.isAnimationPlaying(entity)` and
`game.isAnimationFinished(entity)` (on `Game`, like the rest of the gameplay
API). The sample hero walks in four directions and the elder blinks.

## After the alpha: timeline animation editor (*added*)

Agreed on 2026-11-01 as a post-alpha item. A timeline editor animates by
keyframes: rows for properties of objects (position, rotation, scale,
colour, a sprite's image), keyframes at moments in time, curves for how
values move between keyframes (steady, easing, bounce), and events at chosen
times (e.g. play a sound). Uses: doors and chests opening, effects, UI
movement, and cutscenes.

Why after the alpha: it is one of the largest editor pieces (time ruler,
property rows, keyframe dragging, curve editing, scrubbing and preview,
recording changes from the Scene view, undo, file format, runtime
playback). Before then, walking characters are covered by flipbook clips
(6.5), small effects by behaviour code, and scripted sequences by
Milestone 6's events. Lantern's property system means any component property
can become a timeline row without extra work per component, which is the
main thing the earlier milestones should keep true.

## Also noted for later

- A visual (graph) dialogue editor. Milestone 6 edits dialogues as forms in
  the Inspector, as the brief asks (runtime and validation before complex
  visual editors).
- Item and inventory definitions, and localisation of text: listed among
  the brief's eventual content editors, not yet planned in a milestone.
- Multi-line text fields in Lantern UI (long dialogue lines are edited in
  one-line fields today; they wrap in the game).

Possible later steps for animation, not planned: animation state machines (switching
clips automatically from game values) and skeletal animation (better
imported from dedicated tools such as Spine or DragonBones than built).
