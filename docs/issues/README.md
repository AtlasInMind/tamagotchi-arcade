# Issue specs

Known graphics debt is tracked as [GitHub Issues](../../issues) on this
repository. The files in this directory are the long-form specs — full
problem statement and acceptance criteria — that each corresponding GitHub
issue links back to.

| ID | Title |
|---|---|
| [001](001-explicit-accessory-layers.md) | Replace row-cropping with explicit back/front layers |
| [002](002-redraw-accessory-sprites.md) | Redraw all accessories on the target canvas |
| [003](003-single-source-cosmetic-manifest.md) | Consolidate anchors and layer metadata into one source |
| [004](004-expand-visual-test-matrix.md) | Expand the simulator to all relevant combinations |
| [005](005-shop-preview-and-device-qa.md) | Reconcile shop preview and physical QA |

Recommended order: 001 → 002, then 003 → 004 → 005. Issue 001 can be
implemented with a simple temporary sprite first; issue 002 delivers the
final art in the new format.
