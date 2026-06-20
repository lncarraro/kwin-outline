// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kwinoutline_config.h"

#include <KColorButton>
#include <KPluginFactory>

#include <QCheckBox>
#include <QComboBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

K_PLUGIN_CLASS_WITH_JSON(KWinOutline::KWinOutlineConfigModule, "kwinoutline_config.json")

namespace KWinOutline
{

KWinOutlineConfigModule::KWinOutlineConfigModule(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    auto *mainLayout = new QVBoxLayout(widget());
    auto *form = new QFormLayout;
    mainLayout->addLayout(form);
    mainLayout->addStretch();

    auto *thicknessSpinBox = new QDoubleSpinBox(widget());
    thicknessSpinBox->setObjectName(QStringLiteral("kcfg_Thickness"));
    thicknessSpinBox->setRange(0.5, 8.0);
    thicknessSpinBox->setSingleStep(0.1);
    thicknessSpinBox->setDecimals(1);
    thicknessSpinBox->setSuffix(QStringLiteral(" px"));
    form->addRow(tr("Thickness:"), thicknessSpinBox);

    auto *placementCombo = new QComboBox(widget());
    placementCombo->setObjectName(QStringLiteral("kcfg_Placement"));
    placementCombo->addItem(tr("Inside"));
    placementCombo->addItem(tr("Centered"));
    placementCombo->addItem(tr("Outside"));
    form->addRow(tr("Placement:"), placementCombo);

    auto *activeColorButton = new KColorButton(widget());
    activeColorButton->setObjectName(QStringLiteral("kcfg_ActiveColor"));
    activeColorButton->setAlphaChannelEnabled(true);
    form->addRow(tr("Active color:"), activeColorButton);

    m_drawInactiveCheck = new QCheckBox(tr("Draw outline for inactive windows"), widget());
    m_drawInactiveCheck->setObjectName(QStringLiteral("kcfg_DrawInactive"));
    form->addRow(m_drawInactiveCheck);

    m_inactiveColorLabel = new QLabel(tr("Inactive color:"), widget());
    m_inactiveColorButton = new KColorButton(widget());
    m_inactiveColorButton->setObjectName(QStringLiteral("kcfg_InactiveColor"));
    m_inactiveColorButton->setAlphaChannelEnabled(true);
    form->addRow(m_inactiveColorLabel, m_inactiveColorButton);

    connect(m_drawInactiveCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        m_inactiveColorButton->setEnabled(enabled);
        m_inactiveColorLabel->setEnabled(enabled);
    });

    auto *separator = new QFrame(widget());
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    form->addRow(separator);

    auto *policyHeader = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Window policy")), widget());
    form->addRow(policyHeader);

    auto *includeUtilityCheck = new QCheckBox(tr("Include utility windows"), widget());
    includeUtilityCheck->setObjectName(QStringLiteral("kcfg_IncludeUtilityWindows"));
    form->addRow(includeUtilityCheck);

    auto *decorationPolicyCombo = new QComboBox(widget());
    decorationPolicyCombo->setObjectName(QStringLiteral("kcfg_ExistingDecorationOutlinePolicy"));
    decorationPolicyCombo->addItem(tr("Always draw"));
    decorationPolicyCombo->addItem(tr("Skip known decoration outline"));
    form->addRow(tr("Decoration outline:"), decorationPolicyCombo);

    auto *noteLabel = new QLabel(
        tr("Note: Client-painted borders inside application content cannot be detected. "
           "If your decoration theme draws its own outline, you may need to disable it manually for a consistent appearance."),
        widget());
    noteLabel->setWordWrap(true);
    form->addRow(noteLabel);

    addConfig(&m_settings, widget());
}

void KWinOutlineConfigModule::load()
{
    KCModule::load();
    bool drawInactive = m_drawInactiveCheck->isChecked();
    m_inactiveColorButton->setEnabled(drawInactive);
    m_inactiveColorLabel->setEnabled(drawInactive);
}

void KWinOutlineConfigModule::save()
{
    KCModule::save();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QStringLiteral("reconfigureEffect"));
    msg << QStringLiteral("kwinoutline");
    QDBusConnection::sessionBus().send(msg);
}

} // namespace KWinOutline

#include "kwinoutline_config.moc"
