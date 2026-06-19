// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <effect/effect.h>

namespace KWinOutline
{

class KWinOutlineEffect : public KWin::Effect
{
    Q_OBJECT

public:
    KWinOutlineEffect();
    ~KWinOutlineEffect() override;

    bool blocksDirectScanout() const override;
};

} // namespace KWinOutline
