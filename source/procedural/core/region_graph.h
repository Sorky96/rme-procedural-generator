// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/surface.h"

#include <cstdint>
#include <expected>
#include <stop_token>
#include <vector>

namespace rme::procedural {

// Region graph of a generated surface (G5 iteration 24). Definitions and criteria: docs/procedural/G5_METRICS.md.
inline constexpr uint32_t regionMinCells = 16;

enum class RegionClass : uint8_t { Land, Rock, Water };
enum class PoiKind : uint8_t { Start, Entrance, Exit };

struct RegionNode {
	RegionClass kind = RegionClass::Land;
	uint32_t cells = 0;
	uint32_t minX = 0, minY = 0, maxX = 0, maxY = 0;
	uint32_t centroidX = 0, centroidY = 0;
};

struct RegionEdge {
	uint32_t a = 0, b = 0;   // a < b
	uint32_t boundary = 0;   // number of 4-adjacent cell pairs between the two regions
};

struct PoiNode {
	PoiKind kind = PoiKind::Start;
	uint32_t cell = 0;
	int32_t region = -1;     // index into RegionGraph::regions, -1 when the cell lies in a fragment smaller than regionMinCells
	int32_t distance = -1;   // steps over walkable cells from the start, -1 = not reachable
};

struct RegionGraph {
	std::vector<RegionNode> regions;
	std::vector<RegionEdge> edges;
	std::vector<PoiNode> pois;   // start first (when there is one), then entrances in door order, then exits (north, south, west, east)
	uint32_t smallFragments = 0;
	uint64_t smallFragmentCells = 0;
	uint64_t excludedCells = 0;  // Preserve cells
};

enum class RegionGraphError : uint8_t { InvalidGrid, Cancelled, AllocationFailed };

// Pure function of the grid and `walls` (walls on: a building outline without a door is not walkable, as in reachabilityOf).
// Checks stop_requested once per row. Working memory is about 16 bytes per cell.
[[nodiscard]] std::expected<RegionGraph, RegionGraphError> buildRegionGraph(
	const SurfaceGrid& grid, bool walls, std::stop_token stop = {});

} // namespace rme::procedural
