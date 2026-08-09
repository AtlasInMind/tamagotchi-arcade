"""Generate compact RGB565 firmware atlases from the approved premium art sheets."""
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import accessory_art  # noqa: E402

COSMETICS = ROOT / "art_reference" / "cosmetics_style_guide.png"
STRAW_HAT = ROOT / "art_reference" / "straw_hat_redesign.png"
BEANIE = ROOT / "art_reference" / "beanie_redesign.png"
WORLD = ROOT / "art_reference" / "world_ui_style_guide.png"
OUTPUT = ROOT / "tamagotchi_arcade" / "premium_assets.h"

HAT_BOXES = [
    (164, 152, 276, 212), (404, 142, 493, 214), (606, 152, 702, 211),
    (831, 156, 906, 207), (0, 448, 79, 520), (232, 447, 305, 516),
    (444, 465, 521, 512), (643, 457, 721, 508), (643, 457, 721, 508),
]
HAT_CONTENT_SIZES = [
    (24, 12), (24, 19), (28, 15), (22, 10), (24, 20),
    (20, 19), (20, 8), (22, 12), (24, 14),
]
BG_BOXES = [
    (18, 45, 196, 328), (212, 45, 390, 328), (406, 45, 584, 328),
    (603, 45, 782, 328), (807, 45, 986, 328), (18, 354, 196, 628),
    (212, 354, 390, 628), (406, 354, 584, 628), (603, 354, 782, 628),
    (807, 354, 986, 628),
]
SLOT_BOXES = [
    (91, 812, 205, 918), (238, 808, 345, 916), (383, 805, 484, 916),
    (523, 800, 626, 915), (663, 801, 768, 914), (798, 800, 916, 916),
]
ITEM_BOXES = [
    (88, 1158, 194, 1245), (219, 1155, 322, 1244), (328, 1150, 432, 1248),
    (447, 1145, 538, 1248), (555, 1148, 658, 1247), (681, 1143, 777, 1250),
    (792, 1137, 930, 1254),
]


def rgb565(pixel):
    r, g, b = pixel[:3]
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def chroma_transparent(pixel):
    r, g, b = pixel[:3]
    return g > 120 and g > r * 1.75 and g > b * 1.75


def fitted(source, box, size, transparent=False, content_size=None):
    crop = source.crop(box).convert("RGB")
    crop.thumbnail(content_size or size, Image.Resampling.NEAREST)
    canvas = Image.new("RGB", size, (0, 255, 0) if transparent else (16, 25, 46))
    x = (size[0] - crop.width) // 2
    y = size[1] - crop.height
    canvas.paste(crop, (x, y))
    values = []
    for pixel in canvas.getdata():
        values.append(0 if transparent and chroma_transparent(pixel) else rgb565(pixel))
    return values


def fixed(source, box, size):
    image = source.crop(box).convert("RGB").resize(size, Image.Resampling.LANCZOS)
    # Quantize before RGB565 conversion for deliberate handheld-style ramps.
    image = image.quantize(colors=48, method=Image.Quantize.MEDIANCUT).convert("RGB")
    return [rgb565(p) for p in image.getdata()]


def remove_green_fringe(values):
    cleaned = []
    for value in values:
        if value == 0:
            cleaned.append(0)
            continue
        r = ((value >> 11) & 31) * 255 // 31
        g = ((value >> 5) & 63) * 255 // 63
        b = (value & 31) * 255 // 31
        # The source sheet uses several antialiased shades of chroma green.
        # Remove the darker edge shades too, without touching cyan/blue cloth.
        cleaned.append(0 if g > 50 and g > r * 1.15 and g > b * 1.15 else value)
    return cleaned


def recolor_green_fringe(values, replacement):
    cleaned = []
    for value in values:
        if value == 0:
            cleaned.append(0)
            continue
        r = ((value >> 11) & 31) * 255 // 31
        g = ((value >> 5) & 63) * 255 // 63
        b = (value & 31) * 255 // 31
        cleaned.append(replacement if g > 50 and g > r * 1.15 and g > b * 1.15 else value)
    return cleaned


def emit_array(lines, ctype, name, frames, width, height):
    lines.append(f"static const {ctype} {name}[{len(frames)}][{width * height}] PROGMEM = {{")
    for frame in frames:
        lines.append("  {")
        for pos in range(0, len(frame), width):
            row = frame[pos:pos + width]
            lines.append("    " + ",".join(f"0x{v:04X}" for v in row) + ",")
        lines.append("  },")
    lines.append("};")


def emit_scalar_array(lines, ctype, name, values):
    body = ",".join(str(v) for v in values)
    lines.append(f"static const {ctype} {name}[{len(values)}] PROGMEM = {{{body}}};")


