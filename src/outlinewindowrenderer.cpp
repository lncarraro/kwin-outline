// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "outlinewindowrenderer.h"

// KWin scene headers are isolated here. WindowItem::windowContainer() is used for
// stacking: the outline item is placed after the window container so it renders on
// top of the window content without being clipped by the container's border radius.
#include <effect/effectwindow.h>
#include <scene/borderoutline.h>
#include <scene/borderradius.h>
#include <scene/outlinedborderitem.h>
#include <scene/windowitem.h>

namespace KWinOutline
{

OutlineWindowRenderer::OutlineWindowRenderer(KWin::EffectWindow &window)
{
    KWin::WindowItem *windowItem = window.windowItem();
    if (!windowItem) {
        return;
    }

    const QSizeF frameSize = window.frameGeometry().size();
    const QRectF localFrame(0.0, 0.0, frameSize.width(), frameSize.height());
    const OutlinePlacementGeometry geom = computeOutlineGeometry(localFrame, m_thickness, OutlinePlacement::Inside);

    const KWin::BorderOutline outline(m_thickness, m_color, KWin::BorderRadius(m_cornerRadius));
    m_outlineItem = std::make_unique<KWin::OutlinedBorderItem>(KWin::RectF(geom.innerRect), outline, windowItem);
    m_outlineItem->stackAfter(windowItem->windowContainer());
}

OutlineWindowRenderer::~OutlineWindowRenderer() = default;

void OutlineWindowRenderer::setGeometry(const OutlinePlacementGeometry &geometry)
{
    if (!m_outlineItem) {
        return;
    }

    // Thickness is the inset from outerRect to innerRect on any side.
    const qreal newThickness = geometry.innerRect.x() - geometry.outerRect.x();
    if (newThickness != m_thickness) {
        m_thickness = newThickness;
        updateOutline();
    }

    m_outlineItem->setInnerRect(KWin::RectF(geometry.innerRect));
}

void OutlineWindowRenderer::setColor(const QColor &color)
{
    if (m_color == color) {
        return;
    }
    m_color = color;
    updateOutline();
}

void OutlineWindowRenderer::setVisible(bool visible)
{
    if (m_outlineItem) {
        m_outlineItem->setVisible(visible);
    }
}

void OutlineWindowRenderer::setCornerRadius(qreal radius)
{
    if (qFuzzyCompare(m_cornerRadius, radius)) {
        return;
    }
    m_cornerRadius = radius;
    updateOutline();
}

bool OutlineWindowRenderer::isVisible() const
{
    return m_outlineItem && m_outlineItem->isVisible();
}

int OutlineWindowRenderer::trackedWindowCount() const
{
    return m_outlineItem ? 1 : 0;
}

void OutlineWindowRenderer::updateFrameSize(const QSizeF &frameSize)
{
    if (!m_outlineItem) {
        return;
    }
    const QRectF localFrame(0.0, 0.0, frameSize.width(), frameSize.height());
    setGeometry(computeOutlineGeometry(localFrame, m_thickness, m_placement));
}

void OutlineWindowRenderer::updateOutline()
{
    if (m_outlineItem) {
        m_outlineItem->setOutline(KWin::BorderOutline(m_thickness, m_color, KWin::BorderRadius(m_cornerRadius)));
    }
}

} // namespace KWinOutline
