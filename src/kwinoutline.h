// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "outlinewindowrenderer.h"

#include <effect/effect.h>

#include <map>
#include <memory>

class QObject;

namespace KWin
{
class EffectWindow;
class RectF;
}

namespace KWinOutline
{

class KWinOutlineSettings;

class KWinOutlineEffect : public KWin::Effect
{
    Q_OBJECT

public:
    KWinOutlineEffect();
    ~KWinOutlineEffect() override;

    bool blocksDirectScanout() const override;

private Q_SLOTS:
    void handleWindowAdded(KWin::EffectWindow *w);
    void handleWindowClosed(KWin::EffectWindow *w);
    void handleWindowDeleted(KWin::EffectWindow *w);
    void handleWindowDestroyed(QObject *object);
    void handleWindowFrameGeometryChanged(KWin::EffectWindow *w, const KWin::RectF &oldGeometry);
    void handleWindowActivated(KWin::EffectWindow *w);
    void handleWindowFullScreenChanged(KWin::EffectWindow *w);
    void handleHasActiveFullScreenEffectChanged();

private:
    void reevaluateWindow(KWin::EffectWindow *w);
    void removeWindow(KWin::EffectWindow *w);
    void removeWindow(QObject *object);
    void watchWindowLifetime(KWin::EffectWindow *w);
    void applyOutlineState(KWin::EffectWindow *w);
    void applyOutlineStateToAll();

    std::unique_ptr<KWinOutlineSettings> m_settings;
    KWin::EffectWindow *m_activeWindow = nullptr;
    bool m_suppressedByFullScreenEffect = false;
    std::map<KWin::EffectWindow *, std::unique_ptr<OutlineWindowRenderer>> m_renderers;
};

} // namespace KWinOutline
