"""Writes the sample game's project files: settings, .meta files, the
tileset, two scenes (a meadow and a cave) and the RPG content (variables,
the elder's dialogue and the gem quest).

    python3 make_project.py <project folder>

Run make_art.py first. The output is committed; this script records how it
was made and can rebuild it. Scenes are ordinary Lantern scene files: open
the project in the editor to look at or change them (format:
docs/formats/scene.md).
"""
import json
import math
import random
import sys
from pathlib import Path

# Fixed IDs, so the files are the same every time this runs.
IDS = {
    "tiles.png": "5a4d1e0000000001",
    "hero.png": "5a4d1e0000000002",
    "tiles.ltileset": "5a4d1e0000000003",
    "meadow.lscene": "5a4d1e0000000004",
    "cave.lscene": "5a4d1e0000000005",
    "elder.png": "5a4d1e0000000006",
    "game.lvars": "5a4d1e0000000010",
    "elder.ldialogue": "5a4d1e0000000011",
    "gems.lquest": "5a4d1e0000000012",
}
GRASS, FLOWERS, PATH, WALL, WATER, CAVE_FLOOR, CAVE_WALL, MAT = range(8)
CELL = 16      # tile size in pixels
SCALE = 2      # tilemaps are drawn twice as large: one cell is 32 points
STEP = CELL * SCALE

_next_entity = [0]


def entity_id():
    _next_entity[0] += 1
    return f"e17e{_next_entity[0]:012x}"


def transform(x=0.0, y=0.0, rotation=0.0, scale=(1, 1)):
    return {"type": "lantern.Transform", "position": [x, y], "rotation": rotation, "scale": list(scale)}


def entity(name, components, parent=None, active=True):
    e = {"id": entity_id(), "name": name, "active": active, "components": components}
    if parent:
        e = {"id": e["id"], "parent": parent, "name": name, "active": active, "components": components}
    return e


def tilemap(width, height, paint):
    """A Tilemap component: paint(x, y) gives each cell's tile (or -1)."""
    rows = []
    for y in range(height):
        values = [paint(x, y) for x in range(width)]
        rows.append([y, 0] + values)
    return {
        "type": "lantern.Tilemap",
        "tileset": IDS["tiles.ltileset"],
        "cellSize": [CELL, CELL],
        "data": {"layers": [{"name": "Ground", "visible": True, "rows": rows}], "solid": []},
    }


def cell_center(x, y):
    return ((x + 0.5) * STEP, (y + 0.5) * STEP)


def text(name, x, y, words, parent, source="text", size=18, color="#ffffffff", align="left", width=0):
    return entity(name, [
        transform(x, y),
        {"type": "lantern.Text", "text": words, "source": source, "font": None, "size": size, "color": color,
         "align": align, "width": width},
    ], parent=parent)


def interface(camera_id):
    """What the player sees on screen: children of the camera, so they stay
    put while the world scrolls. The game view is 1280 x 720, centred on the
    camera."""
    items = [
        text("Gem Count", -620, 345, "Gems: {gems}", camera_id, size=22, color="#f7d65cff"),
        text("Quest Log", -620, 312, "", camera_id, source="questLog", size=16, color="#e8e8e8ff"),
        text("Help", 620, 345, "Arrows / WASD walk · E talks · F5 save · F9 load", camera_id, size=14,
             color="#ffffffb0", align="right"),
        text("Prompt", 0, -150, "", camera_id, source="interactionPrompt", size=18, color="#f2a93bff",
             align="center"),
    ]
    box = entity("Dialogue Box", [
        transform(0, -262),
        {"type": "lantern.Shape", "kind": "rectangle", "size": [1000, 166], "color": "#101018e8", "filled": True,
         "lineWidth": 2},
        {"type": "lantern.DialogueView", "marker": "> "},
    ], parent=camera_id, active=False)   # hidden while editing; the game shows it during dialogues
    items += [box,
              text("Speaker", -480, 68, "", box["id"], source="dialogueSpeaker", size=20),
              text("Line", -480, 38, "", box["id"], source="dialogueLine", size=18, width=960),
              text("Choices", -480, -8, "", box["id"], source="dialogueChoices", size=18, color="#c8d2ffff"),
              text("Continue", 480, -60, "E / Space", box["id"], size=12, color="#ffffff80", align="right")]
    return items


