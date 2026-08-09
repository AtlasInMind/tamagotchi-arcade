"""Render the exact compiled cat/hat arrays without an ESP32.

This deliberately parses the generated C headers, so the preview catches
cropping, RGB565 conversion, transparency and placement errors that are not
visible in the original style-guide PNGs.
"""
from pathlib import Path
import re
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
CAT_HEADER = ROOT / "tamagotchi_arcade" / "cat_sprites.h"
ASSET_HEADER = ROOT / "tamagotchi_arcade" / "premium_assets.h"
OUTPUT = ROOT / "art_reference" / "hat_mask_v3_simulation.png"

CAT_W, CAT_H = 30, 40
HAT_W, HAT_H = 28, 22
CELL_W, CELL_H = 48, 58
UPSCALE = 5
HAT_NAMES = [
    "NO HAT", "CAP", "BEANIE", "STRAW", "FLOWERS",
    "WIZARD", "TOP HAT", "HALO", "CROWN", "ROYAL",
]
Y_SHIFT = [0, 0, 0, 0, 0, 0, 0, -14, 0, -2]
EAR_FRONT_MAX_Y = [0, 0, 0, 0, 0, 0, 6, 0, 0, 0]
HIDE_EARS = {1, 2, 3, 5, 8, 9}


def array_body(text, name):
    start = text.index(name)
    start = text.index("=", start) + 1
    end = text.index(";", start)
    return text[start:end]


def rgb565_to_rgb(value):
    r = (value >> 11) & 31
    g = (value >> 5) & 63
    b = value & 31
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31, 255)


def read_assets():
    cat_text = CAT_HEADER.read_text()
    asset_text = ASSET_HEADER.read_text()
    palette = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)", array_body(cat_text, "CAT_SPRITE_PALETTE"))]
    frames = [int(v) for v in re.findall(r"\b\d+\b", array_body(cat_text, "CAT_SPRITE_FRAMES"))]
    hats = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)", array_body(asset_text, "PREMIUM_HATS"))]
    assert len(palette) == 16
    assert len(frames) == 5 * CAT_W * CAT_H
    assert len(hats) == 9 * HAT_W * HAT_H
    return palette, frames, hats


def draw_frame(palette, frames, hats, hat_index):
    image = Image.new("RGBA", (CELL_W, CELL_H), rgb565_to_rgb(0x10C5))
    cat_x, cat_y = 9, 15
    frame = frames[:CAT_W * CAT_H]  # neutral

    def cat_pixel(x, y):
        index = frame[y * CAT_W + x]
        return None if index == 0 else rgb565_to_rgb(palette[index])

    def is_ear_pixel(x, y):
        return 1 <= y <= 10 and (3 <= x <= 10 or 20 <= x <= 26)

    for y in range(CAT_H):
        for x in range(CAT_W):
            color = cat_pixel(x, y)
            if color and not (hat_index in HIDE_EARS and is_ear_pixel(x, y)):
                image.putpixel((cat_x + x, cat_y + y), color)

    if hat_index:
        hat_x = cat_x + 15 - HAT_W // 2
        hat_y = cat_y + 2 - 10 + Y_SHIFT[hat_index]
        offset = (hat_index - 1) * HAT_W * HAT_H
        for y in range(HAT_H):
            for x in range(HAT_W):
                value = hats[offset + y * HAT_W + x]
                if value:
                    px, py = hat_x + x, hat_y + y
                    if 0 <= px < CELL_W and 0 <= py < CELL_H:
                        image.putpixel((px, py), rgb565_to_rgb(value))

        # Current firmware occlusion pass.
        max_y = EAR_FRONT_MAX_Y[hat_index]
        for y in range(1, max_y + 1):
            for x in range(3, 27):
                if not is_ear_pixel(x, y):
                    continue
                color = cat_pixel(x, y)
                if color:
                    image.putpixel((cat_x + x, cat_y + y), color)
    return image


def main():
    palette, frames, hats = read_assets()
    label_h = 16
    sheet = Image.new("RGBA", (CELL_W * 5 * UPSCALE, (CELL_H * 2) * UPSCALE + label_h * 2), (16, 25, 46, 255))
    draw = ImageDraw.Draw(sheet)
    for i, name in enumerate(HAT_NAMES):
        col, row = i % 5, i // 5
        frame = draw_frame(palette, frames, hats, i).resize((CELL_W * UPSCALE, CELL_H * UPSCALE), Image.Resampling.NEAREST)
        x = col * CELL_W * UPSCALE
        y = row * (CELL_H * UPSCALE + label_h)
        sheet.alpha_composite(frame, (x, y))
        bbox = draw.textbbox((0, 0), name)
        tw = bbox[2] - bbox[0]
        draw.text((x + (CELL_W * UPSCALE - tw) // 2, y + CELL_H * UPSCALE + 2), name, fill=(255, 239, 201, 255))
    sheet.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    main()
