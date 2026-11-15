// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/generation.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace rme::procedural {

// Surface algorithm v2 is a separate generator from the v1 grass/dirt/water roll.
// v1 types and fingerprints are untouched; v2 has its own kinds and fingerprint domain.
inline constexpr uint32_t surfaceAlgorithmVersion = 2;

enum class SurfaceKind : uint8_t { Preserve = 0, Grass, Dirt, Rock, Shore, Lake, River, Path, Floor, Bridge };
enum class BridgeAxis : uint8_t { None, Horizontal, Vertical };

// All fields below are pure functions of (seed, absolute coordinates) and of
// whole-region thresholds. A mask or protection therefore never changes the
// value of another cell. Integer math only, so results do not depend on the FPU.
struct SurfaceConfig {
	GenerationRegion region;
	uint64_t seed = 0;
	SurfaceTuning tuning;
};

// Working memory is an upper-bound estimate, not a measurement.
struct SurfaceBudget { uint64_t maxCells = 0; uint64_t maxOutputBytes = 0; uint64_t maxWorkingBytes = 0; };
struct SurfaceLayout { size_t cellCount = 0; size_t outputBytes = 0; uint64_t workingBytes = 0; };

enum class SurfaceError {
	InvalidRegion, InvalidParameters, InvalidMask, NoWritableCells,
	CellLimit, OutputLimit, WorkingLimit, SizeOverflow, Cancelled, AllocationFailed
};

struct SurfaceGrid {
	GenerationRegion region;
	std::vector<SurfaceKind> cells;
	// Door cells (G4 iteration 12), one per building foundation, as row-major cell indices; each lies on a Floor cell.
	// Empty unless buildings are on, and then not part of the fingerprint.
	std::vector<uint32_t> doors;
	// Ruins and camps (G5 iteration 25): outline cells of a Floor foundation that get no wall piece (gaps; walkable) and one
	// interior cell per ruin or camp (sites). Row-major cell indices on Floor cells. Empty unless ruins or camps are on.
	std::vector<uint32_t> gaps;
	std::vector<uint32_t> sites;
	// Camp fires (G5 iteration 26): one Path cell in the middle of every camp clearing. Empty unless camps are on.
	std::vector<uint32_t> fires;
	// Z entrances: RockFace on Rock or FloorHole on Grass/Dirt; same x/y one floor below.
	std::vector<uint32_t> entrances;
    std::vector<BridgeAxis> bridgeAxes; // empty without repair bridges; one axis per cell otherwise
	[[nodiscard]] bool valid() const noexcept;
	uint64_t fingerprint() const;
};

struct SandIslandReport {
	uint32_t islands = 0;
	uint32_t nonShoreIslands = 0;
	uint64_t nonShoreCells = 0;
	std::vector<uint32_t> cells;
};

// Four-connected land includes rock, paths, fords and bridges. The largest
// component (first in row order on ties), POI components, rectangle edges and
// components touching Preserve are excluded. Only surface z=7 is classified.
[[nodiscard]] std::expected<SandIslandReport, SurfaceError> sandIslandsOf(
	const SurfaceGrid& grid, std::stop_token stop = {});
[[nodiscard]] std::expected<void, SurfaceError> normalizeSandIslands(
	SurfaceGrid& grid, std::stop_token stop = {});

// Local entrance geometry including margin and water/building clearance.
[[nodiscard]] bool caveEntranceAllowed(std::span<const SurfaceKind> cells, uint32_t width, uint32_t height, uint32_t cell, CaveEntranceStyle style, bool relaxedRockFace = false) noexcept;
// What the generator keeps for the style: RockFace also keeps the FloorHole cells it falls back to when the rock walls
// have too few sites for the requested count (iteration 48).
[[nodiscard]] bool caveEntranceFits(std::span<const SurfaceKind> cells, uint32_t width, uint32_t height, uint32_t cell, CaveEntranceStyle style, bool relaxedRockFace = false) noexcept;

[[nodiscard]] std::expected<void, SurfaceError> reselectCaveEntrances(
    SurfaceGrid& grid, const SurfaceConfig& config, std::stop_token stop = {});

// Empty mask/protection: same rules as validateGeneration. Rivers end at a lake or
// at the border of the generated rectangle, never inside the land.
// Shared integer noise for generators that must agree with the surface fields.
uint32_t surfaceValueNoise(uint64_t seed, uint64_t stream, uint32_t x, uint32_t y, uint32_t step);
uint16_t surfaceLattice(uint64_t seed, uint64_t stream, uint32_t x, uint32_t y);

// Shore smoothing (G4 iteration 18/20), exposed for tests: `passes` rounds of a (2r+1) x (2r+1) majority filter (r = `radius`, 0 means 3)
// over Shore, Grass and Dirt cells within 3r cells of a Lake cell (Lake counts as sand, Rock and the edge of the area are not counted); cells next to a Lake cell
// (8-neighbours) are left alone. A Shore cell that turns into land becomes Grass or Dirt by the majority in its window (tie: Grass).
void smoothShores(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, uint32_t passes, uint32_t radius = 0);

// Spit trimming (G4 iteration 19), exposed for tests: a Shore cell that lies in a horizontal or vertical run of at most 2 Shore
// cells with Lake or River at both ends becomes water (River when either end is River, otherwise Lake). One pass on a snapshot.
void trimSpits(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height);

// Fords (G4 iteration 14), exposed for tests: joins walkable components of at least 16 cells to another component across at
// most `reach` River or Lake cells by stamping the shortest crossing (and the water beside it) as Shore. `kinds` is a
// width * height grid, `doors` lie on Floor cells; walls are assumed on. Deterministic. generateSurface calls this when
// SurfaceTuning::fordLength is set.
// Crossings of at least 8 water cells get a natural shape (swinging axis, varying width, iteration 14d) driven by the seed and the
// absolute position of the area; shorter ones stay straight.
// `repair` (iteration 23): only components that hold a door or an exit and are not the start's component get a ford, whatever their size.
struct FordShape { uint64_t seed = 0; int32_t x = 0; int32_t y = 0; bool repair = false; std::span<const uint32_t> sites = {}; };

std::expected<void, SurfaceError> placeFords(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, uint32_t reach,
	std::span<const uint32_t> doors, std::stop_token stop = {}, FordShape shape = {});

// Seed rejection (G4 iteration 23): tries `seed`, `seed + 1`, ... up to `maxAttempts` and returns the first surface whose doors and exits are all
// reachable from the fixed start (see poiReachabilityOf; walls assumed). When none is, the first attempt comes back with allReached = false.
struct ReachableSurface { SurfaceGrid grid; uint64_t seed = 0; uint32_t attempts = 0; bool allReached = false; };

std::expected<ReachableSurface, SurfaceError> generateReachableSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget, uint32_t maxAttempts,
	std::span<const uint8_t> mask = {}, std::span<const uint8_t> protection = {}, std::stop_token stop = {});

std::expected<SurfaceLayout, SurfaceError> validateSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget,
	std::span<const uint8_t> mask = {}, std::span<const uint8_t> protection = {},
	std::stop_token stop = {});

std::expected<SurfaceGrid, SurfaceError> generateSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget,
	std::span<const uint8_t> mask = {}, std::span<const uint8_t> protection = {},
	std::stop_token stop = {});

} // namespace rme::procedural
