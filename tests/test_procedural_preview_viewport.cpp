// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/preview_viewport.h"

#include <cmath>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace rme::procedural;

namespace {

void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

// Screen position of a continuous cell coordinate.
double screenX(const PreviewViewport& v, PreviewArea a, PreviewPane p, double cell) { return previewOriginX(v, a, p) + cell * previewScale(v, a, p); }
double screenY(const PreviewViewport& v, PreviewArea a, PreviewPane p, double cell) { return previewOriginY(v, a, p) + cell * previewScale(v, a, p); }

// True when the view is at a clamp limit on an axis (or centred because the area is narrower than the pane).
bool clampedX(const PreviewViewport& v, PreviewArea a, PreviewPane p) {
    const double half = p.width / (2 * previewScale(v, a, p));
    return std::abs(v.centerX - half) < 1e-9 || std::abs(v.centerX - (a.width - half)) < 1e-9 || std::abs(v.centerX - a.width / 2.0) < 1e-9;
}
bool clampedY(const PreviewViewport& v, PreviewArea a, PreviewPane p) {
    const double half = p.height / (2 * previewScale(v, a, p));
    return std::abs(v.centerY - half) < 1e-9 || std::abs(v.centerY - (a.height - half)) < 1e-9 || std::abs(v.centerY - a.height / 2.0) < 1e-9;
}

void fit() {
    const PreviewArea area {200, 100};
    const PreviewPane pane {400, 400};
    require(previewFitScale(area, pane) == 2.0, "fit scale");
    const auto view = previewFit(area);
    require(view.level == 0 && view.centerX == 100 && view.centerY == 50, "fit view");
    const auto rect = previewVisibleCells(view, area, pane);
    require(rect.x0 == 0 && rect.x1 == 200 && rect.y0 == 0 && rect.y1 == 100, "fit shows everything");
    require(previewOriginX(view, area, pane) == 0 && previewOriginY(view, area, pane) == 100, "fit is centred");
    require(previewMaxLevel(area, pane) == 5, "levels up to 32x");
    require(previewMaxLevel({4, 4}, {400, 400}) == 0, "100 px per cell already: no zoom");
    require(previewMaxLevel({10, 10}, {400, 400}) == 0 && previewMaxLevel({20, 20}, {400, 400}) == 1, "64 px cap");
}

void zoom() {
    const PreviewArea areas[] {{200, 100}, {1, 1}, {1, 2000}, {2000, 2000}, {128, 128}, {512, 300}};
    const PreviewPane panes[] {{400, 400}, {1, 1}, {640, 360}, {300, 700}};
    for (const auto area : areas)
        for (const auto pane : panes) {
            const double xs[] {0, pane.width / 2, pane.width - 1, pane.width / 3};
            const double ys[] {0, pane.height / 2, pane.height - 1, pane.height * 0.8};
            for (const double x : xs)
                for (const double y : ys) {
                    auto view = previewFit(area);
                    for (uint32_t level = 1; level <= previewMaxLevel(area, pane); ++level) {
                        const auto anchor = previewCellAt(view, area, pane, x, y);
                        const double before = previewScale(view, area, pane);
                        const double cellX = view.centerX + (x - pane.width / 2) / before, cellY = view.centerY + (y - pane.height / 2) / before;
                        const auto next = previewZoomAt(view, area, pane, x, y, 1);
                        require(next.level == level, "one level per step");
                        const std::string where = std::format("{}x{} in {}x{} at {},{} level {}", area.width, area.height, pane.width, pane.height, x, y, level);
                        require(std::abs(previewScale(next, area, pane) - 2 * before) < 1e-9, "scale doubles " + where);
                        // The anchor keeps its screen position, unless the view had to stop at the area's edge.
                        require(std::abs(screenX(next, area, pane, cellX) - x) <= 1.0 || clampedX(next, area, pane), "anchor x " + where);
                        require(std::abs(screenY(next, area, pane, cellY) - y) <= 1.0 || clampedY(next, area, pane), "anchor y " + where);
                        if (anchor) {
                            const auto rect = previewVisibleCells(next, area, pane);
                            require(rect.x0 <= anchor->x && anchor->x < rect.x1 && rect.y0 <= anchor->y && anchor->y < rect.y1, "anchor stays visible " + where);
                        }
                        view = next;
                    }
                    require(previewScale(view, area, pane) <= std::max(maxPreviewPixelsPerCell, previewFitScale(area, pane)) + 1e-9, "64 px cap");
                    require(previewZoomAt(view, area, pane, x, y, 1) == view, "no level above the cap");
                    require(previewZoomAt(view, area, pane, x, y, -100) == previewFit(area), "zoom out returns to the fit");
                }
        }
}

void pan() {
    const PreviewArea area {200, 100};
    const PreviewPane pane {400, 400};
    auto view = previewZoomAt(previewFit(area), area, pane, 200, 200, 2); // 8 px per cell, 50 x 50 cells visible
    require(previewScale(view, area, pane) == 8.0, "level 2");
    auto right = previewPan(view, area, pane, -1e6, 0);
    require(previewVisibleCells(right, area, pane).x1 == 200 && previewVisibleCells(right, area, pane).x0 == 150, "pan to the right edge");
    auto left = previewPan(view, area, pane, 1e6, 0);
    require(previewVisibleCells(left, area, pane).x0 == 0, "pan to the left edge");
    auto bottom = previewPan(view, area, pane, 0, -1e6);
    require(previewVisibleCells(bottom, area, pane).y1 == 100 && previewVisibleCells(bottom, area, pane).y0 == 50, "pan to the bottom edge");
    auto top = previewPan(view, area, pane, 0, 1e6);
    require(previewVisibleCells(top, area, pane).y0 == 0, "pan to the top edge");
    auto step = previewPan(view, area, pane, -80, 0);
    require(std::abs(step.centerX - view.centerX - 10) < 1e-9, "80 px pan moves 10 cells");
    // Cell under a point agrees with the visible rectangle.
    const auto rect = previewVisibleCells(step, area, pane);
    require(previewCellAt(step, area, pane, 0, 0) == PreviewCell {rect.x0, rect.y0}, "top-left cell");
    require(previewCellAt(step, area, pane, 399.5, 399.5) == PreviewCell {rect.x1 - 1, rect.y1 - 1}, "bottom-right cell");
    require(!previewCellAt(previewFit(area), area, pane, 10, 10), "letterbox has no cell");
}

} // namespace

int main() try {
    fit();
    zoom();
    pan();
    std::cout << "PREVIEW_VIEWPORT_PASS\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAILED " << error.what() << '\n';
    return 1;
}
