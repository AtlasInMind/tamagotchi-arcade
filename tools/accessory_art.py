"""Hand-authored accessory art: explicit back/front layers on a shared canvas.

Each accessory is defined directly against the compiled cat sprite instead of
being cropped from a style-sheet montage (see docs/ART_PIPELINE.md). Both
layers share one 28x26 canvas and one anchor, so the generator, the firmware,
and the simulator all place them identically with no per-item runtime logic.

Coordinate model: for a chest-anchored item, canvas row `ay` lands on cat-grid
row `23 - anchor_row + ay` (chest anchor is cat row 23); for a face-anchored
item it's `17 - anchor_row + ay` (face/eye anchor is cat row 17). Canvas
column `ax` lands on cat-grid column `ax + 1 - dx` (chest/face anchor column
is 15, canvas half-width is 14). Designing with `anchor_row` chosen so
`ay == cat row` keeps the numbers in this file directly readable against the
cat silhouette.
"""

CANVAS_W = 28
CANVAS_H = 26

# Shared palette. '.' is reserved (never assign it) and always means
# transparent. Colors are deliberately reused from the existing cat/UI
# palette where possible instead of inventing new ones.
COLORS = {
    "R": 0xB9C7,  # crimson cloth (existing cape color)
    "r": 0x7104,  # crimson cloth, shaded
    "B": 0x04BF,  # royal blue cloth (existing cape color)
    "b": 0x0289,  # royal blue cloth, shaded
    "G": 0xF5ED,  # warm gold (existing pendant/clasp color)
    "g": 0xAD2A,  # gold, shaded
    "N": 0x2168,  # navy frame (existing glasses recolor target)
    "D": 0x10A4,  # near-black lens (never 0x0000, which is transparent)
    "O": 0xFD20,  # scarf orange (TFT_ORANGE)
    "o": 0xB180,  # scarf orange, shaded
    "L": 0x7226,  # leather brown (existing dark-fur brown)
    "l": 0x4143,  # leather, shaded
    "Y": 0xFFE0,  # medal ribbon yellow (TFT_YELLOW)
}


def _blank_canvas():
    return [[None] * CANVAS_W for _ in range(CANVAS_H)]


def stamp(canvas, ox, oy, rows):
    """Paint an ASCII grid (list of equal-length strings) at (ox, oy)."""
    width = len(rows[0])
    for row in rows:
        if len(row) != width:
            raise ValueError(f"uneven row width in stamp at ({ox},{oy}): {rows!r}")
    for dy, row in enumerate(rows):
        for dx, ch in enumerate(row):
            if ch == ".":
                continue
            if ch not in COLORS:
                raise ValueError(f"unknown glyph {ch!r} in stamp at ({ox},{oy})")
            x, y = ox + dx, oy + dy
            if not (0 <= x < CANVAS_W and 0 <= y < CANVAS_H):
                raise ValueError(f"stamp pixel ({x},{y}) outside {CANVAS_W}x{CANVAS_H} canvas")
            canvas[y][x] = COLORS[ch]


def rect(canvas, ox, oy, w, h, ch):
    stamp(canvas, ox, oy, [ch * w for _ in range(h)])


def layer(*parts):
    """Build one canvas (list[26][28] of int|None) from a sequence of
    (ox, oy, rows-or-rect-spec) stamp calls. `parts` is a list of callables
    taking the canvas, so callers just pass lambdas/partials; see usage
    below via the `Layer` builder instead for readability."""
    canvas = _blank_canvas()
    for part in parts:
        part(canvas)
    return canvas


def empty_layer():
    return _blank_canvas()


