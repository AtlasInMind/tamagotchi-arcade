# 005 – Reconcile shop preview and physical QA

**Priority:** P2
**Status:** Open ([tracked on GitHub](../../../issues))

## Problem

`drawAccessory()` shows the layers without the cat, while the home screen
composites them around the cat. An item can therefore look different or
unclear in the shop. The simulator also doesn't capture the screen's actual
contrast and scaling.

## Task

- Decide whether shop cards should show the item alone or on a small cat.
- Ensure the same anchor and color handling in the shop and at home.
- Create a physical QA checklist for all hats and accessories.
- Document screenshots/findings after each approved hardware round.

## Acceptance criteria

- All items are recognizable at the shop's actual size.
- A selected item looks consistent between shop, closet, and home.
- The full cosmetics list is checked on the ESP32 after an approved
  simulator sheet.
- Uploads only happen after a successful compile, using an explicitly
  found serial port at 115200 baud.
