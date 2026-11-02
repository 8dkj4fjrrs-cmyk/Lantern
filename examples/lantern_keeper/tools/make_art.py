"""Draws The Lantern Keeper's pixel art. Original art, MIT like the rest.

    python3 make_art.py <folder>

Writes into <folder>:
  tiles.png      24 tiles of 16 x 16 pixels, 8 per row (see TILES below)
  hero_walk.png  the hero, 16 x 24 frames: rows down, left, right, up;
                 columns standing, left foot forward, right foot forward
  maren.png, wren.png, nell.png
                 villagers, two 16 x 24 frames each (the second for idling)
  biscuit.png    the cat, two 16 x 16 frames (tail down, tail up)
  flame.png      a lost flame, four 16 x 16 frames
  lantern.png    the great lantern on its post, three 32 x 64 frames:
                 dark, lit, lit (flickered)
  shrine.png     the wayshrine (the save point), two 16 x 24 frames
  gate.png       the mine gate, 32 x 32
  sign.png       a signpost, 16 x 16

The results are committed; run it again only to change the art.
"""
import random
import sys
from pathlib import Path

from PIL import Image

T = 16

# Tile numbers (the order in tiles.png). Keep in sync with build_project.py.
TILES = ["grass", "flowers", "path", "water", "stone_wall", "tree", "house_wall", "roof",
         "door", "fence", "forest_floor", "bush", "mine_floor", "mine_wall", "planks", "window",
         "cobbles", "forest_tree", "rocks", "mushrooms", "well", "rails", "crate", "reeds"]


def rgb(hex_colour, a=255):
    h = hex_colour.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


GRASS, GRASS_DARK = rgb("4c8c40"), rgb("3c7634")
FOREST, FOREST_DARK = rgb("2f5a35"), rgb("264a2c")
PATH, PATH_DARK = rgb("b08c5c"), rgb("9c7a4e")
LEAF, LEAF_DARK, LEAF_LIGHT, TRUNK = rgb("2e6b2e"), rgb("1f4f22"), rgb("4f9a43"), rgb("6b4a2b")
STONE, STONE_DARK = rgb("8f8f9c"), rgb("5c5c6a")
WOOD, WOOD_DARK = rgb("a0703f"), rgb("74502c")
ROOF, ROOF_DARK = rgb("b8483c"), rgb("8a3229")
WATER, WATER_LIGHT = rgb("3470c4"), rgb("aad6f0")
MINE, MINE_DARK, MINE_WALL, MINE_EDGE = rgb("3a3442"), rgb("302b38"), rgb("4e445c"), rgb("221e28")


