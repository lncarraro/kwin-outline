// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "windoweligibility.h"

namespace KWin
{
class EffectWindow;
}

namespace KWinOutline
{

WindowEligibilitySnapshot snapshotWindowEligibility(const KWin::EffectWindow &window);

} // namespace KWinOutline
