# Art pipeline and workflow

## What's been done

The graphics were first developed as large style sheets inspired by warm
16-bit RPGs. The cat was then reduced to a single fixed silhouette so all
cosmetic elements could be designed against the same head and body.

The cat's five expressions come from
`art_reference/cat_pixel_art_style_guide.png`. The generator
`tools/generate_cat_sprites.py` turns each crop into a 30×40-pixel sprite,
removes the connected background, maps colors onto a fixed palette, and
writes RGB565-compatible data to `tamagotchi_arcade/cat_sprites.h`.

Hats, backgrounds, slot symbols, and items come from two larger style sheets.
`tools/generate_premium_assets.py` uses fixed coordinates, scales the
motifs, removes the green chroma background, converts to RGB565, and writes
`tamagotchi_arcade/premium_assets.h`. The beanie and straw hat have their own
redesign files that override the corresponding crops.

Accessories are the one exception: they are **not** cropped from a montage.
They're authored directly in `tools/accessory_art.py` as explicit back/front
pixel-art layers on a shared 28×26 canvas (see "How accessories are built"
below); `generate_premium_assets.py` just flattens them into the same header
alongside the montage-derived assets.

In the firmware, `drawPet()` in `tamagotchi_arcade/art.cpp` draws layers in
this order:

1. back accessory layer;
2. the cat's sprite and fur pattern;
3. hat, with any ear occlusion;
4. front accessory layer;
5. sleep indicators.

Hats can hide the ears entirely or have specific ear pixels redrawn on top.

## How the art is tested

`tools/render_hat_simulator.py` and `tools/render_accessory_simulator.py`
read the generated C headers directly. This means they show exactly what
actually gets compiled: the same resolution, transparency, RGB565 colors,
anchors, and layering. Images are upscaled with nearest-neighbor for visual
inspection.

This is better than checking the style sheet alone, because errors from
cropping, color mapping, and transparency become visible. The simulator is
still limited: it currently only shows the neutral cat in one fur variant and
doesn't yet cover the full expression/fur/pattern matrix (issue 004).

## How accessories are built

Each accessory in `tools/accessory_art.py` is two independent pixel grids -
`back` and `front` - on the same `CANVAS_W x CANVAS_H` canvas, plus three
placement fields: `anchor` (`"chest"` or `"face"`), `anchor_row` (which
canvas row lands on the anchor), and `dx` (a small horizontal nudge). Both
layers are generated straight into `PREMIUM_ACC_BACK`/`PREMIUM_ACC_FRONT`
alongside `PREMIUM_ACC_ANCHOR_FACE`/`_ROW` and `PREMIUM_ACC_DX`, and
`drawPremiumAccessoryLayer()` in `art.cpp` is pure table lookup - no
per-item `switch`, no row ranges, no special cases. The simulator reads the
same arrays, so there is exactly one source of truth for placement.

Because each layer has its own local canvas, a "back" piece (a cape's cloth,
a scarf's hanging tail, a backpack's bag) can be positioned anywhere in that
28×26 space independently of the "front" piece (a clasp, a knot, a strap) -
no shared offset hacks are needed to pull them apart.

To add or adjust an accessory:

1. Edit its entry in `tools/accessory_art.py`. `stamp()` paints an ASCII
   grid at an offset; `rect()` fills a block; use `layer(...)` to combine
   several stamps into one canvas.
2. Run `python3 tools/generate_premium_assets.py` - it validates canvas
   size and unknown glyphs and fails loudly.
3. Run `python3 tools/render_accessory_simulator.py` and inspect
   `art_reference/accessory_v2_simulation.png`.
4. Compile the firmware (see `README.md`).

## Recommended workflow for hats and other montage-derived assets

1. Pick one issue at a time.
2. Regenerate the headers after any style-sheet or crop-box change.
3. Render the full test matrix in the simulator.
4. Check transparency and silhouette at high zoom.
5. Compile the firmware.
6. Only flash after an approved simulator sheet, and compare the physical
   screen against the simulator.

See the [open issues](../../issues) for order and acceptance criteria.
