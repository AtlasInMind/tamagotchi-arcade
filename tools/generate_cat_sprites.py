"""Build the firmware cat frames from the approved pixel-art style sheet."""
from collections import deque
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "art_reference" / "cat_pixel_art_style_guide.png"
OUTPUT = ROOT / "tamagotchi_arcade" / "cat_sprites.h"

WIDTH, HEIGHT = 30, 40
BOXES = [
    (48, 82, 228, 322),   # neutral
    (242, 82, 422, 322),  # happy
    (434, 82, 614, 322),  # blink
    (625, 82, 805, 322),  # excited
    (814, 82, 994, 322),  # sad
]

# Compact 16-color palette sampled from the style guide. Index zero is
# transparent; the remaining entries are stored as RGB565 in the firmware.
RGB = [
    (0, 255, 0), (16, 25, 46), (39, 44, 67), (78, 75, 91),
    (119, 110, 112), (176, 153, 137), (117, 70, 54), (190, 105, 58),
    (224, 145, 76), (247, 188, 107), (242, 215, 170), (255, 239, 201),
    (244, 118, 102), (99, 51, 43), (20, 22, 36), (255, 255, 255),
]


def rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def frame_indices(source, box):
    frame = source.crop(box).resize((WIDTH, HEIGHT), Image.Resampling.NEAREST)
    pixels = list(frame.getdata())

    # Remove only the connected navy sheet background. This deliberately
    # preserves equally dark isolated eye, nose and outline pixels.
    outside, queue = set(), deque()
    queue.extend((x, y) for x in range(WIDTH) for y in (0, HEIGHT - 1))
    queue.extend((x, y) for y in range(HEIGHT) for x in (0, WIDTH - 1))
    while queue:
        x, y = queue.popleft()
        if (x, y) in outside:
            continue
        r, g, b = pixels[y * WIDTH + x]
        if (r - 16) ** 2 + (g - 25) ** 2 + (b - 46) ** 2 > 32 ** 2:
            continue
        outside.add((x, y))
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < WIDTH and 0 <= ny < HEIGHT:
                queue.append((nx, ny))

    result = []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if (x, y) in outside:
                result.append(0)
                continue
            r, g, b = pixels[y * WIDTH + x]
            nearest = min(
                range(1, len(RGB)),
                key=lambda i: (r - RGB[i][0]) ** 2 + (g - RGB[i][1]) ** 2 + (b - RGB[i][2]) ** 2,
            )
            result.append(nearest)
    return result


def main():
    source = Image.open(SOURCE).convert("RGB")
    frames = [frame_indices(source, box) for box in BOXES]
    lines = [
        "// Generated from art_reference/cat_pixel_art_style_guide.png.",
        "// Run tools/generate_cat_sprites.py after replacing the approved style sheet.",
        "#pragma once",
        "#include <Arduino.h>",
        f"#define CAT_SPRITE_W {WIDTH}",
        f"#define CAT_SPRITE_H {HEIGHT}",
        f"#define CAT_SPRITE_FRAME_COUNT {len(frames)}",
        "static const uint16_t CAT_SPRITE_PALETTE[16] PROGMEM = {",
        "  " + ", ".join(f"0x{rgb565(c):04X}" for c in RGB),
        "};",
        "static const uint8_t CAT_SPRITE_FRAMES[CAT_SPRITE_FRAME_COUNT][CAT_SPRITE_W * CAT_SPRITE_H] PROGMEM = {",
    ]
    for frame in frames:
        lines.append("  {")
        for pos in range(0, len(frame), WIDTH):
            lines.append("    " + ",".join(f"{v:2d}" for v in frame[pos:pos + WIDTH]) + ",")
        lines.append("  },")
    lines.extend(["};", ""])
    OUTPUT.write_text("\n".join(lines))
    print(f"Wrote {OUTPUT} ({OUTPUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
