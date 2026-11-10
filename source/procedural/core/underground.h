// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/decoration.h"
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
// An optional lower-floor tail is relative to P; no upper ramp part is placed.
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

// P = entrance only, first fitting shape in list order. Reserve lower P, landing P - direction
// and optional tail (the ramp's lower half, usually on the landing), and open surface destination P + direction.
// Entrances without a fit are skipped; never search adjacent ramp cells.
// occupied, when present, has one byte per surface cell (nonzero blocks placement).
[[nodiscard]] std::expected<CaveExitPlan, UndergroundError> placeCaveExits(
	const SurfaceGrid& surface, const CaveGrid& cave, std::span<const ExitShape> shapes,
	std::span<const uint8_t> occupied = {}, std::stop_token stop = {});

// An entrance is reached when its surface cell lies in the start's component (poiReachabilityOf); cave cells are reached when they lie in a
// 4-connected component of Floor cells that holds the landing of a reached entrance.
[[nodiscard]] std::expected<UndergroundReport, UndergroundError> undergroundReachabilityOf(
	const SurfaceGrid& surface, const CaveGrid& cave, bool walls, std::stop_token stop = {});

// Upper-floor plateau geometry. Landings are tracked separately from Top.
enum class PlateauKind : uint8_t { None = 0, Top };
struct PlateauRamp {
    uint32_t cell = 0, destination = 0, component = 0;
    uint8_t shape = 0;
    bool operator==(const PlateauRamp&) const = default;
};
struct PlateauGrid {
    GenerationRegion region;
    std::vector<PlateauKind> cells;
    std::vector<uint8_t> landings;
    std::vector<PlateauRamp> ramps;
    uint32_t components = 0;
    uint32_t inset = 1; // 1 for the whole first component; Chebyshev distance threshold >= 3 above it
};
std::expected<PlateauGrid, SurfaceError> generatePlateau(
    const SurfaceGrid& surface, uint64_t seed, uint32_t minTop, std::span<const ExitShape> shapes, std::span<const uint8_t> occupied = {},
    std::stop_token stop = {}, uint32_t maxPercent = 0
);
std::expected<PlateauGrid, SurfaceError> generateNextPlateau(
    const PlateauGrid& lower, uint64_t seed, std::span<const ExitShape> shapes, std::stop_token stop = {}, uint32_t maxPercent = 0
);
std::expected<DecorGrid, SurfaceError>
generatePlateauDecoration(const PlateauGrid& plateau, uint64_t seed, const DecorTuning& tuning, std::stop_token stop = {});

} // namespace rme::procedural
