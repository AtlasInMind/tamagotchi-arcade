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

Hats, accessories, backgrounds, slot symbols, and items mostly come from two
larger style sheets. `tools/generate_premium_assets.py` uses fixed
coordinates, scales the motifs, removes the green chroma background,
converts to RGB565, and writes `tamagotchi_arcade/premium_assets.h`. The
beanie and straw hat have their own redesign files that override the
corresponding crops.

In the firmware, `drawPet()` in `tamagotchi_arcade/art.cpp` draws layers in
this order:

1. back accessory layer;
2. the cat's sprite and fur pattern;
3. hat, with any ear occlusion;
4. front accessory layer;
5. sleep indicators.

Hats can hide the ears entirely or have specific ear pixels redrawn on top.
Accessories currently use item-specific placements and row selections.
Scarves, backpacks, and capes have back layers; the scarf has a limited front
neck layer, the backpack is entirely behind, and capes get a small
program-drawn clasp in front.

## How the art is tested

`tools/render_hat_simulator.py` and `tools/render_accessory_simulator.py`
read the generated C headers directly. This means they show exactly what
actually gets compiled: the same resolution, transparency, RGB565 colors,
anchors, and layering. Images are upscaled with nearest-neighbor for visual
inspection.

This is better than checking the style sheet alone, because errors from
cropping, color mapping, and transparency become visible. The simulator is
still limited: it currently only shows the neutral cat in one fur variant and
duplicates some layout rules from C++.

## Why accessories still look unnatural

The sources are finished illustrations on one large montage sheet, not
hand-drawn 28×26 sprites with semantic layers. A single flat sprite contains
both what should be in front and what should be behind the cat. Horizontal
cropping cannot describe shapes like scarf tails, shoulder straps, and capes
in a natural way. Chroma-keying an anti-aliased image can also leave fringe
colors at the edges.

The long-term solution is separate, pixel-edited layers per accessory:

- `back`: fabric, straps, or volume that sits behind the cat;
- `front`: knot, collar, clasp, or lens that actually sits in front;
- optionally an explicit mask if part of the cat should be hidden.

The layers should share the same canvas, anchor, and palette, and the
generator should read all metadata from one manifest that's also used by the
simulator and the firmware.

## Recommended workflow going forward

1. Pick one issue and one accessory at a time.
2. Draw back/front sprites directly on the target canvas; don't crop a
   montage.
3. Regenerate the headers.
4. Render the full test matrix in the simulator.
5. Check transparency and silhouette at high zoom.
6. Compile the firmware.
7. Only flash after an approved simulator sheet, and compare the physical
   screen against the simulator.

See the [open issues](../../issues) for order and acceptance criteria.
