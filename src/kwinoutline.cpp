// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kwinoutline.h"

#include "kwinoutlinesettings.h"
#include "kwinwindoweligibility.h"
#include "outlinegeometry.h"
#include "windoweligibility.h"

#include <effect/effect.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>

namespace KWinOutline
{

static OutlinePlacement toOutlinePlacement(KWinOutlineSettings::EnumPlacement::type p)
{
    return static_cast<OutlinePlacement>(static_cast<int>(p));
}

KWinOutlineEffect::KWinOutlineEffect()
    : KWin::Effect()
    , m_settings(std::make_unique<KWinOutlineSettings>())
{
    m_activeWindow = KWin::effects->activeWindow();

    connect(KWin::effects, &KWin::EffectsHandler::windowAdded, this, &KWinOutlineEffect::handleWindowAdded);
    connect(KWin::effects, &KWin::EffectsHandler::windowClosed, this, &KWinOutlineEffect::handleWindowClosed);
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this, &KWinOutlineEffect::handleWindowDeleted);
    connect(KWin::effects, &KWin::EffectsHandler::windowActivated, this, &KWinOutlineEffect::handleWindowActivated);
    connect(KWin::effects, &KWin::EffectsHandler::hasActiveFullScreenEffectChanged, this, &KWinOutlineEffect::handleHasActiveFullScreenEffectChanged);

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

void KWinOutlineEffect::reconfigure(ReconfigureFlags)
{
    const bool oldIncludeUtility = m_settings->includeUtilityWindows();
    const double oldThickness = m_settings->thickness();
    const auto oldPlacement = m_settings->placement();
    const QColor oldActiveColor = m_settings->activeColor();
    const QColor oldInactiveColor = m_settings->inactiveColor();
    const bool oldDrawInactive = m_settings->drawInactive();

    m_settings->load();

    const bool includeUtilityChanged = (m_settings->includeUtilityWindows() != oldIncludeUtility);
    const bool geometryChanged = !qFuzzyCompare(m_settings->thickness(), oldThickness)
                                 || (m_settings->placement() != oldPlacement);
    const bool colorStateChanged = (m_settings->activeColor() != oldActiveColor)
                                   || (m_settings->inactiveColor() != oldInactiveColor)
                                   || (m_settings->drawInactive() != oldDrawInactive);

    if (includeUtilityChanged) {
        for (KWin::EffectWindow *w : KWin::effects->stackingOrder()) {
            reevaluateWindow(w);
        }
    }

    if (geometryChanged) {
        const double newThickness = m_settings->thickness();
        const OutlinePlacement newPlacement = toOutlinePlacement(m_settings->placement());
        for (auto &[w, renderer] : m_renderers) {
            renderer->setThicknessAndPlacement(newThickness, newPlacement, w->frameGeometry().size());
        }
    }

    if (colorStateChanged || includeUtilityChanged) {
        applyOutlineStateToAll();
    }
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
    const WindowEligibilityOptions options{.includeUtilityWindows = m_settings->includeUtilityWindows()};
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

    auto [it, inserted] = m_renderers.emplace(w, std::move(renderer));
    it->second->setThicknessAndPlacement(
        m_settings->thickness(),
        toOutlinePlacement(m_settings->placement()),
        w->frameGeometry().size());
    applyOutlineState(w);
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
    connect(w, &KWin::EffectWindow::windowFrameGeometryChanged, this, &KWinOutlineEffect::handleWindowFrameGeometryChanged, Qt::UniqueConnection);
    connect(w, &KWin::EffectWindow::windowFullScreenChanged, this, &KWinOutlineEffect::handleWindowFullScreenChanged, Qt::UniqueConnection);
}

void KWinOutlineEffect::handleWindowFrameGeometryChanged(KWin::EffectWindow *w, const KWin::RectF &)
{
    auto it = m_renderers.find(w);
    if (it == m_renderers.end()) {
        return;
    }
    it->second->updateFrameSize(w->frameGeometry().size());
}

void KWinOutlineEffect::handleWindowActivated(KWin::EffectWindow *w)
{
    KWin::EffectWindow *previousActive = m_activeWindow;
    m_activeWindow = w;

    applyOutlineState(previousActive);
    applyOutlineState(w);
}

void KWinOutlineEffect::applyOutlineState(KWin::EffectWindow *w)
{
    if (!w) {
        return;
    }
    auto it = m_renderers.find(w);
    if (it == m_renderers.end()) {
        return;
    }

    OutlineWindowRenderer &renderer = *it->second;
    if (m_suppressedByFullScreenEffect) {
        renderer.setVisible(false);
        return;
    }
    if (w == m_activeWindow) {
        renderer.setColor(m_settings->activeColor());
        renderer.setVisible(true);
    } else {
        renderer.setColor(m_settings->inactiveColor());
        renderer.setVisible(m_settings->drawInactive());
    }
}

void KWinOutlineEffect::applyOutlineStateToAll()
{
    for (auto &[w, renderer] : m_renderers) {
        applyOutlineState(w);
    }
}

void KWinOutlineEffect::handleWindowFullScreenChanged(KWin::EffectWindow *w)
{
    reevaluateWindow(w);
}

void KWinOutlineEffect::handleHasActiveFullScreenEffectChanged()
{
    m_suppressedByFullScreenEffect = KWin::effects->hasActiveFullScreenEffect();
    applyOutlineStateToAll();
}

} // namespace KWinOutline

KWIN_EFFECT_FACTORY(KWinOutline::KWinOutlineEffect, "metadata.json")

#include "kwinoutline.moc"
