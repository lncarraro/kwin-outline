// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "outlinewindowrenderer.h"

#include "kwinoutlinelog.h"

// KWin scene headers are isolated here. WindowItem::windowContainer() is used for
// stacking: the outline item is placed after the window container so it renders on
// top of the window content without being clipped by the container's border radius.
#include <effect/effectwindow.h>
#include <scene/borderoutline.h>
#include <scene/borderradius.h>
#include <scene/outlinedborderitem.h>
#include <scene/windowitem.h>

#include <kdecoration3/decoration.h>

#include <algorithm>

namespace KWinOutline
{

OutlineWindowRenderer::OutlineWindowRenderer(KWin::EffectWindow &window)
{
    KWin::WindowItem *windowItem = window.windowItem();
    if (!windowItem) {
        warnLog("renderer-create-failed: no WindowItem for window=%p frame=%fx%f",
                static_cast<void *>(&window),
                window.frameGeometry().width(),
                window.frameGeometry().height());
        return;
    }

    const QSizeF frameSize = window.frameGeometry().size();
    const QRectF localFrame(0.0, 0.0, frameSize.width(), frameSize.height());
    const OutlinePlacementGeometry geom = computeOutlineGeometry(localFrame, m_thickness, OutlinePlacement::Inside);

    const KWin::BorderOutline outline(m_thickness, m_color, computeInnerRadius());
    m_outlineItem = std::make_unique<KWin::OutlinedBorderItem>(KWin::RectF(geom.innerRect), outline, windowItem);
    m_outlineItem->stackAfter(windowItem->windowContainer());

    debugLog("renderer-created: window=%p frame=%fx%f", static_cast<void *>(&window), frameSize.width(), frameSize.height());
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

void OutlineWindowRenderer::refreshWindowRadius(KWin::EffectWindow &window)
{
    bool useExplicitInnerRadius = false;
    KWin::BorderRadius explicitInnerRadius;
    KWin::BorderRadius outerRadius;

    // SSD: if the decoration exposes an outline, its radius already has the
    // same inner-edge semantics that KWin::OutlinedBorderItem expects.
    const KDecoration3::Decoration *decoration = window.decoration();
    if (decoration) {
        const KWin::BorderOutline decorationOutline = KWin::BorderOutline::from(decoration->borderOutline());
        if (!decorationOutline.isNull()) {
            useExplicitInnerRadius = true;
            explicitInnerRadius = decorationOutline.radius();
        } else {
            outerRadius = KWin::BorderRadius::from(decoration->borderRadius());
        }
    } else {
        // CSD/undecorated: use whatever KWin's scene has computed for this item.
        const KWin::WindowItem *item = window.windowItem();
        if (item) {
            outerRadius = item->borderRadius();
        }
    }

    if (useExplicitInnerRadius == m_useExplicitInnerRadius
        && explicitInnerRadius == m_explicitInnerRadius
        && outerRadius == m_windowOuterRadius) {
        return;
    }
    m_useExplicitInnerRadius = useExplicitInnerRadius;
    m_explicitInnerRadius = explicitInnerRadius;
    m_windowOuterRadius = outerRadius;
    updateOutline();
}

KWin::BorderRadius OutlineWindowRenderer::computeInnerRadiusFromOuterRadius(const KWin::BorderRadius &outerRadius) const
{
    // The outline's inner rect is inset from the frame outer edge by `inset` on each side.
    // BorderOutline's radius field is the inner-edge corner radius. The outer-edge corner
    // radius = innerRadius + thickness. So innerRadius = outerRadius - inset.
    const qreal inset = (m_placement == OutlinePlacement::Inside) ? m_thickness
                      : (m_placement == OutlinePlacement::Centered) ? m_thickness / 2.0
                      : 0.0;
    return KWin::BorderRadius(
        std::max(0.0, outerRadius.topLeft() - inset),
        std::max(0.0, outerRadius.topRight() - inset),
        std::max(0.0, outerRadius.bottomRight() - inset),
        std::max(0.0, outerRadius.bottomLeft() - inset));
}

KWin::BorderRadius OutlineWindowRenderer::computeInnerRadius() const
{
    if (m_useExplicitInnerRadius) {
        return m_explicitInnerRadius;
    }
    return computeInnerRadiusFromOuterRadius(m_windowOuterRadius);
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

void OutlineWindowRenderer::setThicknessAndPlacement(qreal thickness, OutlinePlacement placement, const QSizeF &frameSize)
{
    if (!m_outlineItem) {
        return;
    }
    const bool thicknessChanged = !qFuzzyCompare(m_thickness, thickness);
    const bool placementChanged = (m_placement != placement);
    if (!thicknessChanged && !placementChanged) {
        return;
    }
    m_thickness = thickness;
    m_placement = placement;
    updateOutline();
    const QRectF localFrame(0.0, 0.0, frameSize.width(), frameSize.height());
    const OutlinePlacementGeometry geom = computeOutlineGeometry(localFrame, m_thickness, m_placement);
    m_outlineItem->setInnerRect(KWin::RectF(geom.innerRect));
}

void OutlineWindowRenderer::updateOutline()
{
    if (m_outlineItem) {
        const KWin::BorderRadius innerRadius = computeInnerRadius();
        m_outlineItem->setOutline(KWin::BorderOutline(m_thickness, m_color, innerRadius));
    }
}

} // namespace KWinOutline
