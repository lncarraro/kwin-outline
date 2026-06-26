# kwin-outline

A native KWin compositor effect for KDE Plasma that draws a thin, configurable
outline around application windows.

The outline is rendered using KWin's `OutlinedBorderItem` API and participates
in normal scene stacking. It respects window occlusion, follows each window's
position and transforms automatically, and matches the window's reported corner
radius. Active and inactive windows can show different colors. All outlines are
hidden while a fullscreen KWin effect (Overview, Desktop Grid, screen locker,
etc.) is active.

## Screenshots

*(No screenshot yet. Contributions welcome.)*

## Supported platform

| Component | Required |
|---|---|
| KDE Plasma | 6.7 |
| KWin session | Wayland |
| Compositor | OpenGL |
| Application protocol | Native Wayland and XWayland |

The effect does **not** load under XRender or software compositing, and is
**not** supported in the KWin X11 session.

Arch Linux is the primary development and packaging environment. The effect
should build on any distribution that provides Plasma 6.7 dependencies, but
only Arch Linux is continuously tested in CI.

## Requirements

**Runtime:**
- KDE Plasma 6.7
- KWin Wayland session with OpenGL compositing
- KDecoration3 (installed as a dependency of Plasma)

**Build:**
- CMake ≥ 3.20
- Extra CMake Modules (ECM) ≥ 6.0
- C++20 compiler (GCC or Clang)
- Qt 6 (Core, Gui, Widgets, DBus)
- KF6 (CoreAddons, Config, KCMUtils, WidgetsAddons)
- KWin development headers (6.7)
- KDecoration3 development headers

## Installation

### Arch Linux

A PKGBUILD is provided in `packaging/`. It pulls the source from the Git
repository and installs the effect and its configuration module system-wide.

```sh
cd packaging
makepkg -si
```

