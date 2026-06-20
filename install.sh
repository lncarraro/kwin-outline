#!/usr/bin/env bash
set -euo pipefail

cmake --build build
sudo cmake --install build

rm -f /tmp/kwinoutline-debug.log

# Replace KWin to force the dynamic linker to load the new .so from disk.
# unloadEffect + loadEffect is not sufficient: Qt's plugin loader caches the
# shared library in memory and dlopen returns the stale version.
if ! command -v kwin_wayland >/dev/null 2>&1; then
    echo "kwin_wayland not found; cannot replace KWin." >&2
    exit 1
fi

kwin_wayland --replace >/tmp/kwinoutline-kwin-replace.log 2>&1 &
sleep 3

if command -v qdbus6 >/dev/null 2>&1; then
    checked_effect=false

    for _ in {1..10}; do
        if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded kwinoutline; then
            checked_effect=true
            break
        fi

        sleep 1
    done

    if [[ "$checked_effect" == false ]]; then
        echo "Could not query KWin over D-Bus; check /tmp/kwinoutline-kwin-replace.log." >&2
    fi
else
    echo "qdbus6 not found; check effect state manually."
fi

echo "Install complete. Debug logs:"
echo "journalctl --user -b --no-pager | rg 'DEBUG-kwinoutline'"