def paint_tile(img, index, painter):
    ox, oy = (index % 8) * T, (index // 8) * T
    rnd = random.Random(index * 7919 + 11)
    for y in range(T):
        for x in range(T):
            c = painter(x, y, rnd)
            if c is not None:
                img.putpixel((ox + x, oy + y), c)


def speckle(base, dark, amount=0.14):
    return lambda x, y, r: dark if r.random() < amount else base


def grass(x, y, r):
    return speckle(GRASS, GRASS_DARK)(x, y, r)


def flowers(x, y, r):
    if (x, y) in {(3, 4), (11, 9), (6, 12), (13, 2)}:
        return rgb("f7d65c")
    if (x, y) in {(4, 4), (12, 9), (7, 12), (14, 2), (3, 3), (11, 8)}:
        return rgb("f0ece4")
    return grass(x, y, r)


def path(x, y, r):
    return speckle(PATH, PATH_DARK, 0.16)(x, y, r)


def water(x, y, r):
    return WATER_LIGHT if (x + y * 2) % 9 in (0, 1) and y % 4 == 1 else WATER


def bricks(base, edge):
    def p(x, y, r):
        row = y // 4
        line = y % 4 == 3 or (x + (4 if row % 2 else 0)) % 8 == 7
        return edge if line else base
    return p


def tree_on(ground):
    """A round tree top over a trunk, on the given ground."""
    def p(x, y, r):
        dx, dy = x - 7.5, y - 6.5
        if dx * dx + dy * dy <= 42:
            if dx * dx + (dy + 2) ** 2 <= 10:
                return LEAF_LIGHT
            return LEAF_DARK if (dy > 3 or r.random() < 0.12) else LEAF
        if 6 <= x <= 9 and y >= 11:
            return TRUNK
        return ground(x, y, r)
    return p


def house_wall(x, y, r):
    if x % 4 == 3:
        return WOOD_DARK
    return WOOD if r.random() > 0.08 else WOOD_DARK


def roof(x, y, r):
    if y % 4 == 3:
        return ROOF_DARK
    return ROOF_DARK if (x + (2 if (y // 4) % 2 else 0)) % 4 == 3 else ROOF


def door(x, y, r):
    if x in (0, 15) or y == 0:
        return WOOD_DARK
    if 3 <= x <= 12 and y >= 2:
        if x == 10 and y == 9:
            return rgb("e8c050")                         # the handle
        return rgb("5a3a20") if x in (3, 12) or x == 7 else rgb("6e4626")
    return house_wall(x, y, r)


def window(x, y, r):
    if 3 <= x <= 12 and 3 <= y <= 11:
        if x in (3, 12) or y in (3, 11) or x == 7 or y == 7:
            return WOOD_DARK
        return rgb("ffd86a") if y < 7 else rgb("f2b84a")  # warm light inside
    return house_wall(x, y, r)


def fence(x, y, r):
    if x in (2, 3, 12, 13) and 4 <= y <= 14:
        return WOOD
    if y in (6, 7, 11, 12):
        return WOOD_DARK if y in (7, 12) else WOOD
    return grass(x, y, r)


def forest_floor(x, y, r):
    return speckle(FOREST, FOREST_DARK, 0.2)(x, y, r)


def bush(x, y, r):
    dx, dy = x - 7.5, y - 8.5
    if dx * dx / 50 + dy * dy / 36 <= 1:
        if (x, y) in {(5, 6), (10, 8), (7, 11)}:
            return rgb("d84a5a")                         # berries
        return LEAF_LIGHT if dy < -2 else LEAF
    return grass(x, y, r)


def mine_floor(x, y, r):
    return speckle(MINE, MINE_DARK, 0.12)(x, y, r)


def planks(x, y, r):
    if y % 4 == 3:
        return WOOD_DARK
    return rgb("8a5e34") if r.random() > 0.1 else WOOD_DARK


def cobbles(x, y, r):
    cx, cy = x % 5, (y + (2 if (x // 5) % 2 else 0)) % 5
    if cx == 4 or cy == 4:
        return rgb("6a6a70")
    return rgb("a4a2a0") if (x * 7 + y * 3) % 5 else rgb("9a9894")


def rocks(x, y, r):
    for (cx, cy, rad) in ((5, 6, 4.2), (11, 10, 3.6), (4, 12, 2.6)):
        d = (x - cx) ** 2 + (y - cy) ** 2
        if d <= rad * rad:
            return rgb("7a7088") if d <= (rad - 1.5) ** 2 else rgb("4a4256")
    return mine_floor(x, y, r)


def mushrooms(x, y, r):
    caps = {(4, 9): 1, (11, 5): 1, (9, 12): 1}
    for (cx, cy) in caps:
        if cy - 1 <= y <= cy and cx - 1 <= x <= cx + 1:
            return rgb("e85a4a") if (x + y) % 3 else rgb("f6e8d8")
        if y == cy + 1 and x == cx:
            return rgb("f0e6d6")
    return forest_floor(x, y, r)


def well(x, y, r):
    dx, dy = x - 7.5, y - 7.5
    d = dx * dx + dy * dy
    if d <= 20:
        return rgb("1a2a4a")
    if d <= 50:
        return STONE if (x + y) % 3 else STONE_DARK
    return cobbles(x, y, r)


def rails(x, y, r):
    if x in (4, 11):
        return rgb("9a9aa8")
    if y % 5 == 2 and 2 <= x <= 13:
        return WOOD_DARK
    return mine_floor(x, y, r)


def crate(x, y, r):
    if x in (1, 14) or y in (1, 14) or x == y or x == 15 - y:
        return WOOD_DARK if 1 <= x <= 14 and 1 <= y <= 14 else mine_floor(x, y, r)
    if 1 <= x <= 14 and 1 <= y <= 14:
        return WOOD
    return mine_floor(x, y, r)


def reeds(x, y, r):
    if x % 3 == 1 and y >= 4 + (x * 5) % 4:
        return rgb("6aa048") if y > 6 else rgb("8cbc5c")
    return water(x, y, r)


PAINTERS = {
    "grass": grass, "flowers": flowers, "path": path, "water": water,
    "stone_wall": bricks(STONE, STONE_DARK), "tree": tree_on(grass), "house_wall": house_wall, "roof": roof,
    "door": door, "fence": fence, "forest_floor": forest_floor, "bush": bush,
    "mine_floor": mine_floor, "mine_wall": bricks(MINE_WALL, MINE_EDGE), "planks": planks, "window": window,
    "cobbles": cobbles, "forest_tree": tree_on(forest_floor), "rocks": rocks, "mushrooms": mushrooms,
    "well": well, "rails": rails, "crate": crate, "reeds": reeds,
}


def tiles():
    img = Image.new("RGBA", (T * 8, T * 3), (0, 0, 0, 0))
    for i, name in enumerate(TILES):
        paint_tile(img, i, PAINTERS[name])
    return img


# --- People ---------------------------------------------------------------------------------------

SKIN, EYE = rgb("f0c4a0"), rgb("282020")


def hero_frame(facing, step):
    """The hero, Pip. facing: "down", "up" or "left" ("right" mirrors left);
    step: 0 standing, 1 and 2 a foot forward."""
    hair, shirt, shade = rgb("5c3820"), rgb("3c8c9c"), rgb("2c6c7c")
    belt, legs, boots = rgb("50341e"), rgb("4a4a6a"), rgb("34241c")
    img = Image.new("RGBA", (16, 24), (0, 0, 0, 0))
    bob = 1 if step else 0

    def put(x, y, c):
        if 0 <= x < 16 and 0 <= y < 24:
            img.putpixel((x, y), c)

    for y in range(1, 16):
        for x in range(16):
            c = None
            if 1 <= y <= 3 and 4 <= x <= 11:
                c = hair
            elif 4 <= y <= 8 and 4 <= x <= 11:
                if facing == "down":
                    c = hair if x in (4, 11) and y <= 5 else SKIN
                    if y == 6 and x in (6, 9):
                        c = EYE
                elif facing == "up":
                    c = hair if y <= 7 else SKIN
                else:
                    c = hair if x >= 9 or (y <= 4 and x >= 6) else SKIN
                    if y == 6 and x == 5:
                        c = EYE
            elif 9 <= y <= 15:
                if facing in ("down", "up") and 3 <= x <= 12:
                    c = SKIN if x in (3, 12) and y >= 13 else shirt
                    if y == 15 and 4 <= x <= 11:
                        c = belt
                elif facing == "left" and 5 <= x <= 10:
                    c = belt if y == 15 else shirt
            if c:
                put(x, y + bob, c)
    if facing == "left":
        arm = {0: 0, 1: 1, 2: -1}[step]
        for y in range(10, 14):
            put(7 + arm, y + bob, shade)
        put(7 + arm, 14 + bob, SKIN)
    if facing in ("down", "up"):
        for leg, (x0, x1) in enumerate(((5, 6), (9, 10))):
            lift = 1 if step == leg + 1 else 0
            for y in range(16, 21 - lift):
                for x in (x0, x1):
                    put(x, y + (bob if y < 18 else 0), legs)
            for y in (21 - lift, 22 - lift):
                for x in range(x0 - 1, x1 + 1):
                    put(x, y, boots)
    else:
        if step == 0:
            spots = [(6, 9, 0)]
        else:
            spots = [(4, 6, 1), (9, 11, 0)] if step == 1 else [(4, 6, 0), (9, 11, 1)]
        for x0, x1, lift in spots:
            for y in range(16, 21 - lift):
                for x in range(x0, x1 + 1):
                    put(x, y, legs)
            for y in (21 - lift, 22 - lift):
                for x in range(x0 - 1, x1 + 1):
                    put(x, y, boots)
    return img


def hero_walk():
    sheet = Image.new("RGBA", (16 * 3, 24 * 4), (0, 0, 0, 0))
    for row, facing in enumerate(["down", "left", "right", "up"]):
        for step in range(3):
            frame = hero_frame("left" if facing == "right" else facing, step)
            if facing == "right":
                frame = frame.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            sheet.paste(frame, (step * 16, row * 24))
    return sheet


def person(hair, hair_shape, body, body_shape, extra=None):
    """A villager facing the viewer. hair_shape(x, y) and body_shape(x, y)
    say where hair and clothes are; extra(img) adds details."""
    img = Image.new("RGBA", (16, 24), (0, 0, 0, 0))
    for y in range(24):
        for x in range(16):
            c = None
            if 4 <= y <= 8 and 4 <= x <= 11:
                c = SKIN
                if y == 6 and x in (6, 9):
                    c = EYE
            if hair_shape(x, y):
                c = hair
            b = body_shape(x, y)
            if b:
                c = body if b is True else b
            if c:
                img.putpixel((x, y), c)
    if extra:
        extra(img)
    return img


def blink(img, eye_row=6, eyes=(6, 9)):
    closed = img.copy()
    for x in eyes:
        closed.putpixel((x, eye_row), rgb("c49274"))
    return closed


def two_frames(a, b, size=(16, 24)):
    sheet = Image.new("RGBA", (size[0] * 2, size[1]), (0, 0, 0, 0))
    sheet.paste(a, (0, 0))
    sheet.paste(b, (size[0], 0))
    return sheet


def maren():
    """Old Maren, keeper of the lantern: white hair and beard, a deep blue
    robe with gold trim, a staff."""
    robe, trim, staff = rgb("3a4c9c"), rgb("f0c45c"), rgb("7a5430")

    def hair(x, y):
        return (1 <= y <= 3 and 4 <= x <= 11) or (4 <= y <= 7 and x in (4, 11)) or (8 <= y <= 10 and 5 <= x <= 10)

    def body(x, y):
        if 11 <= y <= 22 and 3 <= x <= 12 - (1 if y < 14 else 0):
            return trim if x == 7 or y == 22 else True
        return None

    def extras(img):
        for y in range(2, 23):
            img.putpixel((14, y), staff)
        img.putpixel((14, 1), rgb("ffe08a"))
        for x in (13, 15):
            img.putpixel((x, 2), rgb("ffe08a"))

    img = person(rgb("e6e6ee"), hair, robe, body, extras)
    return two_frames(img, blink(img))


def wren():
    """Wren the baker: auburn hair in a bun, a white apron over green."""
    dress, apron = rgb("4c8a5c"), rgb("f4f0e6")

    def hair(x, y):
        return (0 <= y <= 1 and 6 <= x <= 9) or (2 <= y <= 3 and 4 <= x <= 11) or (4 <= y <= 6 and x in (4, 11))

    def body(x, y):
        if 9 <= y <= 21 and 3 <= x <= 12:
            if 5 <= x <= 10 and y >= 11:
                return apron
            return SKIN if x in (3, 12) and 13 <= y <= 15 else True
        if y == 22 and x in (5, 6, 9, 10):
            return rgb("4a3020")
        return None

    img = person(rgb("a8482c"), hair, dress, body)
    return two_frames(img, blink(img))


def nell():
    """Nell, a child of the village: pigtails and a yellow dress. Her second
    frame bounces on her toes."""
    dress = rgb("e8b83c")

    def hair(x, y):
        return (2 <= y <= 3 and 4 <= x <= 11) or (4 <= y <= 9 and x in (3, 12)) or (y == 4 and 4 <= x <= 11)

    def body(x, y):
        if 9 <= y <= 17 and 4 <= x <= 11:
            return True
        if 18 <= y <= 20 and x in (5, 6, 9, 10):
            return SKIN
        if y == 21 and x in (5, 6, 9, 10):
            return rgb("8a3a3a")
        return None

    img = person(rgb("6a3c1c"), hair, dress, body)
    hop = Image.new("RGBA", (16, 24), (0, 0, 0, 0))
    hop.paste(img.crop((0, 1, 16, 24)), (0, 0))
    return two_frames(img, hop)


def biscuit():
    """Biscuit, Wren's ginger cat, sitting, tail down then up."""
    fur, dark, belly = rgb("e0883a"), rgb("b0602a"), rgb("f6d6a8")

    def frame(tail_up):
        img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        for y in range(16):
            for x in range(16):
                c = None
                if 3 <= y <= 8 and 3 <= x <= 9:             # head
                    c = fur
                    if y == 5 and x in (5, 7):
                        c = rgb("2a4a2a")
                    if y == 7 and x == 6:
                        c = rgb("d06a6a")
                if y == 2 and x in (3, 4, 8, 9):            # ears
                    c = dark
                if 9 <= y <= 14 and 3 <= x <= 10:           # body
                    c = belly if 5 <= x <= 7 and y >= 10 else fur
                    if x in (3, 10) and y % 2 == 0:
                        c = dark
                if c:
                    img.putpixel((x, y), c)
        tail = [(11, 13), (12, 12), (13, 11), (13, 10)] if not tail_up else [(11, 12), (12, 11), (12, 10), (12, 9), (13, 8)]
        for (x, y) in tail:
            img.putpixel((x, y), dark)
        return img

    return two_frames(frame(False), frame(True), (16, 16))


def flame():
    """A lost flame: a small fire that flickers (four frames)."""
    sheet = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
    shapes = [(0.0, 0), (0.6, 1), (0.0, 0), (-0.6, -1)]
    for i, (lean, lift) in enumerate(shapes):
        for y in range(16):
            for x in range(16):
                t = (y - 2 + lift) / 13.0                   # 0 at the tip, 1 at the base
                if not 0 <= t <= 1:
                    continue
                centre = 7.5 + lean * (1 - t) * 2
                half = 1 + 5.5 * (t ** 0.7) * (1.0 if t < 0.85 else (1 - t) / 0.15 * 0.9 + 0.1)
                d = abs(x - centre)
                if d <= half:
                    inner = d <= half * 0.45 and t > 0.35
                    core = d <= half * 0.2 and t > 0.55
                    c = rgb("fff6c0") if core else rgb("ffd04a") if inner else rgb("f07a2a")
                    sheet.putpixel((i * 16 + x, y), c)
    return sheet


def lantern():
    """The great lantern on its stone post: dark, then lit (two flickers)."""
    sheet = Image.new("RGBA", (96, 64), (0, 0, 0, 0))
    iron, iron_light, glass_dark = rgb("2c2c34"), rgb("54545e"), rgb("3a4058")
    for f in range(3):
        img = Image.new("RGBA", (32, 64), (0, 0, 0, 0))
        lit = f > 0
        glow = [rgb("ffe9a0"), rgb("fff2c0")][f - 1] if lit else None
        for y in range(64):
            for x in range(32):
                c = None
                if 36 <= y <= 61 and 12 <= x <= 19:          # the stone post
                    c = STONE if (y // 4 + x) % 5 else STONE_DARK
                if 60 <= y <= 63 and 8 <= x <= 23:           # its base
                    c = STONE_DARK
                if 32 <= y <= 35 and 9 <= x <= 22:           # the lantern's foot
                    c = iron
                if 12 <= y <= 31 and 9 <= x <= 22:           # the glass box
                    frame_bar = x in (9, 22) or y in (12, 31) or x == 15 or x == 16
                    if frame_bar:
                        c = iron_light if x in (9, 22) else iron
                    else:
                        if lit:
                            dx, dy = x - 15.5, y - 23
                            c = glow if dx * dx + dy * dy < 14 else rgb("ffc84a") if dx * dx + dy * dy < 30 else rgb("f09a3a")
                        else:
                            c = glass_dark if (x + y) % 7 else rgb("4c5470")
                if 6 <= y <= 11 and 9 + (11 - y) <= x <= 22 - (11 - y):   # the roof
                    c = iron
                if y in (3, 4, 5) and 15 <= x <= 16:         # the ring on top
                    c = iron_light
                if c:
                    img.putpixel((x, y), c)
        sheet.paste(img, (f * 32, 0))
    return sheet


def shrine():
    """The wayshrine (where the game is saved): a standing stone with a
    glowing rune, the second frame brighter."""
    sheet = Image.new("RGBA", (32, 24), (0, 0, 0, 0))
    for f in range(2):
        rune = rgb("8ad8ff") if f == 0 else rgb("d8f4ff")
        for y in range(24):
            for x in range(16):
                c = None
                if 3 <= y <= 21 and 4 <= x <= 11 and not (y == 3 and x in (4, 11)):
                    c = STONE if (x + y // 3) % 4 else STONE_DARK
                if 20 <= y <= 23 and 2 <= x <= 13:
                    c = STONE_DARK
                if (x, y) in {(7, 7), (8, 7), (7, 8), (8, 8), (6, 10), (9, 10), (7, 11), (8, 11), (7, 13), (8, 13),
                              (7, 14), (8, 14), (7, 15), (8, 15)}:
                    c = rune
                if c:
                    sheet.putpixel((f * 16 + x, y), c)
    return sheet


def gate():
    """The old mine's iron gate, with a padlock."""
    img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    iron, rust = rgb("3c3c46"), rgb("8a5034")
    for y in range(32):
        for x in range(32):
            c = None
            if x in (0, 1, 30, 31):
                c = rgb("6a5a4a")
            elif x % 5 == 2 or y in (3, 4, 16, 17):
                c = iron if (x + y) % 9 else rust
            if 14 <= x <= 17 and 18 <= y <= 22:
                c = rgb("d8b040")                            # padlock
            if c:
                img.putpixel((x, y), c)
    return img


def sign():
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for y in range(16):
        for x in range(16):
            c = None
            if 2 <= y <= 8 and 1 <= x <= 14:
                c = WOOD_DARK if y in (2, 8) or x in (1, 14) else WOOD
                if y in (4, 6) and 3 <= x <= 12 and x % 3:
                    c = rgb("4a3020")                        # writing
            if 9 <= y <= 15 and 7 <= x <= 8:
                c = WOOD_DARK
            if c:
                img.putpixel((x, y), c)
    return img


def main():
    folder = Path(sys.argv[1])
    folder.mkdir(parents=True, exist_ok=True)
    art = {"tiles.png": tiles(), "hero_walk.png": hero_walk(), "maren.png": maren(), "wren.png": wren(),
           "nell.png": nell(), "biscuit.png": biscuit(), "flame.png": flame(), "lantern.png": lantern(),
           "shrine.png": shrine(), "gate.png": gate(), "sign.png": sign()}
    for name, img in art.items():
        img.save(folder / name)


if __name__ == "__main__":
    main()
