"""Render exact compiled accessory arrays on the compiled cat sprite.

Placement is read entirely from the generated header (PREMIUM_ACC_BACK/FRONT
plus the PREMIUM_ACC_ANCHOR_* metadata) - this file duplicates no per-item
offsets, row ranges, or layer rules. If a placement or layer looks wrong
here, the fix belongs in tools/accessory_art.py, not in this script.
"""
from pathlib import Path
import re
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
CAT_HEADER = ROOT / "tamagotchi_arcade" / "cat_sprites.h"
ASSET_HEADER = ROOT / "tamagotchi_arcade" / "premium_assets.h"
OUTPUT = ROOT / "art_reference" / "accessory_v2_simulation.png"
CAT_W, CAT_H = 30, 40
ACC_W, ACC_H = 28, 26
CELL_W, CELL_H, UPSCALE = 48, 58, 5
NAMES = ["NONE", "BOWTIE", "PENDANT", "GLASSES", "SCARF",
         "SHADES", "BACKPACK", "MEDAL", "CAPE", "ROYAL CAPE"]


def body(text, name):
    start = text.index(name)
    start = text.index("=", start) + 1
    return text[start:text.index(";", start)]


def rgba565(value):
    r, g, b = (value >> 11) & 31, (value >> 5) & 63, value & 31
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31, 255)


def read_arrays():
    cat = CAT_HEADER.read_text()
    assets = ASSET_HEADER.read_text()
    palette = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)", body(cat, "CAT_SPRITE_PALETTE"))]
    frames = [int(v) for v in re.findall(r"\b\d+\b", body(cat, "CAT_SPRITE_FRAMES"))]
    back = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)", body(assets, "PREMIUM_ACC_BACK"))]
    front = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)", body(assets, "PREMIUM_ACC_FRONT"))]
    anchor_face = [int(v) for v in re.findall(r"\d+", body(assets, "PREMIUM_ACC_ANCHOR_FACE"))]
    anchor_row = [int(v) for v in re.findall(r"\d+", body(assets, "PREMIUM_ACC_ANCHOR_ROW"))]
    dx = [int(v) for v in re.findall(r"-?\d+", body(assets, "PREMIUM_ACC_DX"))]
    n = len(anchor_face)
    assert len(back) == n * ACC_W * ACC_H
    assert len(front) == n * ACC_W * ACC_H
    assert len(anchor_row) == n and len(dx) == n
    return palette, frames[:CAT_W * CAT_H], back, front, anchor_face, anchor_row, dx


def render(palette, cat, back, front, anchor_face, anchor_row, dx, index):
    out = Image.new("RGBA", (CELL_W, CELL_H), rgba565(0x10C5))
    cat_x, cat_y = 9, 15
    chest_x, chest_y = cat_x + 15, cat_y + 23
    face_x, face_y = cat_x + 15, cat_y + 17

    def draw_cat():
        for y in range(CAT_H):
            for x in range(CAT_W):
                p = cat[y * CAT_W + x]
                if p:
                    out.putpixel((cat_x + x, cat_y + y), rgba565(palette[p]))

    def draw_acc(frames, front_layer):
        if not index:
            return
        i = index - 1
        face_item = anchor_face[i]
        anchor_x = face_x if face_item else chest_x
        anchor_y = face_y if face_item else chest_y
        x0 = anchor_x - ACC_W // 2 + dx[i]
        y0 = anchor_y - anchor_row[i]
        offset = i * ACC_W * ACC_H
        for py in range(ACC_H):
            for px in range(ACC_W):
                value = frames[offset + py * ACC_W + px]
                if value:
                    ox, oy = x0 + px, y0 + py
                    if 0 <= ox < CELL_W and 0 <= oy < CELL_H:
                        out.putpixel((ox, oy), rgba565(value))

    draw_acc(back, False)
    draw_cat()
    draw_acc(front, True)
    return out


def main():
    palette, cat, back, front, anchor_face, anchor_row, dx = read_arrays()
    label_h = 16
    sheet = Image.new("RGBA", (CELL_W * 5 * UPSCALE, CELL_H * 2 * UPSCALE + label_h * 2), rgba565(0x10C5))
    draw = ImageDraw.Draw(sheet)
    for i, name in enumerate(NAMES):
        col, row = i % 5, i // 5
        image = render(palette, cat, back, front, anchor_face, anchor_row, dx, i).resize(
            (CELL_W * UPSCALE, CELL_H * UPSCALE), Image.Resampling.NEAREST)
        x = col * CELL_W * UPSCALE
        y = row * (CELL_H * UPSCALE + label_h)
        sheet.alpha_composite(image, (x, y))
        bbox = draw.textbbox((0, 0), name)
        draw.text((x + (CELL_W * UPSCALE - (bbox[2] - bbox[0])) // 2, y + CELL_H * UPSCALE + 2), name, fill=(255, 239, 201, 255))
    sheet.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    main()
