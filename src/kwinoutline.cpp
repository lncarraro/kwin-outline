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

#include <kdecoration3/decoration.h>

#include <cstdarg>
#include <cstdio>

namespace KWinOutline
{

static void debugLog(const char *format, ...)
{
    FILE *file = std::fopen("/tmp/kwinoutline-debug.log", "a");
    if (!file) {
        return;
    }

    va_list args;
    va_start(args, format);
    std::vfprintf(file, format, args);
    va_end(args);
    std::fprintf(file, "\n");
    std::fclose(file);
}

__attribute__((constructor)) static void debugLibraryLoaded()
{
    debugLog("[DEBUG-kwinoutline-file] library-loaded");
}

static OutlinePlacement toOutlinePlacement(KWinOutlineSettings::EnumPlacement::type p)
{
    return static_cast<OutlinePlacement>(static_cast<int>(p));
}

static bool decorationProvidesOutline(const KWin::EffectWindow &w)
{
    if (!w.hasDecoration()) {
        return false;
    }
    const KDecoration3::Decoration *decoration = w.decoration();
    if (!decoration) {
        return false;
    }
    return !decoration->borderOutline().isNull();
}

KWinOutlineEffect::KWinOutlineEffect()
    : KWin::Effect()
    , m_settings(std::make_unique<KWinOutlineSettings>())
{
    m_activeWindow = KWin::effects->activeWindow();
    debugLog("[DEBUG-kwinoutline-file] effect-constructed activeWindow=%p", static_cast<void *>(m_activeWindow));

    connect(KWin::effects, &KWin::EffectsHandler::windowAdded, this, &KWinOutlineEffect::handleWindowAdded);
    connect(KWin::effects, &KWin::EffectsHandler::windowClosed, this, &KWinOutlineEffect::handleWindowClosed);
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this, &KWinOutlineEffect::handleWindowDeleted);
    connect(KWin::effects, &KWin::EffectsHandler::windowActivated, this, &KWinOutlineEffect::handleWindowActivated);
    connect(KWin::effects, &KWin::EffectsHandler::hasActiveFullScreenEffectChanged, this, &KWinOutlineEffect::handleHasActiveFullScreenEffectChanged);

    // Attach outlines to windows already open when the effect loads.
    const auto existingWindows = KWin::effects->stackingOrder();
    debugLog("[DEBUG-kwinoutline-file] initial-scan count=%zu", existingWindows.size());
    for (KWin::EffectWindow *w : existingWindows) {
        watchWindowLifetime(w);
        reevaluateWindow(w);
    }
}

KWinOutlineEffect::~KWinOutlineEffect()
{
    debugLog("[DEBUG-kwinoutline-file] destruct rendererCount=%zu", m_renderers.size());
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
    const auto oldDecorationPolicy = m_settings->existingDecorationOutlinePolicy();

    m_settings->load();
    debugLog("[DEBUG-kwinoutline-file] reconfigure thickness=%f placement=%d active=%s inactive=%s drawInactive=%d includeUtility=%d decorationPolicy=%d rendererCount=%zu",
             m_settings->thickness(),
             static_cast<int>(m_settings->placement()),
             qPrintable(m_settings->activeColor().name(QColor::HexArgb)),
             qPrintable(m_settings->inactiveColor().name(QColor::HexArgb)),
             m_settings->drawInactive(),
             m_settings->includeUtilityWindows(),
             static_cast<int>(m_settings->existingDecorationOutlinePolicy()),
             m_renderers.size());

    const bool includeUtilityChanged = (m_settings->includeUtilityWindows() != oldIncludeUtility);
    const bool decorationPolicyChanged = (m_settings->existingDecorationOutlinePolicy() != oldDecorationPolicy);
    const bool geometryChanged = !qFuzzyCompare(m_settings->thickness(), oldThickness)
                                 || (m_settings->placement() != oldPlacement);
    const bool colorStateChanged = (m_settings->activeColor() != oldActiveColor)
                                   || (m_settings->inactiveColor() != oldInactiveColor)
                                   || (m_settings->drawInactive() != oldDrawInactive);

    if (includeUtilityChanged || decorationPolicyChanged) {
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
    debugLog("[DEBUG-kwinoutline-file] window-added window=%p", static_cast<void *>(w));
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
        debugLog("[DEBUG-kwinoutline-file] reject window=%p managed=%d deleted=%d fullScreen=%d internal=%d popup=%d normal=%d dialog=%d utility=%d desktop=%d dock=%d frame=%fx%f",
                 static_cast<void *>(w),
                 snapshot.managed,
                 snapshot.deleted,
                 snapshot.fullScreen,
                 snapshot.internal,
                 snapshot.popupWindow,
                 snapshot.normalWindow,
                 snapshot.dialog,
                 snapshot.utility,
                 snapshot.desktop,
                 snapshot.dock,
                 w->frameGeometry().width(),
                 w->frameGeometry().height());
        removeWindow(w);
        return;
    }

    const auto policy = m_settings->existingDecorationOutlinePolicy();
    if (policy == KWinOutlineSettings::EnumExistingDecorationOutlinePolicy::SkipKnownDecorationOutline
        && decorationProvidesOutline(*w)) {
        debugLog("[DEBUG-kwinoutline-file] skip-decoration-outline window=%p hasDecoration=%d",
                 static_cast<void *>(w),
                 w->hasDecoration());
        removeWindow(w);
        return;
    }

    if (m_renderers.contains(w)) {
        debugLog("[DEBUG-kwinoutline-file] keep-existing window=%p frame=%fx%f",
                 static_cast<void *>(w),
                 w->frameGeometry().width(),
                 w->frameGeometry().height());
        return;
    }

    debugLog("[DEBUG-kwinoutline-file] create-renderer window=%p normal=%d dialog=%d utility=%d frame=%fx%f",
             static_cast<void *>(w),
             snapshot.normalWindow,
             snapshot.dialog,
             snapshot.utility,
             w->frameGeometry().width(),
             w->frameGeometry().height());

    auto renderer = std::make_unique<OutlineWindowRenderer>(*w);
    if (renderer->trackedWindowCount() == 0) {
        debugLog("[DEBUG-kwinoutline-file] renderer-empty window=%p frame=%fx%f",
                 static_cast<void *>(w),
                 w->frameGeometry().width(),
                 w->frameGeometry().height());
        return;
    }

    auto [it, inserted] = m_renderers.emplace(w, std::move(renderer));
    debugLog("[DEBUG-kwinoutline-file] renderer-inserted window=%p inserted=%d rendererCount=%zu",
             static_cast<void *>(w),
             inserted,
             m_renderers.size());
    it->second->setThicknessAndPlacement(
        m_settings->thickness(),
        toOutlinePlacement(m_settings->placement()),
        w->frameGeometry().size());
    applyOutlineState(w);
}

