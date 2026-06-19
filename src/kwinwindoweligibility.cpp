// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kwinwindoweligibility.h"

#include <effect/effectwindow.h>

namespace KWinOutline
{

WindowEligibilitySnapshot snapshotWindowEligibility(const KWin::EffectWindow &window)
{
    return {
        .managed = window.isManaged(),
        .deleted = window.isDeleted(),
        .fullScreen = window.isFullScreen(),
        .internal = window.internalWindow() != nullptr,
        .lockScreen = window.isLockScreen(),
        .inputMethod = window.isInputMethod(),
        .popupWindow = window.isPopupWindow(),
        .outline = window.isOutline(),
        .desktop = window.isDesktop(),
        .dock = window.isDock(),
        .toolbar = window.isToolbar(),
        .menu = window.isMenu(),
        .splash = window.isSplash(),
        .dropdownMenu = window.isDropdownMenu(),
        .popupMenu = window.isPopupMenu(),
        .tooltip = window.isTooltip(),
        .notification = window.isNotification(),
        .criticalNotification = window.isCriticalNotification(),
        .appletPopup = window.isAppletPopup(),
        .onScreenDisplay = window.isOnScreenDisplay(),
        .comboBox = window.isComboBox(),
        .dndIcon = window.isDNDIcon(),
        .normalWindow = window.isNormalWindow(),
        .dialog = window.isDialog(),
        .utility = window.isUtility(),
    };
}

} // namespace KWinOutline
