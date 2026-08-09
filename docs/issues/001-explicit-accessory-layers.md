# 001 – Replace row-cropping with explicit accessory layers

**Priority:** P0
**Status:** Open ([tracked on GitHub](../../../issues))

## Problem

`drawPremiumAccessoryLayer()` picks certain horizontal rows from one flat
sprite to simulate foreground. This cuts shapes at arbitrary points and can't
place scarves, straps, or capes naturally around the cat.

## Task

- Change the asset format to separate `back` and `front` layers for each
  accessory.
- Both layers should use the same 28×26 canvas and the same anchor.
- Draw the whole back layer before the cat and the whole front layer after
  the cat.
- Remove item-specific `firstRow`/`lastRow` rules.
- Keep `None` as a valid choice with no sprite.
- Update the simulator to read and draw the same layers.

## Acceptance criteria

- No accessory uses horizontal row-cropping at runtime.
- Scarf: tail behind the body, knot/collar in front, no face coverage.
- Backpack: bag behind the body; any shoulder strap is a separate front
  layer.
- Capes: fabric behind the body; only a small collar/clasp in front.
- Glasses and sunglasses can have an empty back layer and a complete front
  layer.
- Simulator sheet and firmware use identical generated layer data.
- Firmware compiles without a meaningful increase in dynamic memory use.

## Affected files

- `tools/generate_premium_assets.py`
- `tools/render_accessory_simulator.py`
- `tamagotchi_arcade/premium_assets.h` (generated)
- `tamagotchi_arcade/art.cpp`
