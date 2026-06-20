// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "kwinoutlinesettings.h"

#include <KCModule>
#include <KPluginMetaData>

class KColorButton;
class QCheckBox;
class QLabel;

namespace KWinOutline
{

class KWinOutlineConfigModule : public KCModule
{
    Q_OBJECT

public:
    explicit KWinOutlineConfigModule(QObject *parent, const KPluginMetaData &data);

    void load() override;
    void save() override;

private:
    KWinOutlineSettings m_settings;
    KColorButton *m_inactiveColorButton = nullptr;
    QLabel *m_inactiveColorLabel = nullptr;
    QCheckBox *m_drawInactiveCheck = nullptr;
};

} // namespace KWinOutline
