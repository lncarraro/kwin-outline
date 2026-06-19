# SPDX-FileCopyrightText: 2024 kwin-outline contributors
# SPDX-License-Identifier: GPL-2.0-or-later

kcoreaddons_add_plugin(kwinoutline
    SOURCES
        kwinoutline.cpp
    INSTALL_NAMESPACE "kwin/effects/plugins"
)

target_link_libraries(kwinoutline PRIVATE
    KWin::kwin
    Qt6::Core
    Qt6::Gui
)
