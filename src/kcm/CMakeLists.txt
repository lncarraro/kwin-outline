# SPDX-FileCopyrightText: 2024 kwin-outline contributors
# SPDX-License-Identifier: GPL-2.0-or-later

kcoreaddons_add_plugin(kwinoutline_config
    SOURCES kwinoutline_config.cpp
    INSTALL_NAMESPACE "kwin/effects/configs"
)

target_link_libraries(kwinoutline_config PRIVATE
    kwinoutline_settings
    KF6::KCMUtils
    KF6::WidgetsAddons
    Qt6::Widgets
    Qt6::DBus
)
