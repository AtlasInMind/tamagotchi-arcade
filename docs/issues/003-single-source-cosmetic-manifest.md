# 003 – Consolidate cosmetic metadata into one source

**Priority:** P1
**Status:** Open ([tracked on GitHub](../../../issues))

## Problem

Anchors, offsets, ear-hiding, and layer rules are duplicated between the
Python tools and `art.cpp`. The simulator can therefore drift from the
firmware after a change.

## Task

- Create one small manifest for name, source files, canvas, anchors,
  offsets, ear policy, and layer policy.
- Generate C++ metadata from the manifest alongside the pixel data.
- Have both simulators read the same manifest.
- Add validation of item count, dimensions, and index order.

## Acceptance criteria

- No manual copy of accessory/hat offsets exists in the simulators.
- Name order is validated against `HAT_NAMES` and `ACCESSORY_NAMES`.
- The generator fails clearly on a missing file, wrong dimension, or invalid
  index.
- Changing an anchor requires only one metadata edit.
