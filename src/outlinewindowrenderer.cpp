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

#include <kdecoration3/decoration.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>

namespace KWinOutline
{

static void debugLog(const char *format, ...)
{
    FILE *file = std::fopen("/tmp/kwinoutline-debug.log", "a");
    if (!file) {
        return;
    }

    va_list args;
    va_start(args, format);
    std::vfprintf(file, format, args);
    va_end(args);
    std::fprintf(file, "\n");
    std::fclose(file);
}

OutlineWindowRenderer::OutlineWindowRenderer(KWin::EffectWindow &window)
{
    KWin::WindowItem *windowItem = window.windowItem();
    if (!windowItem) {
        debugLog("[DEBUG-kwinoutline-file] renderer-ctor no-window-item window=%p frame=%fx%f",
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
    debugLog("[DEBUG-kwinoutline-file] renderer-ctor created window=%p windowItem=%p container=%p frame=%fx%f inner=%f,%f,%f,%f itemRect=%f,%f,%f,%f bounding=%f,%f,%f,%f visible=%d color=%s thickness=%f",
             static_cast<void *>(&window),
             static_cast<void *>(windowItem),
             static_cast<void *>(windowItem->windowContainer()),
             frameSize.width(),
             frameSize.height(),
             geom.innerRect.x(),
             geom.innerRect.y(),
             geom.innerRect.width(),
             geom.innerRect.height(),
             m_outlineItem->rect().x(),
             m_outlineItem->rect().y(),
             m_outlineItem->rect().width(),
             m_outlineItem->rect().height(),
             m_outlineItem->boundingRect().x(),
             m_outlineItem->boundingRect().y(),
             m_outlineItem->boundingRect().width(),
             m_outlineItem->boundingRect().height(),
             m_outlineItem->isVisible(),
             qPrintable(m_color.name(QColor::HexArgb)),
             m_thickness);
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
    debugLog("[DEBUG-kwinoutline-file] renderer-set-geometry outer=%f,%f,%f,%f inner=%f,%f,%f,%f rect=%f,%f,%f,%f bounding=%f,%f,%f,%f thickness=%f",
             geometry.outerRect.x(),
             geometry.outerRect.y(),
             geometry.outerRect.width(),
             geometry.outerRect.height(),
             geometry.innerRect.x(),
             geometry.innerRect.y(),
             geometry.innerRect.width(),
             geometry.innerRect.height(),
             m_outlineItem->rect().x(),
             m_outlineItem->rect().y(),
             m_outlineItem->rect().width(),
             m_outlineItem->rect().height(),
             m_outlineItem->boundingRect().x(),
             m_outlineItem->boundingRect().y(),
             m_outlineItem->boundingRect().width(),
             m_outlineItem->boundingRect().height(),
             m_thickness);
}

void OutlineWindowRenderer::setColor(const QColor &color)
{
    if (m_color == color) {
        return;
    }
    m_color = color;
    updateOutline();
    debugLog("[DEBUG-kwinoutline-file] renderer-set-color color=%s thickness=%f visible=%d",
             qPrintable(m_color.name(QColor::HexArgb)),
             m_thickness,
             isVisible());
}

void OutlineWindowRenderer::setVisible(bool visible)
{
    if (m_outlineItem) {
        m_outlineItem->setVisible(visible);
        debugLog("[DEBUG-kwinoutline-file] renderer-set-visible requested=%d actual=%d explicit=%d rect=%f,%f,%f,%f bounding=%f,%f,%f,%f",
                 visible,
                 m_outlineItem->isVisible(),
                 m_outlineItem->explicitVisible(),
                 m_outlineItem->rect().x(),
                 m_outlineItem->rect().y(),
                 m_outlineItem->rect().width(),
                 m_outlineItem->rect().height(),
                 m_outlineItem->boundingRect().x(),
                 m_outlineItem->boundingRect().y(),
                 m_outlineItem->boundingRect().width(),
                 m_outlineItem->boundingRect().height());
    }
}

void OutlineWindowRenderer::refreshWindowRadius(KWin::EffectWindow &window)
{
    KWin::BorderRadius outerRadius;

    // SSD: decoration provides the authoritative corner radius.
    const KDecoration3::Decoration *decoration = window.decoration();
    if (decoration) {
        outerRadius = KWin::BorderRadius::from(decoration->borderRadius());
    } else {
        // CSD/undecorated: use whatever KWin's scene has computed for this item.
        const KWin::WindowItem *item = window.windowItem();
        if (item) {
            outerRadius = item->borderRadius();
        }
    }

    if (outerRadius == m_windowOuterRadius) {
        return;
    }
    m_windowOuterRadius = outerRadius;
    updateOutline();
}

KWin::BorderRadius OutlineWindowRenderer::computeInnerRadius() const
{
    // The outline's inner rect is inset from the frame outer edge by `inset` on each side.
    // BorderOutline's radius field is the inner-edge corner radius. The outer-edge corner
    // radius = innerRadius + thickness. So innerRadius = outerRadius - inset.
    const qreal inset = (m_placement == OutlinePlacement::Inside) ? m_thickness
                      : (m_placement == OutlinePlacement::Centered) ? m_thickness / 2.0
                      : 0.0;
    return KWin::BorderRadius(
        std::max(0.0, m_windowOuterRadius.topLeft() - inset),
        std::max(0.0, m_windowOuterRadius.topRight() - inset),
        std::max(0.0, m_windowOuterRadius.bottomRight() - inset),
        std::max(0.0, m_windowOuterRadius.bottomLeft() - inset));
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
    debugLog("[DEBUG-kwinoutline-file] renderer-set-thickness-placement thickness=%f placement=%d frame=%fx%f inner=%f,%f,%f,%f rect=%f,%f,%f,%f bounding=%f,%f,%f,%f color=%s",
             m_thickness,
             static_cast<int>(m_placement),
             frameSize.width(),
             frameSize.height(),
             geom.innerRect.x(),
             geom.innerRect.y(),
             geom.innerRect.width(),
             geom.innerRect.height(),
             m_outlineItem->rect().x(),
             m_outlineItem->rect().y(),
             m_outlineItem->rect().width(),
             m_outlineItem->rect().height(),
             m_outlineItem->boundingRect().x(),
             m_outlineItem->boundingRect().y(),
             m_outlineItem->boundingRect().width(),
             m_outlineItem->boundingRect().height(),
             qPrintable(m_color.name(QColor::HexArgb)));
}

void OutlineWindowRenderer::updateOutline()
{
    if (m_outlineItem) {
        const KWin::BorderRadius innerRadius = computeInnerRadius();
        m_outlineItem->setOutline(KWin::BorderOutline(m_thickness, m_color, innerRadius));
        debugLog("[DEBUG-kwinoutline-file] renderer-update-outline thickness=%f color=%s visible=%d radius-tl=%f tr=%f br=%f bl=%f",
                 m_thickness,
                 qPrintable(m_color.name(QColor::HexArgb)),
                 m_outlineItem->isVisible(),
                 innerRadius.topLeft(),
                 innerRadius.topRight(),
                 innerRadius.bottomRight(),
                 innerRadius.bottomLeft());
    }
}

} // namespace KWinOutline
