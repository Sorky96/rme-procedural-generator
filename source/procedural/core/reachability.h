// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/surface.h"
#include "procedural/core/decoration.h"
#include "procedural/core/underground.h"
#include <array>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

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

// Walkable components of a surface, numbered from 1 in row-major order of their first cell; label 0 = not walkable.
// `largest` is the biggest component (ties go to the lowest label), 0 when nothing is walkable.
struct WalkableLabels {
	std::vector<uint32_t> label;
	std::vector<uint64_t> sizes;  // index = label; sizes[0] unused
	uint32_t largest = 0;
};

enum class ReachabilityError : uint8_t { InvalidGrid, Cancelled, AllocationFailed };

// The component labelling behind reachabilityOf, for callers that hold the cells before they form a SurfaceGrid. `doors`
// are cell indices that must lie on Floor cells. Checks stop_requested once per row.
[[nodiscard]] std::expected<WalkableLabels, ReachabilityError> walkableLabelsOf(
	std::span<const SurfaceKind> cells, uint32_t width, uint32_t height, std::span<const uint32_t> doors, bool walls, std::stop_token stop = {});

// Pure function of the grid and `walls`. Checks stop_requested once per row. Working memory is 9 bytes per cell:
// component labels (4), a queue of at most one entry per cell (4) and one flag byte. An estimate, not a measurement.
[[nodiscard]] std::expected<ReachabilityReport, ReachabilityError> reachabilityOf(
	const SurfaceGrid& grid, bool walls, std::stop_token stop = {});

// Doors then gaps of a grid: the outline cells of a foundation that can be walked through (iteration 25 adds the gaps of ruins and camps).
[[nodiscard]] std::vector<uint32_t> openingsOf(const SurfaceGrid& grid);

// The fixed start and the exit cells of iteration 22 (see PoiReport), computed from a labelling; also used by the repair pass.
[[nodiscard]] std::optional<uint32_t> fixedStartOf(const WalkableLabels& labels, uint32_t width, uint32_t height);
[[nodiscard]] std::vector<uint32_t> exitCellsOf(const WalkableLabels& labels, uint32_t width, uint32_t height);

// Reachability from a fixed start (G4 iteration 22, FAZA 5). Start: the walkable cell nearest to the middle of the area (squared
// distance, ties: lower index). Entrances: the door cells of SurfaceGrid::doors. Exits: for each side the walkable cell on that edge
// nearest to the middle of the side (ties: lower index); a side without a walkable edge cell has no exit. A point is reached when it
// lies in the start's component. Same walkability model as reachabilityOf.
struct PoiReport {
	bool hasStart = false;
	uint32_t start = 0;            // cell index
	uint32_t entrances = 0, entrancesReached = 0;
	uint32_t exits = 0, exitsReached = 0;
	uint32_t sites = 0, sitesReached = 0;   // ruins and camps (SurfaceGrid::sites), iteration 25
	uint64_t walkable = 0, startComponent = 0;
};

[[nodiscard]] std::expected<PoiReport, ReachabilityError> poiReachabilityOf(
	const SurfaceGrid& grid, bool walls, std::stop_token stop = {});

// Iteration 36. Floors share the region's x/y rectangle; index 0 is the surface.
// Producers translate plan semantics (including cliffs and ramp openings) to walkable.
// No item IDs or client/editor objects cross this boundary.
enum class LevelPoiKind : uint8_t { Door, Site, CaveEntrance, CaveExit, Cap, CaveCell, Count };
inline constexpr size_t levelPoiKindCount = size_t(LevelPoiKind::Count);
struct LevelFloor {
	int32_t z = 0;
	std::vector<uint8_t> walkable;
	std::vector<DecorKind> decor; // empty means no decoration
};
struct LevelCell {
	uint32_t floor = 0, cell = 0;
	bool operator==(const LevelCell&) const = default;
};
struct LevelEdge { LevelCell a, b; };
struct LevelPoi { LevelPoiKind kind; LevelCell at; };
struct LevelPlan {
	GenerationRegion region;
	std::vector<LevelFloor> floors;
	std::vector<LevelEdge> edges; // undirected Z transitions only
	std::vector<LevelPoi> pois;
};
// Semantics only: cap Top closes the supporting lower cell; landings and ramp
// openings are open. Only entrances with an actually placed exit produce Z edges.
[[nodiscard]] std::expected<LevelPlan, ReachabilityError> levelPlanOf(
	const SurfaceGrid& surface, bool walls, bool rockBlocks, const CaveGrid* cave,
	const CaveExitPlan& exits, std::span<const ExitShape> caveShapes,
	std::span<const PlateauGrid> caps, std::span<const DecorGrid> decor = {}, std::stop_token stop = {});
struct LevelCount { uint64_t reached = 0, total = 0; };
struct UnreachablePlace { LevelPoiKind kind; int32_t x = 0, y = 0, z = 0; };
struct LevelReachabilityReport {
	std::optional<uint32_t> start;
	std::array<LevelCount, levelPoiKindCount> categories {};
	std::vector<uint32_t> pockets; // every unreachable same-floor component, including small ones
	std::vector<UnreachablePlace> firstUnreachable; // category, z, y, x; at most 16
	std::vector<uint8_t> walkable, reached; // floor-major, then row-major; also used by the map probe
	uint64_t edges = 0, walkableEdges = 0;
	[[nodiscard]] bool allReached() const noexcept;
};
// A supplied mask replaces class blocking, one byte per cell in floor-major order.
// Without it, any blocking variant makes the entire class blocking (conservative).
// Mask never makes a semantically closed cell walkable. Checks cancellation during traversal.
[[nodiscard]] std::expected<LevelReachabilityReport, ReachabilityError> levelReachabilityOf(
	const LevelPlan& plan, std::array<bool, decorKindCount> blockingClasses,
	std::span<const uint8_t> blockingMask = {}, std::stop_token stop = {});

} // namespace rme::procedural
