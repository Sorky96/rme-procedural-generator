// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/preview_viewport.h"

#include <algorithm>
#include <cmath>

namespace rme::procedural {

namespace {
PreviewArea valid(PreviewArea area) noexcept { return {std::max(area.width, 1u), std::max(area.height, 1u)}; }
PreviewPane valid(PreviewPane pane) noexcept { return {std::max(pane.width, 1.0), std::max(pane.height, 1.0)}; }

// Centre range on one axis: the middle when the content is narrower than the pane, else the inner range.
double clampAxis(double center, double cells, double paneCells) noexcept {
    if (paneCells >= cells) return cells / 2.0;
    return std::clamp(center, paneCells / 2.0, cells - paneCells / 2.0);
}
} // namespace

double previewFitScale(PreviewArea area, PreviewPane pane) noexcept {
    area = valid(area); pane = valid(pane);
    return std::min(pane.width / area.width, pane.height / area.height);
}

uint32_t previewMaxLevel(PreviewArea area, PreviewPane pane) noexcept {
    const double fit = previewFitScale(area, pane);
    uint32_t level = 0;
    while (level < maxPreviewZoomLevel && fit * double(1u << (level + 1)) <= maxPreviewPixelsPerCell) ++level;
    return level;
}

double previewScale(const PreviewViewport& view, PreviewArea area, PreviewPane pane) noexcept {
    return previewFitScale(area, pane) * double(1u << std::min(view.level, previewMaxLevel(area, pane)));
}

PreviewViewport previewFit(PreviewArea area) noexcept {
    area = valid(area);
    return {0, area.width / 2.0, area.height / 2.0};
}

PreviewViewport previewClamp(PreviewViewport view, PreviewArea area, PreviewPane pane) noexcept {
    area = valid(area); pane = valid(pane);
    view.level = std::min(view.level, previewMaxLevel(area, pane));
    const double scale = previewScale(view, area, pane);
    view.centerX = clampAxis(view.centerX, area.width, pane.width / scale);
    view.centerY = clampAxis(view.centerY, area.height, pane.height / scale);
    return view;
}

PreviewViewport previewZoomAt(PreviewViewport view, PreviewArea area, PreviewPane pane, double x, double y, int steps) noexcept {
    area = valid(area); pane = valid(pane);
    view = previewClamp(view, area, pane);
    const double before = previewScale(view, area, pane);
    const double anchorX = view.centerX + (x - pane.width / 2.0) / before;
    const double anchorY = view.centerY + (y - pane.height / 2.0) / before;
    const int level = std::clamp(int(view.level) + steps, 0, int(previewMaxLevel(area, pane)));
    view.level = uint32_t(level);
    if (!level) return previewFit(area);
    const double after = previewScale(view, area, pane);
    view.centerX = anchorX - (x - pane.width / 2.0) / after;
    view.centerY = anchorY - (y - pane.height / 2.0) / after;
    return previewClamp(view, area, pane);
}

PreviewViewport previewPan(PreviewViewport view, PreviewArea area, PreviewPane pane, double dx, double dy) noexcept {
    view = previewClamp(view, area, pane);
    const double scale = previewScale(view, area, pane);
    view.centerX -= dx / scale;
    view.centerY -= dy / scale;
    return previewClamp(view, area, pane);
}

double previewOriginX(const PreviewViewport& view, PreviewArea area, PreviewPane pane) noexcept {
    pane = valid(pane);
    return pane.width / 2.0 - view.centerX * previewScale(view, area, pane);
}

double previewOriginY(const PreviewViewport& view, PreviewArea area, PreviewPane pane) noexcept {
    pane = valid(pane);
    return pane.height / 2.0 - view.centerY * previewScale(view, area, pane);
}

PreviewCellRect previewVisibleCells(const PreviewViewport& view, PreviewArea area, PreviewPane pane) noexcept {
    area = valid(area); pane = valid(pane);
    const double scale = previewScale(view, area, pane);
    const auto axis = [&](double center, double paneSize, uint32_t cells, uint32_t& lo, uint32_t& hi) {
        const double first = center - paneSize / (2.0 * scale), last = center + paneSize / (2.0 * scale);
        lo = uint32_t(std::clamp(std::floor(first), 0.0, double(cells)));
        hi = uint32_t(std::clamp(std::ceil(last), double(lo), double(cells)));
        if (hi == lo && lo < cells) ++hi;
    };
    PreviewCellRect rect;
    axis(view.centerX, pane.width, area.width, rect.x0, rect.x1);
    axis(view.centerY, pane.height, area.height, rect.y0, rect.y1);
    return rect;
}

std::optional<PreviewCell> previewCellAt(const PreviewViewport& view, PreviewArea area, PreviewPane pane, double x, double y) noexcept {
    area = valid(area); pane = valid(pane);
    const double scale = previewScale(view, area, pane);
    const double cx = view.centerX + (x - pane.width / 2.0) / scale, cy = view.centerY + (y - pane.height / 2.0) / scale;
    if (cx < 0 || cy < 0 || cx >= area.width || cy >= area.height) return std::nullopt;
    return PreviewCell {uint32_t(cx), uint32_t(cy)};
}

} // namespace rme::procedural
