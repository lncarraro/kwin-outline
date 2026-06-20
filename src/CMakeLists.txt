# SPDX-FileCopyrightText: 2024 kwin-outline contributors
# SPDX-License-Identifier: GPL-2.0-or-later

kcoreaddons_add_plugin(kwinoutline
    SOURCES
        kwinwindoweligibility.cpp
        kwinoutline.cpp
        outlinegeometry.cpp
        outlinewindowrenderer.cpp
        windoweligibility.cpp
    INSTALL_NAMESPACE "kwin/effects/plugins"
)

add_library(kwinoutline_settings STATIC)
set_target_properties(kwinoutline_settings PROPERTIES POSITION_INDEPENDENT_CODE ON)
kconfig_add_kcfg_files(kwinoutline_settings kwinoutlinesettings.kcfgc)
target_include_directories(kwinoutline_settings PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(kwinoutline_settings PUBLIC
    KF6::ConfigGui
    Qt6::Gui
)

target_link_libraries(kwinoutline PRIVATE
    kwinoutline_settings
    KWin::kwin
    KDecoration3::KDecoration
    Qt6::Core
    Qt6::Gui
)

install(FILES kwinoutlinesettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
