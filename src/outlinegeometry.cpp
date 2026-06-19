// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "outlinegeometry.h"

namespace KWinOutline
{

OutlinePlacementGeometry computeOutlineGeometry(const QRectF &frameRect, double thickness, OutlinePlacement placement)
{
    double outerExpand = 0.0;
    double innerShrink = 0.0;

    switch (placement) {
    case OutlinePlacement::Inside:
        outerExpand = 0.0;
        innerShrink = thickness;
        break;
    case OutlinePlacement::Centered:
        outerExpand = thickness / 2.0;
        innerShrink = thickness / 2.0;
        break;
    case OutlinePlacement::Outside:
        outerExpand = thickness;
        innerShrink = 0.0;
        break;
    }

    return {
        frameRect.adjusted(-outerExpand, -outerExpand, outerExpand, outerExpand),
        frameRect.adjusted(innerShrink, innerShrink, -innerShrink, -innerShrink),
    };
}

} // namespace KWinOutline
