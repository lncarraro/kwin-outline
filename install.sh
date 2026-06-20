#!/usr/bin/env bash
set -euo pipefail

cmake --build build
sudo cmake --install build

rm -f /tmp/kwinoutline-debug.log

if command -v qdbus6 >/dev/null 2>&1; then
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwinoutline >/dev/null 2>&1 || true
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwinoutline >/dev/null
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded kwinoutline
else
    echo "qdbus6 not found; restart KWin or reload the effect manually."
fi

echo "Install complete. Debug logs:"
echo "journalctl --user -b --no-pager | rg 'DEBUG-kwinoutline'"
