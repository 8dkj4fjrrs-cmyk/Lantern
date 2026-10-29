# ADR 0004 — Lantern's own UI toolkit for the editor

- **Status:** Accepted
- **Date:** 2026-10-29
- **Milestone:** 2b (replaces the Dear ImGui interface from Milestone 2)
- **Supersedes:** the Dear ImGui parts of [ADR 0003](0003-editor-architecture.md)

## Context

Milestone 2 built the editor with Dear ImGui, as the brief suggested. The
project owner decided against it before using it: the editor should be
Lantern's own, should not look like a debugging tool, and Dear ImGui "feels
wrong to use". The options were another third-party library (Qt, a web view,
RmlUi, Nuklear, …) or a toolkit written for Lantern. The owner chose a
toolkit of our own.

## Decisions

### 1. A retained-mode toolkit in `engine/ui` (`lantern::ui`)

Widgets are objects that live as long as the window (a tree of `Widget`s),
not code that redraws everything each frame. This is how desktop toolkits
work and suits an editor: panels keep their own state (scroll position,
selection, text being typed), and keyboard focus, tab order and accessibility
can be added naturally later.

Each frame: the application updates what changed → `Context::render()`
measures and arranges the tree, paints it into a `DrawList` (triangles,
texture, clip rectangle) → the runtime draws that list.

### 2. No graphics API, no window system, no Axmol in `engine/ui`

`lantern_ui` only produces vertices and reads abstract input events
(`Context::pointerMove`, `keyDown`, …). Two consequences:

- The whole toolkit — layout, text editing, docking, menus, dialogs — is
  unit-tested without a window, in every preset including `core` and the
  sanitizers.
- If the backend changes (ADR 0001 keeps that possible), only the small
  bridge `engine/runtime/src/UiHost.*` changes. It turns GLFW input into
  `Context` events and the `DrawList` into Axmol render commands (Metal on
  macOS, OpenGL elsewhere), with clipping, the glyph texture and the
  off-screen world image.

### 3. Text: stb_truetype + Inter, embedded

- Glyphs are rasterised with `stb_truetype` (one header, MIT/public domain)
  into a texture atlas that grows as needed and is re-rendered at the
  screen's pixel density (sharp on Retina).
- The editor's typeface is **Inter** (SIL Open Font License), Regular and
  SemiBold, embedded in the program. The editor looks the same on every
  computer and needs no installed fonts. This replaces Milestone 2's search
  for system fonts.
- The toolkit uses real typography: "…" for truncation, ⌘ in macOS
  shortcuts, × and ° in values.

Not yet supported (and not needed for an English-language editor):
right-to-left scripts, complex shaping, colour emoji, glyph fallback for
characters Inter lacks. HarfBuzz/FreeType can replace stb_truetype later
behind `FontAtlas` if localisation needs them.

### 4. Platform conventions follow the operating system

On macOS, ⌘ is the main shortcut key and ⌥ moves by word; elsewhere Ctrl
does both (`Platform::isPrimary`, `isWordModifier`). Menus show the matching
labels (⌘Q / Ctrl+Q).

### 5. Docking is plain data

`DockLayout` is a binary tree of splits and tab groups, saved as one line of
text (`layout.dock`), e.g.

    lantern-dock 1 (h 0.180 [0|hierarchy] (h 0.710 (v 0.700 [0|viewport] [0|console]) [0|inspector]))

The format is versioned; unknown panels are dropped, unreadable files fall
back to the default layout. A closed panel reopens next to its old
neighbours. The `DockSpace` widget turns this into tabs, splitters and
drag-and-drop.

### 6. Scope: what the editor needs, built when it needs it

Milestone 2b provides: layout boxes (row, column, wrapping flow), labels,
buttons, checkboxes, text fields (selection, clipboard, moving by word),
scrolling lists, tree view, property list, image view, menu bar and menus,
modal dialogs, tooltips, keyboard focus, and docking. Inspector editing
widgets (number fields, colour pickers, drop-downs) arrive with Milestone 3,
when there is something to edit.

## Consequences

- Dear ImGui is gone: Axmol's ImGui extension is switched off and nothing
  links it. The player no longer carries an unused UI library.
- More code to own (about 8,000 lines including tests). In exchange the editor's
  look and behaviour are fully ours, and the toolkit can later power in-game
  menus too.
- New third-party notices: stb_truetype and Inter (see
  `THIRD_PARTY_NOTICES.md`).
- `layout.ini` from the Milestone 2 preview is ignored; the editor starts
  with the default layout once and then saves `layout.dock`.

## Alternatives considered

| Option | Why not |
|---|---|
| Keep Dear ImGui, restyle it | Owner rejected it; immediate-mode still shapes the interaction model. |
| Qt Widgets / QML | Very capable, but a second large framework (LGPL or paid licence), its own window and event loop alongside Axmol's, and a much bigger build for a beginner's Mac. |
| Web view (HTML/CSS) | Two languages and a browser engine inside the editor; harder to debug. |
| RmlUi / Nuklear / other small libraries | Still someone else's look and model; RmlUi adds an HTML/CSS-like layer. |
