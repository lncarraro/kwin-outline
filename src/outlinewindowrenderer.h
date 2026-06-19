// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "outlinegeometry.h"

#include <QColor>
#include <memory>

namespace KWin
{
class EffectWindow;
class OutlinedBorderItem;
}

namespace KWinOutline
{

// Per-window outline handle. KWin scene types (WindowItem, OutlinedBorderItem)
// are used only in the .cpp. Callers depend only on domain types.
class OutlineWindowRenderer
{
public:
    explicit OutlineWindowRenderer(KWin::EffectWindow &window);
    ~OutlineWindowRenderer();

    OutlineWindowRenderer(const OutlineWindowRenderer &) = delete;
    OutlineWindowRenderer &operator=(const OutlineWindowRenderer &) = delete;

    // geometry must be computed in WindowItem-local coordinates (frame origin at 0,0).
    void setGeometry(const OutlinePlacementGeometry &geometry);
    void setColor(const QColor &color);
    void setVisible(bool visible);
    void setCornerRadius(qreal radius);

    bool isVisible() const;
    int trackedWindowCount() const;

private:
    void updateOutline();

    std::unique_ptr<KWin::OutlinedBorderItem> m_outlineItem;
    qreal m_thickness = 1.0;
    QColor m_color{QStringLiteral("#3daee9")};
    qreal m_cornerRadius = 0.0;
};

} // namespace KWinOutline