ACCESSORIES = [
    {
        "name": "Silk Bowtie",
        "anchor": "chest",
        # canvas row 20 -> cat row 3+ay, keeping content in the ay 21-25
        # band (cat rows 24-28: on the chest, clear of the head/jaw).
        "anchor_row": 20,
        "dx": 0,
        "back": None,
        "front": layer(
            lambda c: stamp(c, 10, 21, [
                ".RR...RR.",
                "RRR...RRR",
                "..R.r.R..",
                "RRR...RRR",
                ".RR...RR.",
            ]),
        ),
    },
    {
        "name": "Gold Pendant",
        "anchor": "chest",
        "anchor_row": 20,
        "dx": 0,
        "back": None,
        "front": layer(
            lambda c: stamp(c, 11, 20, [
                "...G...",
                "..GGG..",
                ".GGGGG.",
                "GGGgGGG",
                ".GGGGG.",
                "..ggg..",
            ]),
        ),
    },
    {
        "name": "Round Glasses",
        "anchor": "face",
        "anchor_row": 17,
        "dx": 0,
        "back": None,
        # Rims only - the lens interior is left transparent so the cat's
        # own pupils read through, instead of a solid disc hiding them.
        "front": layer(
            lambda c: stamp(c, 6, 15, [
                ".NNNN......NNNN.",
                "N....NNNNNN....N",
                "N....NN..NN....N",
                ".NNNN......NNNN.",
            ]),
        ),
    },
    {
        "name": "Knit Scarf",
        "anchor": "chest",
        "anchor_row": 23,
        "dx": 0,
        # Hanging tail sits behind the body, off to one side, well clear
        # of the front neck wrap so it never touches the face.
        "back": layer(
            lambda c: stamp(c, 19, 15, [
                "OOOOO",
                "OOOOO",
                "OOOOO",
                "OOOOO",
                ".OOO.",
                ".OOO.",
                ".OOO.",
                "..O..",
                "..O..",
                "..O..",
            ]),
        ),
        # Neck wrap sits just below the head, narrower than the face so it
        # never spills onto the cheeks.
        "front": layer(
            lambda c: stamp(c, 10, 22, [
                "OOOOOOOOO",
                "OOOOOOOOO",
                "OOOoooOOO",
            ]),
        ),
    },
    {
        "name": "Sunglasses",
        "anchor": "face",
        "anchor_row": 17,
        "dx": 0,
        "back": None,
        "front": layer(
            lambda c: stamp(c, 6, 15, [
                ".NNNN......NNNN.",
                "NDDDDNNNNNNDDDDN",
                "NDDDDNN..NNDDDDN",
                ".NNNN......NNNN.",
            ]),
        ),
    },
    {
        "name": "Leather Pack",
        "anchor": "chest",
        "anchor_row": 3,
        "dx": 0,
        # Bag rides fully behind the body, peeking out to one side.
        "back": layer(
            lambda c: stamp(c, 17, 4, [
                ".LLLLLLLL.",
                "LLLLLLLLLL",
                "LLllllllLL",
                "LLllllllLL",
                "LLllllllLL",
                "LLllllllLL",
                "LLllllllLL",
                "LLllllllLL",
                "LLLLLLLLLL",
                ".LLLLLLLL.",
            ]),
        ),
        # Two thick straps cross the chest in front, close together so they
        # read clearly at 1x scale instead of thinning out to single pixels.
        "front": layer(
            lambda c: stamp(c, 10, 4, [
                "LL...LL",
                "LL...LL",
                "LL...LL",
                "LL...LL",
                "LL...LL",
            ]),
        ),
    },
    {
        "name": "Hero Medal",
        "anchor": "chest",
        "anchor_row": 20,
        "dx": 0,
        "back": None,
        "front": layer(
            lambda c: stamp(c, 13, 21, [
                ".Y.",
                ".Y.",
            ]),
            lambda c: stamp(c, 11, 23, [
                ".GGGGG.",
                "GYYYYYG",
                ".GGGGG.",
            ]),
        ),
    },
    {
        "name": "Crimson Cape",
        "anchor": "chest",
        "anchor_row": 3,
        "dx": 0,
        # Broad cloth behind the body, wide enough to show past the
        # silhouette on both sides.
        "back": layer(
            lambda c: rect(c, 3, 1, 22, 15, "R"),
            lambda c: rect(c, 3, 1, 22, 1, "r"),
            lambda c: rect(c, 3, 15, 22, 1, "r"),
        ),
        # Only a small collar and clasp show in front, at the neck.
        "front": layer(
            lambda c: stamp(c, 11, 0, [
                ".RRRRR.",
                "RRGGGRR",
            ]),
        ),
    },
    {
        "name": "Royal Cape",
        "anchor": "chest",
        "anchor_row": 3,
        "dx": 0,
        "back": layer(
            lambda c: rect(c, 2, 1, 24, 17, "B"),
            lambda c: rect(c, 2, 1, 24, 1, "G"),
            lambda c: rect(c, 2, 17, 24, 1, "G"),
            lambda c: rect(c, 2, 2, 1, 15, "b"),
            lambda c: rect(c, 25, 2, 1, 15, "b"),
        ),
        "front": layer(
            lambda c: stamp(c, 10, 0, [
                ".GBBBBBG.",
                "GGBGGGBGG",
            ]),
        ),
    },
]

assert len(ACCESSORIES) == 9, "ACCESSORIES must define exactly 9 entries"
for _item in ACCESSORIES:
    for _key in ("back", "front"):
        _grid = _item[_key]
        if _grid is None:
            continue
        if len(_grid) != CANVAS_H or any(len(row) != CANVAS_W for row in _grid):
            raise ValueError(f"{_item['name']} {_key} layer is not {CANVAS_W}x{CANVAS_H}")