def player(x, y):
    hero = entity("Player", [
        transform(x, y, scale=(SCALE, SCALE)),
        {"type": "lantern.Sprite", "image": IDS["hero.png"], "size": [16, 24], "color": "#ffffffff",
         "flipX": False, "flipY": False},
        # Only the feet collide, so the head can overlap a wall's top edge.
        {"type": "lantern.Collider", "size": [12, 6], "offset": [0, -9], "solid": True},
        {"type": "lantern.Interactor", "key": "E"},
        {"type": "sample.PlayerController", "speed": 140},
    ])
    # The camera undoes the player's scale, so the interface is drawn 1:1.
    camera = entity("Camera", [transform(scale=(1 / SCALE, 1 / SCALE)), {"type": "lantern.Camera", "zoom": 1}],
                    parent=hero["id"])
    return [hero, camera] + interface(camera["id"])


def trigger(on, actions, condition=None, prompt="", event="", delay=1.0, once=False):
    return {"type": "lantern.Trigger", "on": on, "prompt": prompt, "event": event, "delay": delay, "once": once,
            "data": {"condition": condition, "actions": actions}}


def door(name, x, y, target):
    return entity(name, [
        transform(x, y),
        {"type": "lantern.Collider", "size": [24, 64], "offset": [0, 0], "solid": False},
        {"type": "sample.SceneDoor", "scene": IDS[target]},
    ])


def gem(name, x, y, message, with_code=False):
    """A spinning gem. In the meadow a Trigger picks it up (no code); in the
    cave the C++ Collectible behaviour does the same."""
    pickup = ({"type": "sample.Collectible", "message": message, "variable": "gems"} if with_code else
              trigger("enterArea", [{"type": "add", "variable": "gems", "value": 1},
                                    {"type": "log", "text": message},
                                    {"type": "destroy", "entity": None}]))
    return entity(name, [
        transform(x, y, rotation=45),
        {"type": "lantern.Shape", "kind": "rectangle", "size": [14, 14], "color": "#f7d65cff", "filled": True,
         "lineWidth": 2},
        {"type": "lantern.Collider", "size": [14, 14], "offset": [0, 0], "solid": False},
        pickup,
        {"type": "sample.Spinner", "speed": 90},
    ])


def elder(x, y):
    body = entity("Elder", [
        transform(x, y, scale=(SCALE, SCALE)),
        {"type": "lantern.Sprite", "image": IDS["elder.png"], "size": [16, 24], "color": "#ffffffff",
         "flipX": True, "flipY": False},
        {"type": "lantern.Collider", "size": [12, 6], "offset": [0, -9], "solid": True},
    ])
    talk = entity("Talk Area", [
        transform(0, -4),
        {"type": "lantern.Collider", "size": [36, 32], "offset": [0, 0], "solid": False},
        trigger("interact", [{"type": "startDialogue", "dialogue": IDS["elder.ldialogue"]}], prompt="Talk"),
    ], parent=body["id"])
    return [body, talk]


def boulder(x, y):
    """Blocks the way to the cave until the gem quest's reward opens it."""
    return entity("Boulder", [
        transform(x, y),
        {"type": "lantern.Shape", "kind": "ellipse", "size": [60, 60], "color": "#8a8478ff", "filled": True,
         "lineWidth": 2},
        {"type": "lantern.Collider", "size": [60, 64], "offset": [0, 0], "solid": True},
        trigger("conditionMet", [{"type": "log", "text": "The boulder rolls aside."}, {"type": "destroy", "entity": None}],
                condition={"type": "compare", "variable": "cave_open", "op": "==", "value": True}, once=True),
    ])


def scene(name, entities):
    return {"format": "lantern.scene", "version": 1, "name": name, "entities": entities}


