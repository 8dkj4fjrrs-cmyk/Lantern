"""Writes the sample game's project files: settings, .meta files, the
tileset and two scenes (a meadow and a cave).

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


def entity(name, components, parent=None):
    e = {"id": entity_id(), "name": name, "active": True, "components": components}
    if parent:
        e = {"id": e["id"], "parent": parent, "name": name, "active": True, "components": components}
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


def player(x, y):
    hero = entity("Player", [
        transform(x, y, scale=(SCALE, SCALE)),
        {"type": "lantern.Sprite", "image": IDS["hero.png"], "size": [16, 24], "color": "#ffffffff",
         "flipX": False, "flipY": False},
        # Only the feet collide, so the head can overlap a wall's top edge.
        {"type": "lantern.Collider", "size": [12, 6], "offset": [0, -9], "solid": True},
        {"type": "sample.PlayerController", "speed": 140},
    ])
    camera = entity("Camera", [transform(), {"type": "lantern.Camera", "zoom": 1}], parent=hero["id"])
    return [hero, camera]


def door(name, x, y, target):
    return entity(name, [
        transform(x, y),
        {"type": "lantern.Collider", "size": [24, 64], "offset": [0, 0], "solid": False},
        {"type": "sample.SceneDoor", "scene": IDS[target]},
    ])


def gem(name, x, y, message):
    return entity(name, [
        transform(x, y, rotation=45),
        {"type": "lantern.Shape", "kind": "rectangle", "size": [14, 14], "color": "#f7d65cff", "filled": True,
         "lineWidth": 2},
        {"type": "lantern.Collider", "size": [14, 14], "offset": [0, 0], "solid": False},
        {"type": "sample.Collectible", "message": message},
        {"type": "sample.Spinner", "speed": 90},
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
                gem("Gem 4", *cell_center(22, 12), "Picked up the cave's gem"),
                door("Door to Meadow", 0.5 * STEP, 8 * STEP, "meadow.lscene")]
    entities += player(px, py + STEP * 0.5)
    return scene("Cave", entities)


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
