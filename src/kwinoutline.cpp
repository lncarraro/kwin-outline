// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kwinoutline.h"

#include "kwinwindoweligibility.h"
#include "windoweligibility.h"

#include <effect/effect.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>

namespace KWinOutline
{

KWinOutlineEffect::KWinOutlineEffect()
    : KWin::Effect()
{
    connect(KWin::effects, &KWin::EffectsHandler::windowAdded, this, &KWinOutlineEffect::handleWindowAdded);
    connect(KWin::effects, &KWin::EffectsHandler::windowClosed, this, &KWinOutlineEffect::handleWindowClosed);
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this, &KWinOutlineEffect::handleWindowDeleted);

    // Attach outlines to windows already open when the effect loads.
    const auto existingWindows = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *w : existingWindows) {
        watchWindowLifetime(w);
        reevaluateWindow(w);
    }
}

KWinOutlineEffect::~KWinOutlineEffect()
{
    // Destroy all renderers before their WindowItems disappear.
    m_renderers.clear();
}

bool KWinOutlineEffect::blocksDirectScanout() const
{
    return false;
}

void KWinOutlineEffect::handleWindowAdded(KWin::EffectWindow *w)
{
    watchWindowLifetime(w);
    reevaluateWindow(w);
}

void KWinOutlineEffect::handleWindowClosed(KWin::EffectWindow *w)
{
    removeWindow(w);
}

void KWinOutlineEffect::handleWindowDeleted(KWin::EffectWindow *w)
{
    removeWindow(w);
}

void KWinOutlineEffect::handleWindowDestroyed(QObject *object)
{
    removeWindow(object);
}

void KWinOutlineEffect::reevaluateWindow(KWin::EffectWindow *w)
{
    if (!w) {
        return;
    }

    const WindowEligibilitySnapshot snapshot = snapshotWindowEligibility(*w);
    const WindowEligibilityOptions options;
    if (!isWindowEligibleForOutline(snapshot, options)) {
        removeWindow(w);
        return;
    }

    if (m_renderers.contains(w)) {
        return;
    }

    auto renderer = std::make_unique<OutlineWindowRenderer>(*w);
    if (renderer->trackedWindowCount() == 0) {
        return;
    }

    m_renderers.emplace(w, std::move(renderer));
}

void KWinOutlineEffect::removeWindow(KWin::EffectWindow *w)
{
    m_renderers.erase(w);
}

void KWinOutlineEffect::removeWindow(QObject *object)
{
    for (auto it = m_renderers.begin(); it != m_renderers.end();) {
        if (static_cast<QObject *>(it->first) == object) {
            it = m_renderers.erase(it);
        } else {
            ++it;
        }
    }
}

void KWinOutlineEffect::watchWindowLifetime(KWin::EffectWindow *w)
{
    if (!w) {
        return;
    }

    connect(w, &QObject::destroyed, this, &KWinOutlineEffect::handleWindowDestroyed, Qt::UniqueConnection);
}

} // namespace KWinOutline

KWIN_EFFECT_FACTORY(KWinOutline::KWinOutlineEffect, "metadata.json")

#include "kwinoutline.moc"
