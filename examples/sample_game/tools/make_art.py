"""Draws the sample game's pixel art (original art, MIT like the rest).

    python3 make_art.py <project folder>

Writes assets/tiles.png (8 tiles of 16 x 16 pixels in one row),
assets/hero.png and assets/elder.png (16 x 24 each), and two sprite sheets
of 16 x 24 frames: assets/hero_walk.png (rows: down, left, right, up;
columns: standing, left foot forward, right foot forward) and
assets/elder_idle.png (eyes open, eyes closed). Run once; the results are committed.
"""
import random
import sys
from pathlib import Path

from PIL import Image

T = 16


def tile(img, index, painter):
    ox = index * T
    rnd = random.Random(index * 7919)
    for y in range(T):
        for x in range(T):
            img.putpixel((ox + x, y), painter(x, y, rnd))


def grass(x, y, r):
    return (76, 140, 64, 255) if r.random() > 0.12 else (60, 118, 52, 255)


def flowers(x, y, r):
    if (x, y) in {(3, 4), (11, 9), (6, 12), (13, 2)}:
        return (247, 214, 92, 255)
    if (x, y) in {(4, 4), (12, 9), (7, 12), (14, 2), (3, 3), (11, 8)}:
        return (236, 236, 228, 255)
    return grass(x, y, r)


def path(x, y, r):
    return (176, 140, 92, 255) if r.random() > 0.15 else (156, 122, 78, 255)


def wall(x, y, r):
    row = y // 4
    edge = y % 4 == 3 or (x + (4 if row % 2 else 0)) % 8 == 7
    return (88, 88, 98, 255) if edge else (132, 132, 144, 255)


def water(x, y, r):
    wave = (x + y * 2) % 9 in (0, 1) and y % 4 == 1
    return (170, 214, 240, 255) if wave else (52, 112, 196, 255)


def cave_floor(x, y, r):
    return (58, 52, 66, 255) if r.random() > 0.1 else (48, 43, 56, 255)


def cave_wall(x, y, r):
    row = y // 4
    edge = y % 4 == 3 or (x + (4 if row % 2 else 0)) % 8 == 7
    return (34, 30, 40, 255) if edge else (78, 68, 92, 255)


def doormat(x, y, r):
    if x in (0, 15) or y in (0, 15):
        return (120, 64, 40, 255)
    return (196, 112, 64, 255) if (x + y) % 4 < 2 else (176, 96, 52, 255)


def hero():
    img = Image.new("RGBA", (16, 24), (0, 0, 0, 0))
    hair, skin, eye = (92, 56, 32, 255), (240, 196, 160, 255), (40, 32, 32, 255)
    shirt, belt, legs, boots = (212, 84, 64, 255), (80, 52, 32, 255), (64, 80, 140, 255), (52, 36, 28, 255)
    for y in range(24):
        for x in range(16):
            c = None
            if 1 <= y <= 3 and 4 <= x <= 11:
                c = hair
            elif 4 <= y <= 8 and 4 <= x <= 11:
                c = hair if x in (4, 11) and y <= 5 else skin
                if y == 6 and x in (6, 9):
                    c = eye
            elif 9 <= y <= 15 and 3 <= x <= 12:
                c = skin if x in (3, 12) and y >= 13 else shirt
                if y == 15 and 4 <= x <= 11:
                    c = belt
            elif 16 <= y <= 20 and 5 <= x <= 10 and x not in (7, 8):
                c = legs
            elif 21 <= y <= 22 and 4 <= x <= 11 and x not in (7, 8):
                c = boots
            if c:
                img.putpixel((x, y), c)
    return img


def elder():
    """The village elder: grey hair and beard, a purple robe, a staff."""
    img = Image.new("RGBA", (16, 24), (0, 0, 0, 0))
    hair, skin, eye = (216, 216, 224, 255), (232, 190, 156, 255), (40, 32, 32, 255)
    robe, trim, staff = (112, 72, 160, 255), (240, 196, 92, 255), (120, 84, 48, 255)
    for y in range(24):
        for x in range(16):
            c = None
            if 1 <= y <= 3 and 4 <= x <= 11:
                c = hair
            elif 4 <= y <= 7 and 4 <= x <= 11:
                c = hair if x in (4, 11) else skin
                if y == 5 and x in (6, 9):
                    c = eye
            elif 8 <= y <= 10 and 5 <= x <= 10:
                c = hair                                     # the beard
            elif 11 <= y <= 22 and 3 <= x <= 12 - (1 if y < 14 else 0):
                c = trim if x == 7 or y == 22 else robe
            if x == 14 and 2 <= y <= 22:
                c = staff
            if c:
                img.putpixel((x, y), c)
    return img


