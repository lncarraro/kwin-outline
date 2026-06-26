# Contributing to kwin-outline

## Prerequisites

Before writing code, read the [KWin ABI stability](README.md#kwin-abi-stability)
section of the README. Building against the wrong KWin headers produces a binary
that may silently do nothing, fail to load, or crash KWin.

Target platform: **Arch Linux with KDE Plasma 6.7 (Wayland session)**.

Install build dependencies:

```sh
sudo pacman -S cmake extra-cmake-modules ninja \
               qt6-base kwin kdecoration kcmutils kconfig \
               kcoreaddons kwidgetsaddons vulkan-headers
```

## Development workflow

### 1. Build

```sh
cmake --build build --parallel
```

For a clean build or when changing CMake options:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

### 2. Install and restart KWin

Qt's plugin loader caches the `.so` in memory. A D-Bus unload/reload cycle
alone does not pick up a recompiled binary — it reuses the cached version.
You must replace the running KWin process to load the new file:

```sh
bash install.sh
```

`install.sh` builds, installs system-wide, clears the debug log, restarts KWin,
and confirms over D-Bus that the effect is loaded.

### 3. Verify the effect loaded

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded kwinoutline
```

### 4. Watch the debug log

```sh
tail -f /tmp/kwinoutline-debug.log
```

Log lines are timestamped and prefixed with a level tag:
- `[INFO]` — lifecycle events (load, unload, reconfigure)
- `[DEBUG]` — per-window state transitions
- `[WARN]` — unexpected or degraded conditions

### 5. Inspect runtime state

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.effectDebug kwinoutline ""
```

Example output:

```
backend: OpenGL compositing
config: thickness=1 placement=Centered activeColor=#ff535659 inactiveColor=#ff535659 drawInactive=true includeUtility=false decorationPolicy=AlwaysDraw
tracked windows: 3
visible outlines: 2
suppression: inactive
```

### 6. Reload configuration without restarting KWin

Edit `~/.config/kwinrc` under `[Effect-kwinoutline]`, then:

```sh
qdbus6 org.kde.KWin /Effects reconfigureEffect kwinoutline
```

This applies color, thickness, placement, and policy changes immediately. It
does not reload a recompiled binary — for that, use `install.sh`.

### 7. Unload and load without restarting KWin

Useful when testing the load/unload path or toggling the effect:

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwinoutline
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwinoutline
```

## Code organization

```
src/
  kwinoutline.h / .cpp           Effect class: signal wiring, window lifecycle
  kwinwindoweligibility.h / .cpp KWin-specific eligibility snapshot builder
  windoweligibility.h / .cpp     Pure eligibility predicate (no KWin types)
  outlinegeometry.h / .cpp       Placement geometry computation
  outlinewindowrenderer.h / .cpp KWin scene item adapter (OutlinedBorderItem)
  kwinoutlinelog.h               File-based structured logger
  kwinoutlinesettings.kcfg       KConfigXT settings schema
  kwinoutlinesettings.kcfgc      KConfigXT code generation configuration
  kcm/                           KCM configuration module
packaging/
  PKGBUILD                       Arch Linux package definition
```

KWin scene item APIs (`WindowItem`, `OutlinedBorderItem`, `BorderOutline`,
`BorderRadius`) are intentionally isolated inside `outlinewindowrenderer.cpp`.
If these APIs change in a future KWin release, only that file needs updating.

## Logging

All logging uses the functions in `kwinoutlinelog.h`:

- `infoLog(format, ...)` — lifecycle events (load, unload, reconfigure)
- `debugLog(format, ...)` — per-window state transitions
- `warnLog(format, ...)` — unexpected or degraded conditions

Do not use Qt logging categories (`qDebug`, `qWarning`, `qCDebug`, etc.). The
file-based logger writes to `/tmp/kwinoutline-debug.log` without requiring any
Qt environment variable or journal filter.

Do not log on every paint, damage event, or geometry update. Log state
transitions.

## KWin API guidelines

Before using any KWin function or class, verify the name and signature in the
installed headers under `/usr/include/kwin/`. Names remembered from
documentation or other sources may not match the exact version installed.

If a required KWin API is absent or has a different signature than expected,
document the exact finding (header file, actual declaration) before implementing
a workaround. Do not guess or adapt from similar names without evidence.

## License

All contributions must be licensed under GPL-2.0-or-later. Add an SPDX header
to every new source file:

```cpp
// SPDX-FileCopyrightText: <year> kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later
```
