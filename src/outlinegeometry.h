// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QRectF>

namespace KWinOutline
{

enum class OutlinePlacement {
    Inside,
    Centered,
    Outside,
};

struct OutlinePlacementGeometry {
    QRectF outerRect;
    QRectF innerRect;
};

OutlinePlacementGeometry computeOutlineGeometry(const QRectF &frameRect, double thickness, OutlinePlacement placement);

} // namespace KWinOutline
