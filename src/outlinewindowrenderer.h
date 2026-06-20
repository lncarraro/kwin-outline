// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "outlinegeometry.h"

#include <scene/borderradius.h>

#include <QColor>
#include <QSizeF>
#include <memory>

namespace KWin
{
class EffectWindow;
class OutlinedBorderItem;
}

namespace KWinOutline
{

// Per-window outline handle. KWin scene types (OutlinedBorderItem) are isolated
// in the .cpp. BorderRadius is a lightweight value type exposed here so callers
// can avoid a second scene header pull-in when they already have windowItem().
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

    // Reads the current window/decoration corner radius and applies it.
    // Call on decoration changes and on initial renderer creation.
    void refreshWindowRadius(KWin::EffectWindow &window);

    // Recomputes and applies outline geometry from the new frame size using
    // stored thickness and placement. Call on windowFrameGeometryChanged.
    void updateFrameSize(const QSizeF &frameSize);

    // Updates thickness and placement together and recomputes geometry.
    // No-op if both values are unchanged.
    void setThicknessAndPlacement(qreal thickness, OutlinePlacement placement, const QSizeF &frameSize);

    bool isVisible() const;
    int trackedWindowCount() const;

private:
    // Returns the inner-edge BorderRadius to pass to BorderOutline, derived from
    // m_windowOuterRadius, m_placement, and m_thickness.
    KWin::BorderRadius computeInnerRadius() const;
    void updateOutline();

    std::unique_ptr<KWin::OutlinedBorderItem> m_outlineItem;
    qreal m_thickness = 1.0;
    OutlinePlacement m_placement = OutlinePlacement::Inside;
    QColor m_color{QStringLiteral("#3daee9")};
    KWin::BorderRadius m_windowOuterRadius; // outer corner radius of the window frame; zero = rectangular
};

} // namespace KWinOutline
