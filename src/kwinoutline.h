// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "outlinewindowrenderer.h"

#include <effect/effect.h>

#include <map>
#include <memory>

namespace KWin
{
class EffectWindow;
}

namespace KWinOutline
{

class KWinOutlineEffect : public KWin::Effect
{
    Q_OBJECT

public:
    KWinOutlineEffect();
    ~KWinOutlineEffect() override;

    bool blocksDirectScanout() const override;

private Q_SLOTS:
    void handleWindowAdded(KWin::EffectWindow *w);
    void handleWindowDeleted(KWin::EffectWindow *w);

private:
    void addWindow(KWin::EffectWindow *w);

    // Dev hook (Phase 07): minimal per-window renderer map.
    // Phase 09 replaces this with full registry lifecycle.
    std::map<KWin::EffectWindow *, std::unique_ptr<OutlineWindowRenderer>> m_renderers;
};

} // namespace KWinOutline