HAIR, SKIN, EYE = (92, 56, 32, 255), (240, 196, 160, 255), (40, 32, 32, 255)
SHIRT, BELT, LEGS, BOOTS = (212, 84, 64, 255), (80, 52, 32, 255), (64, 80, 140, 255), (52, 36, 28, 255)
SHIRT_SHADE = (176, 66, 52, 255)


def hero_frame(facing, step):
    """One walking frame. facing: "down", "up" or "left" ("right" is the
    left frame mirrored); step: 0 standing, 1 and 2 a foot forward."""
    img = Image.new("RGBA", (16, 24), (0, 0, 0, 0))
    bob = 1 if step else 0          # the body dips a pixel mid-stride

    def put(x, y, c):
        if 0 <= x < 16 and 0 <= y < 24:
            img.putpixel((x, y), c)

    # Head (rows 1-8) and body (rows 9-15) move down with the bob.
    for y in range(1, 16):
        for x in range(16):
            c = None
            if 1 <= y <= 3 and 4 <= x <= 11:
                c = HAIR
            elif 4 <= y <= 8 and 4 <= x <= 11:
                if facing == "down":
                    c = HAIR if x in (4, 11) and y <= 5 else SKIN
                    if y == 6 and x in (6, 9):
                        c = EYE
                elif facing == "up":
                    c = HAIR if y <= 7 else SKIN
                else:  # left: the face on the left, hair at the back
                    c = HAIR if x >= 9 or (y <= 4 and x >= 6) else SKIN
                    if y == 6 and x == 5:
                        c = EYE
            elif 9 <= y <= 15:
                if facing in ("down", "up") and 3 <= x <= 12:
                    c = SKIN if x in (3, 12) and y >= 13 else SHIRT
                    if facing == "up" and x in (3, 12) and y >= 13:
                        c = SKIN
                    if y == 15 and 4 <= x <= 11:
                        c = BELT
                elif facing == "left" and 5 <= x <= 10:
                    c = BELT if y == 15 else SHIRT
            if c:
                put(x, y + bob, c)
    if facing == "left":
        # The arm swings opposite to the forward foot.
        arm = {0: 0, 1: 1, 2: -1}[step]
        for y in range(10, 14):
            put(7 + arm, y + bob, SHIRT_SHADE)
        put(7 + arm, 14 + bob, SKIN)

    # Legs (rows 16-20) and boots (21-22): one leg lifted per step.
    if facing in ("down", "up"):
        for leg, (x0, x1) in enumerate(((5, 6), (9, 10))):
            lift = 1 if step == leg + 1 else 0
            for y in range(16, 21 - lift):
                for x in (x0, x1):
                    put(x, y + (bob if y < 18 else 0), LEGS)
            for y in (21 - lift, 22 - lift):
                for x in range(x0 - 1, x1 + 1):
                    put(x, y, BOOTS)
    else:
        if step == 0:
            spots = [(6, 9, 0)]                      # legs together
        else:
            spots = [(4, 6, 1), (9, 11, 0)] if step == 1 else [(4, 6, 0), (9, 11, 1)]
        for x0, x1, lift in spots:
            for y in range(16, 21 - lift):
                for x in range(x0, x1 + 1):
                    put(x, y, LEGS)
            for y in (21 - lift, 22 - lift):
                for x in range(x0 - 1, x1 + 1):
                    put(x, y, BOOTS)
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


def elder_idle():
    """Two frames: the elder as drawn, and with eyes closed (a blink)."""
    sheet = Image.new("RGBA", (32, 24), (0, 0, 0, 0))
    open_eyes = elder()
    closed = open_eyes.copy()
    for x in (6, 9):
        closed.putpixel((x, 5), (176, 132, 104, 255))
    sheet.paste(open_eyes, (0, 0))
    sheet.paste(closed, (16, 0))
    return sheet


def main():
    folder = Path(sys.argv[1]) / "assets"
    folder.mkdir(parents=True, exist_ok=True)
    tiles = Image.new("RGBA", (T * 8, T), (0, 0, 0, 0))
    for i, p in enumerate([grass, flowers, path, wall, water, cave_floor, cave_wall, doormat]):
        tile(tiles, i, p)
    tiles.save(folder / "tiles.png")
    hero().save(folder / "hero.png")
    elder().save(folder / "elder.png")
    hero_walk().save(folder / "hero_walk.png")
    elder_idle().save(folder / "elder_idle.png")


if __name__ == "__main__":
    main()