def meadow():
    w, h = 48, 30
    rnd = random.Random(4)

    def paint(x, y):
        if (x == w - 1) and y in (14, 15):
            return MAT                                   # the way to the cave
        if x in (0, w - 1) or y in (0, h - 1):
            return WALL
        if x == 24 and 4 <= y <= 10:
            return WALL                                  # a ruin to walk around
        if ((x - 12) / 4.5) ** 2 + ((y - 21) / 3.2) ** 2 <= 1.0:
            return WATER
        if y in (14, 15) and x >= 3:
            return PATH
        return FLOWERS if rnd.random() < 0.08 else GRASS

    ground = entity("Ground", [transform(scale=(SCALE, SCALE)), tilemap(w, h, paint)])
    px, py = cell_center(4, 14)
    dx, dy = (w - 1 + 0.5) * STEP, 15 * STEP
    entities = [ground]
    entities += [gem("Gem 1", *cell_center(20, 22), "Picked up a gem by the pond"),
                 gem("Gem 2", *cell_center(30, 6), "Picked up a gem behind the ruin"),
                 gem("Gem 3", *cell_center(40, 24), "Picked up a gem in the long grass")]
    entities += elder(*cell_center(9, 17))
    entities.append(boulder(45.5 * STEP, 15 * STEP))
    entities.append(door("Door to Cave", dx, dy, "cave.lscene"))
    entities += player(px, py + STEP * 0.5)
    return scene("Meadow", entities)


def cave():
    w, h = 26, 16

    def paint(x, y):
        if x == 0 and y in (7, 8):
            return MAT                                   # the way back out
        if x in (0, w - 1) or y in (0, h - 1):
            return CAVE_WALL
        if (x, y) in {(8, 4), (9, 4), (8, 5), (16, 10), (17, 10), (17, 11), (20, 5)}:
            return CAVE_WALL                             # rocks
        return CAVE_FLOOR

    ground = entity("Ground", [transform(scale=(SCALE, SCALE)), tilemap(w, h, paint)])
    px, py = cell_center(3, 7)
    entities = [ground,
                gem("Gem 4", *cell_center(22, 12), "Picked up the cave's gem", with_code=True),
                door("Door to Meadow", 0.5 * STEP, 8 * STEP, "meadow.lscene")]
    entities += player(px, py + STEP * 0.5)
    return scene("Cave", entities)


def quest_is(state):
    return {"type": "quest", "quest": IDS["gems.lquest"], "state": state}


def gems_at_least(n):
    return {"type": "compare", "variable": "gems", "op": ">=", "value": n}


def variables():
    return {"format": "lantern.variables", "version": 1, "variables": [
        {"id": "gems", "type": "int", "initial": 0, "description": "Gems the player carries"},
        {"id": "met_elder", "type": "bool", "initial": False, "description": "Has talked to the elder"},
        {"id": "cave_open", "type": "bool", "initial": False, "description": "The boulder before the cave is gone"},
    ]}


def elder_dialogue():
    def line(node_id, words, next_node=None, actions=None, choices=None):
        n = {"id": node_id, "speaker": "elder", "text": words}
        if actions:
            n["actions"] = actions
        if choices:
            n["choices"] = choices
        else:
            n["next"] = next_node
        return n

    return {"format": "lantern.dialogue", "version": 1,
            "speakers": [{"id": "elder", "name": "Elder Maren", "color": "#c9a6ffff", "portrait": None}],
            "start": "begin",
            "nodes": [
                # Where the conversation starts depends on the quest.
                {"id": "begin", "branches": [
                    {"condition": quest_is("completed"), "next": "after"},
                    {"condition": {"type": "all", "conditions": [quest_is("active"), gems_at_least(3)]}, "next": "done"},
                    {"condition": quest_is("active"), "next": "waiting"}],
                 "next": "greet"},
                line("greet", "Ah, a traveller! Welcome to the meadow.", "ask",
                     actions=[{"type": "set", "variable": "met_elder", "value": True}]),
                line("ask", "A boulder blocks the old cave to the east. Bring me three gems from the meadow, and "
                            "their light will move it.", choices=[
                    {"id": "yes", "text": "I'll find them.",
                     "actions": [{"type": "startQuest", "quest": IDS["gems.lquest"]}], "next": "thanks"},
                    {"id": "no", "text": "Maybe later.", "next": "later"}]),
                line("thanks", "Thank you! Look by the pond, behind the ruin and in the long grass."),
                line("later", "Come back when you are ready."),
                line("waiting", "You have {gems} of 3 gems. Keep looking!"),
                line("done", "Three gems! Listen... the boulder is moving.", "opened",
                     actions=[{"type": "completeObjective", "quest": IDS["gems.lquest"], "objective": "return"}]),
                line("opened", "The way to the cave is open. Follow the path east."),
                line("after", "The cave lies east. Take care in there."),
            ]}