void KWinOutlineEffect::removeWindow(KWin::EffectWindow *w)
{
    debugLog("[DEBUG-kwinoutline-file] remove-window window=%p hadRenderer=%d",
             static_cast<void *>(w),
             m_renderers.contains(w));

    auto connIt = m_decorationOutlineConnections.find(w);
    if (connIt != m_decorationOutlineConnections.end()) {
        QObject::disconnect(connIt->second);
        m_decorationOutlineConnections.erase(connIt);
    }

    m_renderers.erase(w);
}

void KWinOutlineEffect::removeWindow(QObject *object)
{
    for (auto it = m_renderers.begin(); it != m_renderers.end();) {
        if (static_cast<QObject *>(it->first) == object) {
            debugLog("[DEBUG-kwinoutline-file] remove-object object=%p", static_cast<void *>(object));

            auto connIt = m_decorationOutlineConnections.find(it->first);
            if (connIt != m_decorationOutlineConnections.end()) {
                QObject::disconnect(connIt->second);
                m_decorationOutlineConnections.erase(connIt);
            }

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
    connect(w, &KWin::EffectWindow::windowDecorationChanged, this, &KWinOutlineEffect::handleWindowDecorationChanged, Qt::UniqueConnection);
    connectDecorationOutlineSignal(w);
}

void KWinOutlineEffect::connectDecorationOutlineSignal(KWin::EffectWindow *w)
{
    auto it = m_decorationOutlineConnections.find(w);
    if (it != m_decorationOutlineConnections.end()) {
        QObject::disconnect(it->second);
        m_decorationOutlineConnections.erase(it);
    }

    KDecoration3::Decoration *decoration = w->decoration();
    if (!decoration) {
        return;
    }

    auto conn = connect(decoration, &KDecoration3::Decoration::borderOutlineChanged, this, [this, w]() {
        debugLog("[DEBUG-kwinoutline-file] border-outline-changed window=%p", static_cast<void *>(w));
        reevaluateWindow(w);
    });
    m_decorationOutlineConnections.emplace(w, conn);
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
        debugLog("[DEBUG-kwinoutline-file] set-state suppressed window=%p", static_cast<void *>(w));
        renderer.setVisible(false);
        return;
    }
    if (w == m_activeWindow) {
        debugLog("[DEBUG-kwinoutline-file] set-state active window=%p color=%s",
                 static_cast<void *>(w),
                 qPrintable(m_settings->activeColor().name(QColor::HexArgb)));
        renderer.setColor(m_settings->activeColor());
        renderer.setVisible(true);
    } else {
        debugLog("[DEBUG-kwinoutline-file] set-state inactive window=%p color=%s visible=%d",
                 static_cast<void *>(w),
                 qPrintable(m_settings->inactiveColor().name(QColor::HexArgb)),
                 m_settings->drawInactive());
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

void KWinOutlineEffect::handleWindowDecorationChanged(KWin::EffectWindow *w)
{
    debugLog("[DEBUG-kwinoutline-file] decoration-changed window=%p hasDecoration=%d providesOutline=%d",
             static_cast<void *>(w),
             w->hasDecoration(),
             decorationProvidesOutline(*w));
    connectDecorationOutlineSignal(w);
    reevaluateWindow(w);
}

void KWinOutlineEffect::handleHasActiveFullScreenEffectChanged()
{
    m_suppressedByFullScreenEffect = KWin::effects->hasActiveFullScreenEffect();
    debugLog("[DEBUG-kwinoutline-file] fullscreen-effect-changed suppressed=%d", m_suppressedByFullScreenEffect);
    applyOutlineStateToAll();
}

} // namespace KWinOutline

KWIN_EFFECT_FACTORY(KWinOutline::KWinOutlineEffect, "metadata.json")

#include "kwinoutline.moc"
