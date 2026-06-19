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
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this, &KWinOutlineEffect::handleWindowDeleted);

    // Attach outlines to windows already open when the effect loads.
    const auto existingWindows = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *w : existingWindows) {
        addWindow(w);
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
    addWindow(w);
}

void KWinOutlineEffect::handleWindowDeleted(KWin::EffectWindow *w)
{
    m_renderers.erase(w);
}

void KWinOutlineEffect::addWindow(KWin::EffectWindow *w)
{
    const WindowEligibilitySnapshot snapshot = snapshotWindowEligibility(*w);
    const WindowEligibilityOptions options;
    if (!isWindowEligibleForOutline(snapshot, options)) {
        return;
    }
    m_renderers.emplace(w, std::make_unique<OutlineWindowRenderer>(*w));
}

} // namespace KWinOutline

KWIN_EFFECT_FACTORY(KWinOutline::KWinOutlineEffect, "metadata.json")

#include "kwinoutline.moc"
