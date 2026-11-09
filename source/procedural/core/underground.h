// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/surface.h"

#include <cstdint>
#include <expected>
#include <span>
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

// Geometry only: the lower ramp at P leads to P + direction on the surface.
// An optional lower-floor tail is relative to P; the upper ramp part is at P.
enum class ExitDirection : uint8_t { North = 0, South, East, West };
struct ExitShape {
	ExitDirection direction = ExitDirection::North;
	int8_t tailDx = 0;
	int8_t tailDy = 0;
	bool hasTail = false;
	bool operator==(const ExitShape&) const = default;
};
struct CaveExit {
	uint32_t entrance = 0; // index into SurfaceGrid::entrances
	uint32_t cell = 0;     // row-major P
	uint8_t shape = 0;     // index into shapes
};
struct CaveExitPlan {
	std::vector<CaveExit> exits;
	std::vector<uint32_t> skipped; // entrance indices without an exit
};

// First fit in (squared distance, y, x, shape) order, processing entrances in list order.
// occupied, when present, has one byte per surface cell (nonzero blocks placement).
[[nodiscard]] std::expected<CaveExitPlan, UndergroundError> placeCaveExits(
	const SurfaceGrid& surface, const CaveGrid& cave, std::span<const ExitShape> shapes,
	std::span<const uint8_t> occupied = {}, std::stop_token stop = {});

// An entrance is reached when its surface cell lies in the start's component (poiReachabilityOf); cave cells are reached when they lie in a
// 4-connected component of Floor cells that holds the landing of a reached entrance.
[[nodiscard]] std::expected<UndergroundReport, UndergroundError> undergroundReachabilityOf(
	const SurfaceGrid& surface, const CaveGrid& cave, bool walls, std::stop_token stop = {});

} // namespace rme::procedural
