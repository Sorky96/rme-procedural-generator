// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>

namespace rme::procedural {

// I60: zoom and pan of the generator's minimap preview. One image pixel per map cell; a pane in screen pixels.
// Level 0 fits the whole area; level L shows it 2^L times larger, up to 4096x and never above 64 pixels per cell,
// so a large area in a low pane still reaches the sprite threshold.
inline constexpr uint32_t maxPreviewZoomLevel = 12;
inline constexpr double maxPreviewPixelsPerCell = 64.0;

struct PreviewArea { uint32_t width = 1, height = 1; };   // cells
struct PreviewPane { double width = 1, height = 1; };     // screen pixels
struct PreviewViewport {
    uint32_t level = 0;
    double centerX = 0.5, centerY = 0.5; // cells, continuous
    bool operator==(const PreviewViewport&) const = default;
};
struct PreviewCellRect { uint32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0; }; // half-open [x0, x1) x [y0, y1)
struct PreviewCell { uint32_t x = 0, y = 0; bool operator==(const PreviewCell&) const = default; };

[[nodiscard]] double previewFitScale(PreviewArea area, PreviewPane pane) noexcept;
// Highest level whose cells stay within maxPreviewPixelsPerCell (at least 0).
[[nodiscard]] uint32_t previewMaxLevel(PreviewArea area, PreviewPane pane) noexcept;
[[nodiscard]] double previewScale(const PreviewViewport& view, PreviewArea area, PreviewPane pane) noexcept;
[[nodiscard]] PreviewViewport previewFit(PreviewArea area) noexcept;
// Same level and centre, clamped so the view stays within the area (centred where the area is smaller than the pane).
[[nodiscard]] PreviewViewport previewClamp(PreviewViewport view, PreviewArea area, PreviewPane pane) noexcept;
// Changes the level by `steps` (positive zooms in) keeping the point (x, y) of the pane over the same cell, then clamps.
[[nodiscard]] PreviewViewport previewZoomAt(PreviewViewport view, PreviewArea area, PreviewPane pane, double x, double y, int steps) noexcept;
// Moves the content by (dx, dy) screen pixels (positive dx shows cells further left), then clamps.
[[nodiscard]] PreviewViewport previewPan(PreviewViewport view, PreviewArea area, PreviewPane pane, double dx, double dy) noexcept;
// Screen position of the area's top-left corner.
[[nodiscard]] double previewOriginX(const PreviewViewport& view, PreviewArea area, PreviewPane pane) noexcept;
[[nodiscard]] double previewOriginY(const PreviewViewport& view, PreviewArea area, PreviewPane pane) noexcept;
// Cells at least partly visible.
[[nodiscard]] PreviewCellRect previewVisibleCells(const PreviewViewport& view, PreviewArea area, PreviewPane pane) noexcept;
[[nodiscard]] std::optional<PreviewCell> previewCellAt(const PreviewViewport& view, PreviewArea area, PreviewPane pane, double x, double y) noexcept;

} // namespace rme::procedural
