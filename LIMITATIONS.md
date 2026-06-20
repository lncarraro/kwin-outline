# Known Limitations

Date: 2026-06-20

## Bug

For decorated windows such as kitty, the window itself can show rounded bottom
corners, but the kwin-outline effect draws a square outline corner. In the
observed case, both top outline corners are square as expected from KWin's
reported top radius, but the bottom outline corners also appear square even
though the window decoration is rounded at the bottom.

Expected behavior:

- When KWin exposes a reliable bottom-corner radius for the window or
  decoration, the outline should follow that rounded bottom shape.
- The effect should keep the rectangular fallback only when KWin exposes no
  reliable radius data.

Observed behavior:

- The bottom-left corner shows the window/decoration corner is rounded, while the
  outline still paints an L-shaped square corner.

## Current Status

The renderer was changed to prefer the same semantic source KWin uses for
decoration outlines:

- Preferred SSD source: `EffectWindow::decoration()->borderOutline().radius()`
- SSD fallback: `EffectWindow::decoration()->borderRadius()`
- CSD/undecorated fallback: `EffectWindow::windowItem()->borderRadius()`

Touched files:

- `src/outlinewindowrenderer.cpp`
- `src/outlinewindowrenderer.h`

The build passes:

- `cmake --build build`

Runtime verification after installing the new build is complete. The log shows KWin did not expose a decoration outline radius for the observed
kitty window, so the renderer used the SSD fallback source:

- `renderer-refresh-radius source=decoration-radius`
- `outer-tl=0.000000 tr=0.000000 br=5.000000 bl=5.000000`
- `renderer-update-outline ... radius-tl=0.000000 tr=0.000000 br=4.500000 bl=4.500000`

The fresh crops show the bottom outline now follows a rounded corner instead of
painting the previous hard square L shape. Top corners remain square, matching
KWin's reported top radius of zero.

## What We Investigated

### Implementation notes

`.scratch/implementation/00-default.md` requires the effect to use
`KWin::OutlinedBorderItem` and KWin-provided semantic radius data only. It also
requires a rectangular fallback when radius data is absent or unreliable.

`.scratch/implementation/15-automatic-corner-radius.md` requires best-effort
automatic corner-radius matching without pixel inspection, transparency
inspection, or user-configurable radius.

### Current renderer flow

Renderer creation:

- `KWinOutlineEffect::reevaluateWindow()` creates an `OutlineWindowRenderer`.
- The renderer attaches a `KWin::OutlinedBorderItem` to the window's
  `WindowItem`.
- The item is stacked after `windowContainer()`.
- `refreshWindowRadius()` is called before outline state is applied.

Runtime updates:

- `borderRadiusChanged` triggers `updateWindowRadius()`.
- Decoration changes reconnect decoration signals and refresh radius.
- Geometry changes update the outline item inner rect.

### Runtime evidence

Before the latest renderer change, the debug log showed bottom-only radius data:

- `radius-tl=0.000000 tr=0.000000 br=4.500000 bl=4.500000`

That proved KWin was exposing a non-zero bottom radius for some observed windows,
but the visual outline still looked square at the bottom.

### KWin source behavior

Checked KWin 6.7 source from KDE Invent:

- `Window::updateDecorationBorderRadius()` sets `Window::borderRadius()` from
  `decoration()->borderRadius()`.
- `WindowItem::updateBorderRadius()` copies `Window::borderRadius()` to the
  `windowContainer()`.
- `DecorationItem::updateOutline()` renders KWin's own decoration outline from
  `decoration()->borderOutline()`.
- `OutlinedBorderItem::buildQuads()` creates corner quads when outline radius is
  non-zero, and shortens the straight edge quads around those corners.
- The OpenGL renderer has a dedicated `OutlinedBorderItem` path using
  `ShaderTrait::Border`.
- The border shader subtracts an inner rounded box from an outer rounded box
  using `cornerRadius`.

This suggests that `borderOutline().radius()` is the better semantic source for
an `OutlinedBorderItem` than `borderRadius()` when a decoration outline exists.

## Chrome Investigation (2026-06-20)

Moving the debug log before the early-return guard in `refreshWindowRadius()`
confirmed the Chrome case. Chrome logs `source=window-item-radius` with all-zero
radii — `decoration()` is null because Chrome is a CSD window. It draws its own
rounded corners as content pixels and does not communicate a corner radius to
KWin via any Wayland/KDE extension protocol.

Available KWin APIs for CSD windows:
- `windowItem()->borderRadius()` — readable, returns 0 for Chrome.
- `KWin::Item` has no `borderRadiusChanged` signal.
- No `EffectsHandler`-level corner-radius API exists.

Within Phase 15 constraints (no pixel inspection, no user-configurable radius,
KWin semantic data only), a square outline on CSD windows that do not report a
corner radius is correct best-effort behavior. This is a known limitation.

A user-configurable per-window-class radius override would address Chrome and is
a future phase concern.

## SSD top corners are square despite visual rounding (2026-06-20)

For SSD windows such as kitty, Breeze visually paints rounded top corners on the
title bar but reports `tl=0, tr=0` via `decoration()->borderRadius()`. That API
is used for window content clipping — the bottom corners where the decoration
meets the client area. The title bar's top-corner rounding is a Breeze painting
detail that does not propagate to the API.

Log evidence from kitty:

```
renderer-refresh-radius source=decoration-radius outer-tl=0 tr=0 br=5 bl=5
renderer-update-outline radius-tl=0 tr=0 br=4.500000 bl=4.500000
```

The outline faithfully reflects the semantic data: bottom corners rounded, top
corners square. The mismatch is a gap in what Breeze exposes, not a renderer
bug.

### Proposed follow-up: "Mirror bottom corners to top" option

Add a boolean setting **Mirror bottom corners to top** (`mirrorBottomCornersToTop`,
default off). When enabled, any corner whose radius KWin reports as zero is
filled in from its opposite-side partner if that partner is non-zero:

- `tl = max(tl, bl)`
- `tr = max(tr, br)`

This covers the Breeze case where `bl > 0` but `tl = 0` without making any
assumption when both sides are genuinely zero (e.g., maximized windows).
The option belongs in the corner-radius section of the settings UI alongside
thickness and placement.

## Next Steps

1. Phase 15 is complete for SSD windows (kitty). CSD windows without a
   KWin-reported radius (Chrome) will have square outlines by design.
2. If KWin adds a `borderRadiusChanged` signal to `KWin::Item` in a future
   release, or if Chrome starts using a KDE radius protocol, re-evaluate.
3. Implement `mirrorBottomCornersToTop` as a follow-up settings option.

## Important Constraints

- Do not inspect pixels or client transparency to infer application shape.
- Do not add a user-configurable radius in this phase.
- Stay with KWin semantic radius data.
- Keep `KWin::OutlinedBorderItem` as the primary renderer unless local KWin API
  evidence proves it cannot produce the required outline.
