# ADR 0008: RPG content — dialogue, quests, events and save games (Milestone 6)

- **Status:** Accepted
- **Date:** 2026-11-02
- **Milestone:** 6

## Context

Milestone 6 asks for dialogue, quests, conditions, actions and events,
content validation, save-game state and loading, and an initial data-driven
content workflow. The brief adds firm rules: structured, versioned formats;
never execute code from project files; conditions and actions are "a
defined, validated system rather than unrestricted expressions"; authored
definitions apart from per-save progress; and runtime plus validation
before complex visual node editors.

## Decisions

### 1. Content is plain data in its own library and asset files

`lantern_content` (no backend, tested in every preset) holds the formats
and their logic. Variables (`.lvars`), dialogues (`.ldialogue`) and quests
(`.lquest`) are assets with `.meta` IDs like images, so files refer to each
other by ID and renaming never breaks anything. Inside a file, items
(variables, nodes, choices, objectives, speakers) have short author-visible
ids (`gems`, `ask`) that are saved and never changed, which keeps files
readable and diffs small.

Alternatives: one big content database per project (merge conflicts, one
broken entry breaks everything); content inside scene files (a dialogue
would belong to one scene).

### 2. Conditions and actions are a closed set of typed objects

Every condition and action is a JSON object with a `"type"` from a fixed
list (compare, quest, objective, all/any/not; set, add, startQuest,
completeObjective, failQuest, startDialogue, changeScene, setActive,
destroy, emit, log, wait, saveGame, loadGame). Reading checks the shape;
`ContentLibrary::check()` checks references and types across the project.
There is no expression language, no loops, no arithmetic beyond `add`.
Anything richer is C++ in a game module, which can run the same actions
(`game.content().run(...)`) and react to the same events.

### 3. A written form for typing, not a language

Forms for nested conditions and action lists would be large and slow to
use, so the editor shows each condition and action as one line of text
(`gems >= 3 and not met_elder`, `add gems 1`) and turns it into the typed
structure when the field is left (`Notation.hpp`). Files keep the JSON. The
written form has exactly the expressive power of the typed set, each line
is one condition tree or one action, names are resolved to IDs (and back)
at the edges, and every mistake is reported at once in place. Formatting
and reading back give the same structure (tested).

### 4. Progress is GameState; save files hold only progress

`GameState` holds variable values, quest progress, objects removed for good
and one-time triggers that ran (per scene), the scene and the player's
position. A save file is exactly that, versioned, and reading it against
newer content keeps working (dropped variables and quests are ignored,
changed types reset, new variables start at their initial value). Saves
are written atomically (to a temporary file, then renamed), to the user's
data folder for games and to the editor's settings for Play mode.

### 5. Scenes use content through three components

`Trigger` (interact, enter/leave area, scene start, timer, game event,
condition met; with a condition and actions stored as component data),
`Interactor` (the player) and `DialogueView`, plus a new `Text` component
whose *source* can show the dialogue, the interaction prompt or the quest
log. `Game` runs them every frame through a `ContentRunner`. The dialogue
box is therefore an ordinary part of the scene, laid out and styled in the
editor like anything else, rather than a fixed built-in panel.

### 6. Editing is form-based; a visual graph editor waits

Following the brief, Milestone 6 delivers the runtime and validation, and
edits content as forms in the Inspector (lists of lines, choices,
objectives) with undo (`ChangeAssetText`) and live problem lists. A visual
dialogue graph editor is noted on the roadmap.

## Consequences

- New library `lantern_content`; `lantern_gameplay` links it. No new
  third-party code. Text is drawn with Axmol labels; the editor's Inter font
  is reused as games' default font (written to a cache file once).
- New file formats (documented in docs/formats/content.md) and new scene
  components (docs/formats/scene.md); scene format version stays 1.
- The editor's play mode and the player check content when they start and
  report problems in the Console / log without refusing to run.
- Text edits rebuild the drawn scene (as any non-movement change does);
  fine at this scale, and noted with the renderer's incremental updates.