def gems_quest():
    return {"format": "lantern.quest", "version": 1,
            "name": "Gems for the Elder",
            "description": "Elder Maren needs three gems to move the boulder before the cave.",
            "prerequisites": [], "startWhen": None,
            "objectives": [
                {"id": "collect", "text": "Find 3 gems ({gems}/3)", "completeWhen": gems_at_least(3), "optional": False},
                {"id": "return", "text": "Bring them to Elder Maren", "completeWhen": None, "optional": False}],
            "failWhen": None,
            "rewards": [{"type": "set", "variable": "cave_open", "value": True},
                        {"type": "log", "text": "The way to the cave is open."}]}


def meta(asset_id, kind, extra=None):
    m = {"format": "lantern.asset", "version": 1, "id": asset_id, "type": kind}
    m.update(extra or {})
    return m


def to_json(value, indent=0):
    """JSON like the editor writes it: two-space indents, lists of plain
    values on one line, whole numbers without ".0"."""
    pad = "  " * indent
    if isinstance(value, float) and value.is_integer():
        value = int(value)
    if isinstance(value, dict):
        if not value:
            return "{}"
        items = [f'{pad}  {json.dumps(k)}: {to_json(v, indent + 1)}' for k, v in value.items()]
        return "{\n" + ",\n".join(items) + "\n" + pad + "}"
    if isinstance(value, list):
        if all(not isinstance(v, (dict, list)) for v in value):
            return "[" + ", ".join(to_json(v) for v in value) + "]"
        items = [pad + "  " + to_json(v, indent + 1) for v in value]
        return "[\n" + ",\n".join(items) + "\n" + pad + "]"
    return json.dumps(value, ensure_ascii=False)


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(to_json(data) + "\n")


def main():
    root = Path(sys.argv[1])
    write(root / "project.lantern", {"format": "lantern.project", "version": 1, "name": "Lantern Sample",
                                     "startScene": "scenes/meadow.lscene"})
    write(root / "assets/tiles.png.meta", meta(IDS["tiles.png"], "image", {"filter": "pixel"}))
    write(root / "assets/hero.png.meta", meta(IDS["hero.png"], "image", {"filter": "pixel"}))
    write(root / "assets/elder.png.meta", meta(IDS["elder.png"], "image", {"filter": "pixel"}))
    write(root / "content/game.lvars", variables())
    write(root / "content/game.lvars.meta", meta(IDS["game.lvars"], "variables"))
    write(root / "content/elder.ldialogue", elder_dialogue())
    write(root / "content/elder.ldialogue.meta", meta(IDS["elder.ldialogue"], "dialogue"))
    write(root / "content/gems.lquest", gems_quest())
    write(root / "content/gems.lquest.meta", meta(IDS["gems.lquest"], "quest"))
    write(root / "assets/tiles.ltileset", {"format": "lantern.tileset", "version": 1, "image": IDS["tiles.png"],
                                           "tileSize": [CELL, CELL], "margin": 0, "spacing": 0,
                                           "solid": [WALL, WATER, CAVE_WALL]})
    write(root / "assets/tiles.ltileset.meta", meta(IDS["tiles.ltileset"], "tileset"))
    write(root / "scenes/meadow.lscene", meadow())
    write(root / "scenes/meadow.lscene.meta", meta(IDS["meadow.lscene"], "scene"))
    _next_entity[0] = 100
    write(root / "scenes/cave.lscene", cave())
    write(root / "scenes/cave.lscene.meta", meta(IDS["cave.lscene"], "scene"))


if __name__ == "__main__":
    main()
