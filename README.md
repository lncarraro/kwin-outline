# kwin-outline

A native KWin compositor effect for KDE Plasma that draws a configurable outline
around application windows.

The outline is rendered by KWin's own `OutlinedBorderItem` API and follows each
window's corner radius automatically. Active and inactive windows can show
different colors. The effect suppresses itself when a fullscreen KWin effect (such
as a screen locker or overview) is active.

## Requirements

- KDE Plasma with KWin 6, Wayland session
- OpenGL compositing enabled (the effect will not load under XRender or software
  compositing)
- KDecoration3

The effect does not work on X11.

## Installation

### Arch Linux

A PKGBUILD is provided in `packaging/`. Install it with `makepkg`:

```sh
cd packaging
makepkg -si
```

This builds from the current source tree and installs the effect and its KCM
configuration module system-wide.

### Manual build

**Build dependencies:** CMake ≥ 3.20, Extra CMake Modules (ECM) ≥ 6.0, a C++20
compiler, Qt 6 (Core, Gui, Widgets, DBus), KF6 (CoreAddons, Config, KCMUtils,
WidgetsAddons), KWin development headers, KDecoration3.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

After installation, restart KWin to pick up the new plugin:

```sh
kwin_wayland --replace &
```

### Development install

`install.sh` automates building, installing, and restarting KWin:

```sh
bash install.sh
```

The script builds, runs `sudo cmake --install`, restarts `kwin_wayland`, and
verifies that KWin loaded the effect over D-Bus. It also clears the previous
debug log. If `qdbus6` is not available, verify the effect state manually in
System Settings.

## Enabling the effect

The effect is disabled by default. Enable it in:

**System Settings → Workspace → Desktop Effects → Appearance → Window Outline**

Saving the settings page applies the change immediately without restarting KWin.

## Configuration

All settings are stored in `~/.config/kwinrc` under the group
`[Effect-kwinoutline]`. They can also be changed through the settings page in
System Settings.

| Setting | Description | Default | Range / Values |
|---|---|---|---|
| Thickness | Outline width in pixels | `1.0` | `0.5` – `8.0` |
| Placement | Where the outline sits relative to the window edge | `Centered` | `Inside`, `Centered`, `Outside` |
| ActiveColor | Outline color for the focused window | `#535659` | Any color with alpha |
| InactiveColor | Outline color for unfocused windows | `#535659` | Any color with alpha |
| DrawInactive | Whether to draw an outline on unfocused windows | `true` | `true` / `false` |
| IncludeUtilityWindows | Whether utility windows (tool palettes, detached panels) receive an outline | `false` | `true` / `false` |
| ExistingDecorationOutlinePolicy | What to do when the window decoration already draws its own outline | `AlwaysDraw` | `AlwaysDraw`, `SkipKnownDecorationOutline` |

**Placement** controls which side of the window edge the outline occupies:
- `Inside` — the outline is drawn inside the window frame, reducing the visible
  client area by the thickness amount.
- `Centered` — the outline straddles the window edge, half inside and half
  outside.
- `Outside` — the outline is drawn outside the window frame and does not obscure
  client content.

**ExistingDecorationOutlinePolicy** controls double-outline situations. When set
to `SkipKnownDecorationOutline`, kwin-outline skips any window whose KWin
decoration reports an existing border outline (via `Decoration::borderOutline()`).
This avoids stacking two outlines on top of each other. Set it to `AlwaysDraw`
to always draw regardless of decoration state.

Note: client-painted borders drawn inside application content (as opposed to the
KWin decoration layer) cannot be detected. If your decoration theme draws its own
outline, you may need to disable that theme option manually.

## Which windows receive an outline

The effect draws on **normal windows** and **dialogs**. With `IncludeUtilityWindows`
enabled, utility windows also receive an outline.

The following window types are never given an outline regardless of settings:
desktop, dock, toolbar, menu, splash screen, dropdown/popup menus, tooltips,
notifications, on-screen displays, combo box popups, drag-and-drop icons,
lock screen, input method windows, and KWin-internal windows.

Fullscreen windows are excluded. When any fullscreen KWin effect is active, all
outlines are hidden until the effect exits.

## Corner radius

The effect reads corner radius data from KWin's semantic APIs rather than
inspecting pixels. For server-side decorated (SSD) windows, it prefers the
decoration's `borderOutline().radius()`; if that is unavailable, it falls back to
`decoration()->borderRadius()`. For client-side decorated (CSD) windows, it uses
`windowItem()->borderRadius()`.

## Known limitations

**CSD windows with content-painted corners** (such as Google Chrome on Wayland)
do not communicate a corner radius to KWin via any Wayland or KDE protocol. The
effect produces a square outline for these windows. This is correct best-effort
behavior given the available APIs.

**SSD windows where the decoration paints rounded top corners as a visual detail**
(for example, kitty with the Breeze theme) may show square top corners in the
outline. Breeze's top-corner rounding is a paint-time decoration detail that is
not exposed through `borderRadius()`, which covers only the content-area corners.
The outline faithfully reflects the semantic data KWin provides.

See [LIMITATIONS.md](LIMITATIONS.md) for investigation notes and evidence.

## Debug logging

The effect writes structured log lines to `/tmp/kwinoutline-debug.log`. To
follow the log in real time:

```sh
tail -f /tmp/kwinoutline-debug.log
```

The log is cleared each time `install.sh` is run.

## License

GPL-2.0-or-later. See [LICENSES/GPL-2.0-or-later.txt](LICENSES/GPL-2.0-or-later.txt).
