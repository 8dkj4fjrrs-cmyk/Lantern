# ADR 0009: Flipbook animation (Milestone 6.5)

- **Status:** Accepted
- **Date:** 2026-11-02
- **Milestone:** 6.5 (added to the plan on 2026-11-01; see the roadmap)

## Context

The sample RPG (Milestone 7) needs characters that walk instead of sliding
still images. The roadmap scoped this milestone as sprite-sheet ("flipbook")
animation: clips as assets, an editor panel to make them, an Animator
component, and a gameplay API, with a versioned format, undo, tests and the
sample hero walking in four directions. Keyframing other properties (a
timeline editor), animation state machines and skeletal animation are out
of scope.

## Decisions

### 1. An animation set is its own asset: one sheet, many named clips

`.lanim` files hold one sprite sheet (an image, by ID) cut into a grid of
equal frames exactly like a tileset (same frame numbering and grid maths),
and any number of clips, each a list of frame numbers with a speed and a
loop flag. One file per character keeps a character's clips together, so
renaming a clip or changing the frame size happens in one place, and
gameplay code names clips (`"walk_left"`) the way content names variables.
Clip ids follow the content-id rule (letters, digits, `_`).

Alternatives: one file per clip (a character would need eight files that
must agree on the sheet and frame size); clips inside scene files (shared
characters would repeat them); frame lists of separate images (simple, but
sprite sheets are how pixel artists work and what the tile grid already
handles).

### 2. The Animator decorates a Sprite; it does not replace it

The Sprite keeps size, colour, opacity and flipping; an Animator with an
animation set replaces only the image, with the current frame. Objects
therefore keep working when the Animator is removed, the Inspector stays
familiar, and existing tools (resize, flip) apply unchanged. An Animator
without a Sprite draws frames at their pixel size.

### 3. Frames are a function of a clock, not state that ticks

The frame shown is computed from `(clock - startTime) * speed` and the clip
(`animatorFrame`, in `lantern_assets`, unit-tested). Nothing advances per
frame and nothing is saved besides the clip choice. `startTime` is a
runtime-only field (not a property, so never written to scenes). The clock
is the game's time in a game (so pausing, stepping and fixed-step test runs
pause and repeat animations exactly), and the runtime's real-time clock
while editing (so the Scene view plays clips). `Application::setAnimationTime`
/ `UiFrame::setAnimationTime` choose it each frame.

### 4. Switching clips does not rebuild the scene

The renderer keeps a list of animated sprites and, every frame after its
normal update, reads each Animator and changes the sprite's texture
rectangle only when the frame changed. `Game::play()` changes the clip
through `Scene::editAnimatorPlayback()`, which counts as a *move* (the
scene's `revision()` goes up, its `contentRevision()` does not), so a
character switching between walk and idle clips costs no rebuild, like
moving it. Changing which animation set an Animator uses is a content change
and rebuilds as usual.

### 5. Editing: an Animation panel plus the Inspector, every change undoable

The Inspector creates sets ("Create Animations…" on an image), edits the
sheet settings, and offers an Animator's clip as a list of the set's clips.
The Animation panel edits clips: the sheet with its grid (click a frame to
add it, Shift-click for a run of frames), the clip's frames in order
(select, move, remove), speed and looping, and a live preview. Every change
is written to the file at once as a `ChangeAssetText` step, so undo and redo
cover animation edits like scene edits, as for tilesets and content.

## Consequences

- New asset type `animation` (`.lanim`, format `lantern.animations` version
  1, documented in docs/formats/scene.md), new component `lantern.Animator`.
  The scene format version stays 1: older scenes have no Animators, and older
  Lantern versions keep an Animator they do not know as an unknown component.
- No new third-party code.
- `AssetDatabase::find()` pointers are documented as short-lived: records
  are rebuilt on every refresh. The renderer copies the animation set it
  draws. (Found while building this: refresh also dropped the parsed
  definition of unchanged `.lanim` files; fixed with a regression test.)
- A text field that has no use for Enter now passes it on, so Enter presses
  a dialog's main button (e.g. naming a clip). Regression test added.
- Tests that share a temporary folder name now get one of their own, since
  ctest runs test cases in parallel processes.
- Later, if wanted: animation state machines (switching clips from game
  values) and the post-alpha timeline editor. The timeline will animate
  component properties by keyframes; flipbook clips stay the tool for
  frame-by-frame sprites.
