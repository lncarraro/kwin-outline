// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace KWinOutline
{

struct WindowEligibilityOptions {
    bool includeUtilityWindows = false;
};

struct WindowEligibilitySnapshot {
    bool managed = false;
    bool deleted = false;
    bool fullScreen = false;

    bool internal = false;
    bool lockScreen = false;
    bool inputMethod = false;
    bool popupWindow = false;
    bool outline = false;

    bool desktop = false;
    bool dock = false;
    bool toolbar = false;
    bool menu = false;
    bool splash = false;
    bool dropdownMenu = false;
    bool popupMenu = false;
    bool tooltip = false;
    bool notification = false;
    bool criticalNotification = false;
    bool appletPopup = false;
    bool onScreenDisplay = false;
    bool comboBox = false;
    bool dndIcon = false;

    bool normalWindow = false;
    bool dialog = false;
    bool utility = false;
};

bool isWindowEligibleForOutline(const WindowEligibilitySnapshot &window, const WindowEligibilityOptions &options);

} // namespace KWinOutline
