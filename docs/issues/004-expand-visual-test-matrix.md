# 004 – Expand the visual test matrix

**Priority:** P1
**Status:** Open ([tracked on GitHub](../../../issues))

## Problem

The simulators mostly show the neutral calico cat. Errors may be hidden in
other expressions, fur colors, patterns, or hat/accessory combinations.

## Task

- Render all five compiled facial expressions.
- Render at least light, dark, and calico fur.
- Render relevant patterns that could collide with the chest/face.
- Create a focused combination sheet for hats + glasses/scarf/capes.
- Add an automatic check for non-transparent pixels outside the canvas.
- Write files with stable names so before/after can be compared visually.

## Acceptance criteria

- Every accessory appears on all expressions in at least one overview.
- Light and dark fur reveal no halo/edge errors.
- No combination covers eyes, nose, or paws unless that's an explicit part
  of the design.
- The simulators still parse the generated C headers, not just source PNGs.
