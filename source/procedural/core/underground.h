// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/surface.h"

#include <cstdint>
#include <expected>
#include <stop_token>
#include <vector>

namespace rme::procedural {

// The floor below the surface (G5 iteration 27): a cave reached through the Z entrances of SurfaceGrid::entrances.
// Definitions and criteria: docs/procedural/G5_METRICS.md.
enum class CaveKind : uint8_t { Solid = 0, Floor };

struct CaveGrid {
	GenerationRegion region;   // same x, y, width and height as the surface, z = surface z + 1
	std::vector<CaveKind> cells;
	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] uint64_t fingerprint() const;
};

// Landing (21 cells) around every entrance, a tunnel (3 wide, swinging axis) between consecutive entrances (by cell index) or, for one
// entrance, towards the middle of the area (up to 40 cells), and a room every ~24 cells of axis. Cells within 1 of the area's edge stay
// Solid. Pure function of the surface and the seed; checks stop_requested per tunnel.
[[nodiscard]] std::expected<CaveGrid, SurfaceError> generateCave(const SurfaceGrid& surface, uint64_t seed, std::stop_token stop = {});

struct UndergroundReport {
	uint32_t entrances = 0, entrancesReached = 0;   // surface side: reached when in the start's component
	uint64_t caveCells = 0, caveReachedCells = 0;
	uint32_t caveComponents = 0;
	uint32_t pockets = 0;                           // cave components with no reached entrance
};

enum class UndergroundError : uint8_t { InvalidGrid, Cancelled, AllocationFailed };

// An entrance is reached when its surface cell lies in the start's component (poiReachabilityOf); cave cells are reached when they lie in a
// 4-connected component of Floor cells that holds the landing of a reached entrance.
[[nodiscard]] std::expected<UndergroundReport, UndergroundError> undergroundReachabilityOf(
	const SurfaceGrid& surface, const CaveGrid& cave, bool walls, std::stop_token stop = {});

} // namespace rme::procedural
