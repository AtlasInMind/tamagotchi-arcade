# 002 – Redraw all accessories on the target canvas

**Priority:** P0
**Status:** Resolved. All nine accessories are hand-drawn directly on the
28×26 canvas in `tools/accessory_art.py`, no longer cropped from
`cosmetics_style_guide.png`. Further visual polish is welcome but the
montage-cropping path is gone.
**Depends on:** 001

## Problem

Today's accessories are machine-cropped cutouts from
`cosmetics_style_guide.png`. The cutouts carry anti-aliasing, background
residue, and perspective that don't always fit the 30×40 cat.

## Task

Create clean, hand-checked back/front sprites for all nine accessories:

1. Silk Bowtie
2. Gold Pendant
3. Round Glasses
4. Knit Scarf
5. Sunglasses
6. Leather Pack
7. Hero Medal
8. Crimson Cape
9. Royal Cape

Use a limited RGB565-friendly palette, hard pixel edges, and transparent
background. Design directly on the 28×26 canvas against the compiled cat.

## Acceptance criteria

- No foreign fragments or chroma-green fringe pixels.
- Each motif is identifiable at 1× size on the screen.
- Symmetric motifs are visually balanced around the correct anchor.
- Scarf, backpack, and capes have meaningful separate layers.
- Glasses follow the eye line without hiding the nose or ears.
- Medal, pendant, and bowtie sit on the chest, not on the face.
- All nine pass the test matrix in issue 004.

## Deliverable

Keep the original style sheets as reference, but make the new small sprite
files the authoritative source for the generator.
