// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/surface.h"

#include <cstdint>
#include <expected>
#include <stop_token>

namespace rme::procedural {

// Walkable pocket threshold: components of at least this many cells outside the largest one count as pockets
// (same value as TopologyOptions::smallComponentCells).
inline constexpr uint32_t reachabilityPocketCells = 16;

// Reachability of a generated surface (G4 iteration 13). Model and criteria: docs/procedural/G4_METRICS.md.
// Walkable: Grass, Dirt, Rock, Shore, Path and the inside of a Floor foundation. Not walkable: Lake, River, Preserve and,
// with `walls`, the outline of a foundation except its door cell. 4-neighbour connectivity.
struct ReachabilityReport {
	uint64_t cells = 0;          // cells examined
	uint64_t walkable = 0;
	uint32_t components = 0;
	uint64_t largest = 0;
	uint64_t cutOff = 0;         // walkable cells outside the largest component
	uint32_t pockets = 0;        // components >= reachabilityPocketCells outside the largest one
	uint32_t buildings = 0;      // 4-connected Floor components
	uint32_t sealed = 0;         // buildings whose door is not in the largest component (or that have no door with walls)
};

enum class ReachabilityError : uint8_t { InvalidGrid, Cancelled, AllocationFailed };

// Pure function of the grid and `walls`. Checks stop_requested once per row. Working memory is 9 bytes per cell:
// component labels (4), a queue of at most one entry per cell (4) and one flag byte. An estimate, not a measurement.
[[nodiscard]] std::expected<ReachabilityReport, ReachabilityError> reachabilityOf(
	const SurfaceGrid& grid, bool walls, std::stop_token stop = {});

} // namespace rme::procedural
