"""Render exact compiled accessory arrays on the compiled cat sprite."""
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
    accessories = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)", body(assets, "PREMIUM_ACCESSORIES"))]
    assert len(accessories) == 9 * ACC_W * ACC_H
    return palette, frames[:CAT_W * CAT_H], accessories


def render(palette, cat, accessories, index):
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

    def draw_acc(front_layer):
        if not index:
            return
        face_item = index in (3, 5)
        x = (face_x if face_item else chest_x) - ACC_W // 2
        if index == 1: y = chest_y - 20
        elif index == 2: y = chest_y - 14
        elif index == 3: y = face_y - 21
        elif index == 4: y = chest_y - 14
        elif index == 5: y = face_y - 22
        elif index == 6: x += 10; y = chest_y - 18
        elif index == 7: y = chest_y - 12
        elif index == 8: y = chest_y - 8
        else: y = chest_y - 7
        if not front_layer and index == 4:
            x += 4
        first_row, last_row = 0, ACC_H
        if not front_layer:
            if index not in (4, 6, 8, 9):
                return
        else:
            if index == 6:
                return
            if index == 4:
                first_row, last_row = 12, 18
            elif index in (8, 9):
                cloth = 0xB9C7 if index == 8 else 0x04BF
                for py in range(2):
                    for px in range(5):
                        out.putpixel((chest_x - 2 + px, chest_y - 2 + py), rgba565(cloth))
                out.putpixel((chest_x, chest_y - 2), rgba565(0xF5ED))
                return

        offset = (index - 1) * ACC_W * ACC_H
        for py in range(first_row, last_row):
            for px in range(ACC_W):
                value = accessories[offset + py * ACC_W + px]
                if value:
                    ox, oy = x + px, y + py
                    if 0 <= ox < CELL_W and 0 <= oy < CELL_H:
                        out.putpixel((ox, oy), rgba565(value))

    draw_acc(False)
    draw_cat()
    draw_acc(True)
    return out


def main():
    palette, cat, accessories = read_arrays()
    label_h = 16
    sheet = Image.new("RGBA", (CELL_W * 5 * UPSCALE, CELL_H * 2 * UPSCALE + label_h * 2), rgba565(0x10C5))
    draw = ImageDraw.Draw(sheet)
    for i, name in enumerate(NAMES):
        col, row = i % 5, i // 5
        image = render(palette, cat, accessories, i).resize((CELL_W * UPSCALE, CELL_H * UPSCALE), Image.Resampling.NEAREST)
        x = col * CELL_W * UPSCALE
        y = row * (CELL_H * UPSCALE + label_h)
        sheet.alpha_composite(image, (x, y))
        bbox = draw.textbbox((0, 0), name)
        draw.text((x + (CELL_W * UPSCALE - (bbox[2] - bbox[0])) // 2, y + CELL_H * UPSCALE + 2), name, fill=(255, 239, 201, 255))
    sheet.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    main()