After installation, enable the effect in System Settings. See
[Enabling the effect](#enabling-the-effect).

### Manual build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

The default install prefix is `/usr`. The plugin installs to
`/usr/lib/qt6/plugins/kwin/effects/plugins/kwinoutline.so`.

After the first install, enable the effect through System Settings or D-Bus —
KWin does not need to be restarted. If you later rebuild the plugin (changing
compiled code), restart KWin to clear its plugin cache:

```sh
kwin_wayland --replace &
```

### Development install

`install.sh` automates building, installing, and restarting KWin:

```sh
bash install.sh
```

The script builds from the current source tree, installs system-wide, clears
the previous debug log, restarts KWin, and confirms over D-Bus that the effect
loaded.

Restarting KWin is necessary after every rebuild because Qt's plugin loader
caches the shared library in memory. A D-Bus unload/reload cycle alone reuses
the cached binary, not the newly installed one.

## Enabling the effect

The effect is disabled by default. Enable it in:

**System Settings → Workspace → Desktop Effects → Appearance → Window Outline**

Saving the settings page applies the change without restarting KWin.

To toggle the effect over D-Bus without opening System Settings:

```sh
# Load (start) the effect
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwinoutline

# Unload (stop) the effect
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwinoutline

# Check whether the effect is currently loaded
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded kwinoutline
```

**Note:** D-Bus load/unload reuses the `.so` already cached in memory. If you
just installed a new build, restart KWin first (or use `install.sh`) to ensure
the new binary is active before loading through D-Bus.

## Configuration

All settings live in `~/.config/kwinrc` under `[Effect-kwinoutline]`. They can
be edited with any text editor or changed through the settings page in System
Settings.

To apply a change made directly to `kwinrc` without restarting KWin:

```sh
qdbus6 org.kde.KWin /Effects reconfigureEffect kwinoutline
```

`qdbus6 org.kde.KWin /KWin reconfigure` does **not** reach this effect. Use
`/Effects reconfigureEffect` instead.

### Settings reference

| Setting | Type | Default | Range / Values | Description |
|---|---|---|---|---|
| `Thickness` | double | `1.0` | `0.5` – `8.0` | Outline width in logical pixels |
| `Placement` | enum | `Centered` | `Inside` `Centered` `Outside` | Where the outline sits relative to the window edge |
| `ActiveColor` | color | `#535659` | Any ARGB color | Color for the focused window outline |
| `InactiveColor` | color | `#535659` | Any ARGB color | Color for unfocused window outlines |
| `DrawInactive` | bool | `true` | `true` / `false` | Whether to draw outlines on unfocused windows |
| `IncludeUtilityWindows` | bool | `false` | `true` / `false` | Whether tool palettes and detached panels receive outlines |
| `ExistingDecorationOutlinePolicy` | enum | `AlwaysDraw` | `AlwaysDraw` `SkipKnownDecorationOutline` | What to do when the window decoration already draws its own outline |

### Placement

`Placement` controls which side of the window frame edge the outline occupies:

- **`Inside`** — the entire outline is drawn inside the window frame, reducing
  the visible client area by the thickness amount.
- **`Centered`** — the outline straddles the window edge: half inside the
  frame, half outside. This is the default.
- **`Outside`** — the entire outline is drawn outside the window frame. Client
  content is not obscured.

At output edges (screen borders), the chosen placement is preserved and the
outline clips naturally. Individual sides are not moved inward to keep them
on-screen.

### Thickness and fractional scaling

`Thickness` is specified in **logical pixels**. KWin converts logical pixels to
device pixels using the output's scale factor.

On a 2× HiDPI display, a thickness of `1.0` produces a 2-device-pixel outline.
Fractional values such as `0.5` are valid and result in sub-pixel-aligned
rendering on scaled outputs. The exact device-pixel appearance depends on the
display scale and KWin's rounding behavior.

### Existing decoration outline policy

`ExistingDecorationOutlinePolicy` controls what happens when a window's KWin
decoration already draws its own outline:

- **`AlwaysDraw`** (default) — draw the kwin-outline outline regardless of what
  the decoration provides.
- **`SkipKnownDecorationOutline`** — skip the kwin-outline for any window whose
  KWin decoration reports a border outline through
  `KDecoration3::Decoration::borderOutline()`.

Client-painted borders drawn inside application content cannot be detected.
These are not visible to the KWin decoration API. If your application or
decoration theme draws its own border and you want to avoid doubling, you may
need to disable that theme option manually.

## Which windows receive an outline

The effect draws outlines on **normal windows** and **dialogs** by default.
Enabling `IncludeUtilityWindows` also covers tool palettes, detached panels,
and similar utility-type windows.

Both native Wayland windows and XWayland windows are eligible, subject to the
same type-based rules.

The following window types are **never** given an outline regardless of
settings: desktop, dock, toolbar, menu, splash screen, dropdown menus, popup
menus, tooltips, notifications, critical notifications, applet popups,
on-screen displays (OSD), combo box popups, drag-and-drop icons, lock screen,
input method windows, and KWin-internal windows.

Fullscreen windows are also excluded. See [Fullscreen behavior](#fullscreen-behavior).

## Fullscreen behavior

- A fullscreen application window never receives an outline.
- When any fullscreen KWin effect is active — Overview, Desktop Grid, Present
  Windows, the screen locker, or any other effect that sets the fullscreen-effect
  flag — **all** outlines are hidden immediately.
- Outlines are restored when the fullscreen effect exits.

The effect does not attempt to render outlines inside effect thumbnails.

## Corner radius

The effect reads corner radius data from KWin's semantic APIs. It does not
inspect pixels or application transparency.

The lookup priority for each window:

1. `decoration()->borderOutline().radius()` — used for server-side decorated
   (SSD) windows when the decoration reports an outline with a radius.
2. `decoration()->borderRadius()` — SSD fallback when no decoration border
   outline exists.
3. `windowItem()->borderRadius()` — used for client-side decorated (CSD) and
   undecorated windows.

When none of these return a non-zero radius, the outline is rectangular.

## Known limitations

See [LIMITATIONS.md](LIMITATIONS.md) for full investigation notes and technical
evidence.

**CSD windows with self-painted rounded corners** (such as Google Chrome on
Wayland) do not communicate a corner radius to KWin through any Wayland or KDE
protocol. The effect produces a square outline for these windows. No workaround
is available in the current version.

**SSD windows where the decoration paints rounded top corners as a visual
detail** (for example, kitty with the Breeze theme) may show square top corners
in the outline. Breeze's top-corner rounding is a paint-time decoration detail
not exposed through `decoration()->borderRadius()`. The outline faithfully
reflects the data KWin reports.

## Troubleshooting

### The effect does not appear in System Settings

Verify the plugin was installed to the expected path:

```sh
ls /usr/lib/qt6/plugins/kwin/effects/plugins/kwinoutline.so
```

If the file is missing, reinstall:

```sh
sudo cmake --install build
```

### The effect appears in System Settings but does not load

Check whether KWin considers the effect supported:

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectSupported kwinoutline
```

If this returns `false`, the compositor backend does not meet requirements. The
effect requires OpenGL compositing. Verify `~/.config/kwinrc` under
`[Compositing]` — `Backend` must be `OpenGL`.

Check the debug log for a specific reason:

```sh
cat /tmp/kwinoutline-debug.log
```

### The effect loads but outlines are not visible

Print the current runtime state:

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.effectDebug kwinoutline ""
```

Output includes backend status, active configuration, tracked window count,
visible outline count, and suppression state.

Follow the log in real time while interacting with windows:

```sh
tail -f /tmp/kwinoutline-debug.log
```

Common causes:
- `DrawInactive=false` with no window focused, or no focus yet acquired.
- All visible windows are excluded by type (desktop, panels, etc.).
- A fullscreen KWin effect is active — check the `suppression:` line in the
  debug output.
- Active or inactive color has full transparency (alpha `00`).

### Configuration changes have no effect

Use the correct D-Bus call:

```sh
qdbus6 org.kde.KWin /Effects reconfigureEffect kwinoutline
```

The top-level `/KWin reconfigure` does not trigger this effect's reload.

### The debug log is not created

`/tmp/kwinoutline-debug.log` is created when the effect writes its first log
entry. If the file does not exist, the effect has not loaded or has not
processed any event yet.

## Compatibility

### Release matrix

kwin-outline is built and tested against a specific Plasma release. Because
KWin native effect headers are not stable across releases, a plugin built for
one version of KWin will generally not work with a different version.

| kwin-outline | Target Plasma / KWin | Notes |
|---|---|---|
| 0.1.0 | Plasma 6.7 / KWin 6.7 | Initial release |

### KWin ABI stability

KWin does not provide a stable plugin ABI or API for native effects. The scene
item interfaces used by this effect (`WindowItem`, `OutlinedBorderItem`,
`BorderOutline`, `BorderRadius`) are internal KWin APIs that can change in any
KWin release without a deprecation notice.

A plugin compiled against one version of the KWin headers may fail to load,
crash, or behave incorrectly when KWin is updated, even within the same major
version series.

**Always rebuild kwin-outline against the KWin headers that match the running
KWin version.** The Arch Linux PKGBUILD and the CI configuration pin to the
Plasma 6.7 package tree to enforce this.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the development workflow, code
organization, and contribution guidelines.

## License

GPL-2.0-or-later. See [LICENSES/GPL-2.0-or-later.txt](LICENSES/GPL-2.0-or-later.txt).
