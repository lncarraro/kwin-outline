# Known Limitations

kwin-outline reads corner radius data from KWin's semantic APIs. It does not
inspect pixel content or application transparency. When a window's corner shape
is not communicated to KWin through those APIs, the outline falls back to a
rectangular (square-cornered) shape.

Two window categories are affected.

**Terminology used below:**
- **SSD** (server-side decorated) — windows with a decoration frame applied by
  the window manager (e.g., kitty with Breeze).
- **CSD** (client-side decorated) — windows that draw their own title bars and
  decorations inside the application content area (e.g., Google Chrome on
  Wayland).

---

## Square corners on CSD windows with self-painted rounding

**What you observe:** Applications such as Google Chrome draw their own rounded
corners as pixel content. The application does not communicate a corner radius to
KWin through any Wayland or KDE protocol. As a result, kwin-outline draws a
square outline around Chrome even though the Chrome window visually appears
rounded.

**Workaround:** None in the current version.

### Technical notes

The effect reads CSD corner radius from `windowItem()->borderRadius()`. For
Chrome this returns zero because Chrome does not use any KDE or Wayland radius
protocol.

Debug log when Chrome is the active window:

```
renderer-refresh-radius source=window-item-radius outer-tl=0 tr=0 br=0 bl=0
```

KWin APIs checked for CSD windows:
- `windowItem()->borderRadius()` — readable, returns 0 for Chrome.
- `KWin::Item` has no `borderRadiusChanged` signal.
- No `EffectsHandler`-level corner-radius API exists.

A per-window-class radius override setting would address Chrome, but would
require the user to manually configure the radius for each application.

**Design constraint:** The effect must not inspect pixels or application
transparency to infer corner shape. It must use only KWin-provided semantic
radius data.

---

## Square top corners on SSD windows with Breeze decoration

**What you observe:** Applications such as kitty with the Breeze theme show
visually rounded corners at the top of the title bar. The outline has rounded
bottom corners but square top corners on these windows.

**Workaround:** None in the current version.

### Technical notes

The effect queries corner radius from KWin in the following priority order:

1. `EffectWindow::decoration()->borderOutline().radius()` — preferred for SSD
   windows; this is the same source KWin uses for its own decoration outlines.
2. `EffectWindow::decoration()->borderRadius()` — SSD fallback when no
   decoration border outline exists.
3. `EffectWindow::windowItem()->borderRadius()` — used for CSD and undecorated
   windows.

For kitty with Breeze, `borderOutline()` is null, so the SSD fallback applies.
`borderRadius()` covers the corners where the decoration meets the client content
area — the bottom corners. The Breeze title bar's rounded top corners are a
paint-time visual detail that does not propagate to this API.

Debug log from kitty:

```
renderer-refresh-radius source=decoration-radius outer-tl=0 tr=0 br=5 bl=5
renderer-update-outline radius-tl=0 tr=0 br=4.500000 bl=4.500000
```

The outline faithfully reflects the reported data: bottom corners rounded (`br=5,
bl=5`), top corners square (`tl=0, tr=0`). This is not a renderer bug; the
mismatch is a gap in what Breeze exposes through the decoration API.

KWin 6.7 source context:
- `Window::updateDecorationBorderRadius()` sets `Window::borderRadius()` from
  `decoration()->borderRadius()`.
- `WindowItem::updateBorderRadius()` copies that to `windowContainer()`.
- `DecorationItem::updateOutline()` renders KWin's own decoration outline from
  `decoration()->borderOutline()`.
- `OutlinedBorderItem::buildQuads()` creates corner quads when outline radius is
  non-zero and shortens the adjacent straight-edge quads.
- The border shader subtracts an inner rounded box from an outer rounded box
  using `cornerRadius`.

### Proposed approach for a future fix

Add a **Mirror bottom corners to top** boolean setting (default off). When
enabled, any per-corner radius that KWin reports as zero is filled in from its
opposite-side partner if that partner is non-zero:

```
tl = max(tl, bl)
tr = max(tr, br)
```

This covers the Breeze case where `bl > 0` but `tl = 0`, without making any
assumption when both sides are genuinely zero (e.g., maximized windows). The
setting belongs in the outline appearance section alongside thickness and
placement.

**Design constraint:** The effect must not add a user-configurable absolute
radius. The approach above is acceptable because it derives from KWin-reported
data rather than requiring the user to supply a raw pixel value.
