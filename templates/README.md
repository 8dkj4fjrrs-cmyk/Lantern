# Project templates

Starting points offered by the editor's **New Project** wizard. Each folder
holds `template.json` (the name and description the wizard shows) and the
`project/` that is copied for a new project. The example games in
`examples/` that have a `template.json` are offered too.

- `empty/`: one empty scene, as made by `lantern_editor --new-project`.
- `topdown/`: the **Top-Down Starter**, a small game made only of built-in
  components (Top-Down Controller, Scene Door, Camera Bounds), with
  dialogue and a first quest. It is made by `tools/make_starter.py` from a
  fresh project, using art from The Lantern Keeper (original, MIT).

To add a template, put a Lantern project in `<name>/project/` and describe it
in `<name>/template.json`:

```json
{ "name": "Platformer", "description": "One line about it.", "order": 3 }
```