def build_accessory_layers():
    """Flatten tools/accessory_art.py into (back_frames, front_frames, metadata).

    Each accessory's back/front canvas is a 26x28 grid of int|None; a missing
    layer becomes an all-transparent (all-zero) frame so PREMIUM_ACC_BACK and
    PREMIUM_ACC_FRONT always have exactly 9 entries and can be indexed
    identically to PREMIUM_ACC_ANCHOR_ROW/FACE/DX.
    """
    w, h = accessory_art.CANVAS_W, accessory_art.CANVAS_H
    if len(accessory_art.ACCESSORIES) != 9:
        raise ValueError("expected exactly 9 accessories in accessory_art.py")

    def flatten(grid):
        if grid is None:
            return [0] * (w * h)
        flat = []
        for row in grid:
            if len(row) != w:
                raise ValueError(f"accessory layer row has width {len(row)}, expected {w}")
            flat.extend(0 if v is None else v for v in row)
        return flat

    back_frames, front_frames = [], []
    anchor_face, anchor_row, dx = [], [], []
    for item in accessory_art.ACCESSORIES:
        if item["anchor"] not in ("chest", "face"):
            raise ValueError(f"{item['name']}: anchor must be 'chest' or 'face'")
        back_frames.append(flatten(item["back"]))
        front_frames.append(flatten(item["front"]))
        anchor_face.append(1 if item["anchor"] == "face" else 0)
        anchor_row.append(item["anchor_row"])
        dx.append(item["dx"])
    return back_frames, front_frames, anchor_face, anchor_row, dx


def main():
    cosmetics = Image.open(COSMETICS)
    straw_hat = Image.open(STRAW_HAT)
    beanie = Image.open(BEANIE)
    world = Image.open(WORLD)
    hats = [fitted(cosmetics, b, (28, 22), True, s) for b, s in zip(HAT_BOXES, HAT_CONTENT_SIZES)]
    hats[1] = fitted(beanie, (219, 271, 694, 644), (28, 22), True, HAT_CONTENT_SIZES[1])
    hats[2] = fitted(straw_hat, (190, 324, 790, 558), (28, 22), True, HAT_CONTENT_SIZES[2])
    hats = [hat if i == 3 else remove_green_fringe(hat) for i, hat in enumerate(hats)]
    acc_back, acc_front, acc_face, acc_row, acc_dx = build_accessory_layers()
    backgrounds = [fixed(world, b, (45, 80)) for b in BG_BOXES]
    slots = [fitted(world, b, (14, 12), False) for b in SLOT_BOXES]
    items = [fitted(world, b, (16, 16), False) for b in ITEM_BOXES]

    acc_w, acc_h = accessory_art.CANVAS_W, accessory_art.CANVAS_H
    lines = [
        "// Generated by tools/generate_premium_assets.py from approved style guides",
        "// and tools/accessory_art.py.",
        "#pragma once", "#include <Arduino.h>",
        "#define PREMIUM_HAT_W 28", "#define PREMIUM_HAT_H 22",
        f"#define PREMIUM_ACC_W {acc_w}", f"#define PREMIUM_ACC_H {acc_h}",
        "#define PREMIUM_BG_W 45", "#define PREMIUM_BG_H 80",
        "#define PREMIUM_SLOT_W 14", "#define PREMIUM_SLOT_H 12",
        "#define PREMIUM_ITEM_W 16", "#define PREMIUM_ITEM_H 16",
    ]
    emit_array(lines, "uint16_t", "PREMIUM_HATS", hats, 28, 22)
    emit_array(lines, "uint16_t", "PREMIUM_ACC_BACK", acc_back, acc_w, acc_h)
    emit_array(lines, "uint16_t", "PREMIUM_ACC_FRONT", acc_front, acc_w, acc_h)
    emit_scalar_array(lines, "uint8_t", "PREMIUM_ACC_ANCHOR_FACE", acc_face)
    emit_scalar_array(lines, "uint8_t", "PREMIUM_ACC_ANCHOR_ROW", acc_row)
    emit_scalar_array(lines, "int8_t", "PREMIUM_ACC_DX", acc_dx)
    emit_array(lines, "uint16_t", "PREMIUM_BACKGROUNDS", backgrounds, 45, 80)
    emit_array(lines, "uint16_t", "PREMIUM_SLOT_SYMBOLS", slots, 14, 12)
    emit_array(lines, "uint16_t", "PREMIUM_ITEM_ICONS", items, 16, 16)
    OUTPUT.write_text("\n".join(lines) + "\n")
    print(f"Wrote {OUTPUT} ({OUTPUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
