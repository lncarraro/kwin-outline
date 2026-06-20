// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kwinoutline.h"

#include "kwinoutlinelog.h"
#include "kwinoutlinesettings.h"
#include "kwinwindoweligibility.h"
#include "outlinegeometry.h"
#include "windoweligibility.h"

#include <effect/effect.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>

#include <kdecoration3/decoration.h>

#include <QString>

namespace KWinOutline
{

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
        warnLog("decoration-policy-unavailable: window=%p hasDecoration=true but decoration() is null; treating as no outline",
                static_cast<const void *>(&w));
        return false;
    }
    return !decoration->borderOutline().isNull();
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

    const auto existingWindows = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *w : existingWindows) {
        watchWindowLifetime(w);
        reevaluateWindow(w);
    }

    infoLog("effect-init: existingWindows=%lld trackedRenderers=%lld activeWindow=%p",
            static_cast<long long>(existingWindows.size()),
            static_cast<long long>(m_renderers.size()),
            static_cast<void *>(m_activeWindow));
}

KWinOutlineEffect::~KWinOutlineEffect()
{
    infoLog("effect-teardown: renderers=%lld", static_cast<long long>(m_renderers.size()));
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

    infoLog("reconfigure: thickness=%f placement=%d activeColor=%s inactiveColor=%s drawInactive=%d includeUtility=%d decorationPolicy=%d",
            m_settings->thickness(),
            static_cast<int>(m_settings->placement()),
            qPrintable(m_settings->activeColor().name(QColor::HexArgb)),
            qPrintable(m_settings->inactiveColor().name(QColor::HexArgb)),
            m_settings->drawInactive(),
            m_settings->includeUtilityWindows(),
            static_cast<int>(m_settings->existingDecorationOutlinePolicy()));

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

QString KWinOutlineEffect::debug(const QString &) const
{
    const auto *s = m_settings.get();

    const QString placement = (s->placement() == KWinOutlineSettings::EnumPlacement::Inside) ? QStringLiteral("Inside")
                            : (s->placement() == KWinOutlineSettings::EnumPlacement::Centered) ? QStringLiteral("Centered")
                            : QStringLiteral("Outside");

    const QString decorationPolicy =
        (s->existingDecorationOutlinePolicy() == KWinOutlineSettings::EnumExistingDecorationOutlinePolicy::AlwaysDraw)
        ? QStringLiteral("AlwaysDraw")
        : QStringLiteral("SkipKnownDecorationOutline");

    int visibleCount = 0;
    for (const auto &[w, renderer] : m_renderers) {
        if (renderer->isVisible()) {
            ++visibleCount;
        }
    }

    return QStringLiteral(
               "backend: OpenGL compositing\n"
               "config: thickness=%1 placement=%2 activeColor=%3 inactiveColor=%4 drawInactive=%5 includeUtility=%6 decorationPolicy=%7\n"
               "tracked windows: %8\n"
               "visible outlines: %9\n"
               "suppression: %10")
        .arg(s->thickness())
        .arg(placement)
        .arg(s->activeColor().name(QColor::HexArgb))
        .arg(s->inactiveColor().name(QColor::HexArgb))
        .arg(s->drawInactive() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(s->includeUtilityWindows() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(decorationPolicy)
        .arg(static_cast<qsizetype>(m_renderers.size()))
        .arg(visibleCount)
        .arg(m_suppressedByFullScreenEffect ? QStringLiteral("active") : QStringLiteral("inactive"));
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

    const auto policy = m_settings->existingDecorationOutlinePolicy();
    if (policy == KWinOutlineSettings::EnumExistingDecorationOutlinePolicy::SkipKnownDecorationOutline
        && decorationProvidesOutline(*w)) {
        debugLog("skip-decoration-outline: window=%p hasDecoration=%d",
                 static_cast<void *>(w),
                 w->hasDecoration());
        removeWindow(w);
        return;
    }

    if (m_renderers.contains(w)) {
        return;
    }

    auto renderer = std::make_unique<OutlineWindowRenderer>(*w);
    if (renderer->trackedWindowCount() == 0) {
        warnLog("renderer-create-failed: no scene item for window=%p frame=%fx%f",
                static_cast<void *>(w),
                w->frameGeometry().width(),
                w->frameGeometry().height());
        return;
    }

    auto [it, inserted] = m_renderers.emplace(w, std::move(renderer));
    it->second->setThicknessAndPlacement(
        m_settings->thickness(),
        toOutlinePlacement(m_settings->placement()),
        w->frameGeometry().size());
    it->second->refreshWindowRadius(*w);
    applyOutlineState(w);
}

void KWinOutlineEffect::removeWindow(KWin::EffectWindow *w)
{
    auto connIt = m_decorationOutlineConnections.find(w);
    if (connIt != m_decorationOutlineConnections.end()) {
        QObject::disconnect(connIt->second);
        m_decorationOutlineConnections.erase(connIt);
    }

    auto radiusIt = m_decorationRadiusConnections.find(w);
    if (radiusIt != m_decorationRadiusConnections.end()) {
        QObject::disconnect(radiusIt->second);
        m_decorationRadiusConnections.erase(radiusIt);
    }

    m_renderers.erase(w);
}

void KWinOutlineEffect::removeWindow(QObject *object)
{
    for (auto it = m_renderers.begin(); it != m_renderers.end();) {
        if (static_cast<QObject *>(it->first) == object) {
            auto connIt = m_decorationOutlineConnections.find(it->first);
            if (connIt != m_decorationOutlineConnections.end()) {
                QObject::disconnect(connIt->second);
                m_decorationOutlineConnections.erase(connIt);
            }

            auto radiusIt = m_decorationRadiusConnections.find(it->first);
            if (radiusIt != m_decorationRadiusConnections.end()) {
                QObject::disconnect(radiusIt->second);
                m_decorationRadiusConnections.erase(radiusIt);
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
    connectDecorationSignals(w);
}

void KWinOutlineEffect::connectDecorationSignals(KWin::EffectWindow *w)
{
    // Disconnect any previous decoration signal connections for this window.
    auto outlineIt = m_decorationOutlineConnections.find(w);
    if (outlineIt != m_decorationOutlineConnections.end()) {
        QObject::disconnect(outlineIt->second);
        m_decorationOutlineConnections.erase(outlineIt);
    }
    auto radiusIt = m_decorationRadiusConnections.find(w);
    if (radiusIt != m_decorationRadiusConnections.end()) {
        QObject::disconnect(radiusIt->second);
        m_decorationRadiusConnections.erase(radiusIt);
    }

    KDecoration3::Decoration *decoration = w->decoration();
    if (!decoration) {
        return;
    }

    auto outlineConn = connect(decoration, &KDecoration3::Decoration::borderOutlineChanged, this, [this, w]() {
        reevaluateWindow(w);
    });
    m_decorationOutlineConnections.emplace(w, outlineConn);

    auto radiusConn = connect(decoration, &KDecoration3::Decoration::borderRadiusChanged, this, [this, w]() {
        updateWindowRadius(w);
    });
    m_decorationRadiusConnections.emplace(w, radiusConn);
}

void KWinOutlineEffect::updateWindowRadius(KWin::EffectWindow *w)
{
    auto it = m_renderers.find(w);
    if (it == m_renderers.end()) {
        return;
    }
    it->second->refreshWindowRadius(*w);
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

void KWinOutlineEffect::handleWindowDecorationChanged(KWin::EffectWindow *w)
{
    debugLog("decoration-changed: window=%p hasDecoration=%d providesOutline=%d",
             static_cast<void *>(w),
             w->hasDecoration(),
             decorationProvidesOutline(*w));
    connectDecorationSignals(w);
    reevaluateWindow(w);
    // reevaluateWindow only creates a renderer if one is absent. For existing
    // renderers, refresh the radius from the new decoration.
    updateWindowRadius(w);
}

void KWinOutlineEffect::handleHasActiveFullScreenEffectChanged()
{
    m_suppressedByFullScreenEffect = KWin::effects->hasActiveFullScreenEffect();
    debugLog("suppression-changed: suppressed=%d", m_suppressedByFullScreenEffect);
    applyOutlineStateToAll();
}

} // namespace KWinOutline

KWIN_EFFECT_FACTORY_SUPPORTED(KWinOutline::KWinOutlineEffect,
                              "metadata.json",
                              if (!KWin::effects->isOpenGLCompositing()) {
                                  KWinOutline::warnLog("isSupported: OpenGL compositor required but current backend is not OpenGL; effect will not load");
                                  return false;
                              }
                              return true;)

#include "kwinoutline.moc"
