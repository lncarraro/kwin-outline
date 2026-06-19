// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "windoweligibility.h"

namespace KWinOutline
{

bool isWindowEligibleForOutline(const WindowEligibilitySnapshot &window, const WindowEligibilityOptions &options)
{
    if (!window.managed || window.deleted || window.fullScreen) {
        return false;
    }

    if (window.internal || window.lockScreen || window.inputMethod || window.popupWindow || window.outline) {
        return false;
    }

    if (window.desktop || window.dock || window.toolbar || window.menu || window.splash || window.dropdownMenu || window.popupMenu
        || window.tooltip || window.notification || window.criticalNotification || window.appletPopup || window.onScreenDisplay
        || window.comboBox || window.dndIcon) {
        return false;
    }

    return window.normalWindow || window.dialog || (options.includeUtilityWindows && window.utility);
}

} // namespace KWinOutline
