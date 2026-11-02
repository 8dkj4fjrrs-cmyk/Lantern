"""Draws the sample game's pixel art (original art, MIT like the rest).

    python3 make_art.py <project folder>

Writes assets/tiles.png (8 tiles of 16 x 16 pixels in one row),
assets/hero.png and assets/elder.png (16 x 24 each). Run once; the results are committed.
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


def main():
    folder = Path(sys.argv[1]) / "assets"
    folder.mkdir(parents=True, exist_ok=True)
    tiles = Image.new("RGBA", (T * 8, T), (0, 0, 0, 0))
    for i, p in enumerate([grass, flowers, path, wall, water, cave_floor, cave_wall, doormat]):
        tile(tiles, i, p)
    tiles.save(folder / "tiles.png")
    hero().save(folder / "hero.png")
    elder().save(folder / "elder.png")


if __name__ == "__main__":
    main()
