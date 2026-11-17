// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/reachability.h"

namespace rme::procedural {

struct LevelRepairReport {
    uint32_t repairs = 0, bridges = 0, removedCaps = 0, removedPois = 0;
    uint32_t removedDecor = 0;
    uint32_t removedCaveComponents = 0;
    uint32_t removedStoreys = 0;
    uint32_t removedDungeons = 0;
    bool unrepaired = false;
};

// Iteration 43 placement counts: buildings with a door, a rectangular Floor footprint and an interior of at
// least 16 cells qualify; chosen ones lose their floor on a collision (caps, landings, ramp openings or passes
// on z - 1 within one cell) or when no stairs position keeps the ground floor connected.
struct StoreyReport {
    uint32_t qualifying = 0, chosen = 0, placed = 0, collisions = 0, noStairs = 0;
};

// Iteration 45 placement counts (same qualifying buildings as storeys): a chosen building loses its dungeon when no
// stairs position keeps its ground floor connected (noStairs) or when fewer than two rooms fit (noSpace).
struct DungeonReport {
    uint32_t qualifying = 0, chosen = 0, placed = 0, noStairs = 0, noSpace = 0;
};

// Doored rectangular Floor components with an interior of at least 16 cells and no ruin gap, site or fire, in
// row-major order of their first cell (iterations 43 and 45). Sets `cancelled` and returns nothing on a stop request.
struct BuildingFootprint {
    uint32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0, door = 0;
};
[[nodiscard]] std::vector<BuildingFootprint> qualifyingBuildings(const SurfaceGrid& surface, std::stop_token stop, bool& cancelled);

struct MountainPass {
    uint32_t from = 0, to = 0;
    std::vector<uint32_t> cells; // complete three-wide surface footprint
    std::vector<uint8_t> before; // counterfactual walkability before this cut
};

struct LevelWorld {
    SurfaceGrid surface;
    std::optional<CaveGrid> cave;
    CaveExitPlan exits;
    std::vector<PlateauGrid> caps;
    std::vector<DecorGrid> decor; // surface, optional cave, then caps (or one empty grid for a storey-only z - 1)
    LevelRepairReport repair;
    std::vector<MountainPass> passes;
    std::vector<BuildingStorey> storeys;
    StoreyReport storeyReport;
    std::vector<BuildingDungeon> dungeons;
    DungeonReport dungeonReport;
};

// Iteration 43: chooses tuning.upperFloorPercent % of the qualifying buildings (one roll each, row-major order of
// the building's first cell) and gives each a storey with stairs from `stairShapes`. A stair at S with direction d
// needs S, S + d and the low end (tail, then the entry cell beyond it) inside the interior, S beside a wall
// perpendicular to d, and the interior without S and its tail connected to the door. Candidates are taken in
// (shape, cell) order; the seed picks one of them. Does nothing without buildingStoreys or shapes.
[[nodiscard]] std::expected<void, ReachabilityError> placeBuildingStoreys(
    LevelWorld& world, uint64_t seed, const SurfaceTuning& tuning,
    std::span<const ExitShape> stairShapes, std::stop_token stop = {});

// Iteration 45: chooses tuning.dungeonPercent % of the qualifying buildings (one roll each, own stream, row-major order)
// and gives each a dungeon on the cave floor. Only north-facing stair shapes are used (iteration 45c), so the visible
// stairs down and the stairs up below them share one direction. On the surface the descent D lies inside the building with D - d on its
// outline and the arrival D + d inside; the interior without D (and without storey stairs) stays connected to the door.
// On z + 1 the stairs up stand on D, with their tail, the landing D - d and the cell beyond the tail inside room 0.
// Rooms (2..6, interiors 4..9 x 4..9) hang off earlier rooms through straight corridors (length 2..6, width 1..2) with a
// door at both ends. Walkable cells keep a Chebyshev distance >= 5 from cave Floor (two empty cells between the wall
// rings), >= 5 from other dungeons, >= 3 between unconnected parts of one dungeon and >= 2 from the area's edge, and
// never lie under a Preserve surface cell. Without a cave (or the option, or shapes) nothing is placed. `storeyShapes`
// are the iteration 43 stair shapes of world.storeys, whose ground-floor cells the descent avoids.
[[nodiscard]] std::expected<void, ReachabilityError> placeBuildingDungeons(
    LevelWorld& world, uint64_t seed, const SurfaceTuning& tuning, std::span<const ExitShape> stairShapes,
    std::span<const ExitShape> storeyShapes = {}, std::stop_token stop = {});

// Carve valleys, rebuild caps/ramp selections and reselect affected entrances.
[[nodiscard]] std::expected<void, ReachabilityError> placeMountainPasses(
    LevelWorld& world, uint64_t seed, const SurfaceTuning& tuning,
    std::span<const ExitShape> shapes, std::stop_token stop = {});

// Complete geometry before repair. Entrances reserve notches in the first cap.
// The historical repairReach=0 producer stays separate. `caveBlocked` is generateCave's blocked mask (iteration 49).
[[nodiscard]] std::expected<LevelWorld, ReachabilityError> buildLevelWorld(
    SurfaceGrid surface, DecorGrid decor, uint64_t seed, const SurfaceTuning& tuning,
    const DecorTuning& decorTuning, std::span<const ExitShape> caveShapes,
    std::span<const ExitShape> capShapes, bool vegetation, std::stop_token stop = {},
    std::span<const ExitShape> stairShapes = {}, std::span<const ExitShape> dungeonShapes = {},
    std::span<const uint8_t> caveBlocked = {});

// Costs: 0 forbidden, 1 open, 3 removable blocking decor, 6 water.
// Sources and destinations are masks. Water-run length is part of the search
// state; merging it into a single distance per cell loses valid cheaper paths.
struct LevelRepairPath { std::vector<uint32_t> cells; uint64_t cost = 0; };
[[nodiscard]] std::expected<LevelRepairPath, ReachabilityError> cheapestLevelPath(
    std::span<const uint8_t> costs, uint32_t width, uint32_t height,
    std::span<const uint8_t> sources, std::span<const uint8_t> destinations,
    uint32_t fordLength, std::stop_token stop = {});

[[nodiscard]] std::expected<LevelRepairReport, ReachabilityError> repairLevelReach(
    LevelWorld& world, bool walls, bool rockBlocks, std::span<const ExitShape> caveShapes,
    std::array<bool, decorKindCount> blockingClasses, uint32_t fordLength, bool bridges,
    // Optional resolved mask: 1 removable decor, 2 permanent item/ground blocker.
    std::span<const uint8_t> blockingMask = {}, std::stop_token stop = {});

// Final resolved/autobordered floors: 1 is a removable item, 2 is permanent
// (blocking ground, wall or transition). Clears mask entries along cut paths;
// the adapter applies those removals through DrawOperations and rebuilds the
// actual mask before accepting the result. Never changes geometry or drops POI.
// remainingRepairs shares the plan's original 64-repair budget.
[[nodiscard]] std::expected<LevelRepairReport, ReachabilityError> repairLevelReach(
    const LevelPlan& plan, std::span<uint8_t> blockingMask,
    uint32_t remainingRepairs, std::stop_token stop = {});

} // namespace rme::procedural
