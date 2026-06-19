// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kwinoutline.h"

#include <effect/effect.h>

namespace KWinOutline
{

KWinOutlineEffect::KWinOutlineEffect()
    : KWin::Effect()
{
}

KWinOutlineEffect::~KWinOutlineEffect() = default;

bool KWinOutlineEffect::blocksDirectScanout() const
{
    return false;
}

} // namespace KWinOutline

KWIN_EFFECT_FACTORY(KWinOutline::KWinOutlineEffect, "metadata.json")

#include "kwinoutline.moc"
