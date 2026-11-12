// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/reachability.h"

namespace rme::procedural {

struct LevelRepairReport {
    uint32_t repairs = 0, bridges = 0, removedCaps = 0, removedPois = 0;
    uint32_t removedDecor = 0;
    uint32_t removedCaveComponents = 0;
    bool unrepaired = false;
};

struct LevelWorld {
    SurfaceGrid surface;
    std::optional<CaveGrid> cave;
    CaveExitPlan exits;
    std::vector<PlateauGrid> caps;
    std::vector<DecorGrid> decor; // surface, optional cave, then caps
    LevelRepairReport repair;
};

// Complete geometry before repair. Entrances reserve notches in the first cap.
// The historical repairReach=0 producer stays separate.
[[nodiscard]] std::expected<LevelWorld, ReachabilityError> buildLevelWorld(
    SurfaceGrid surface, DecorGrid decor, uint64_t seed, const SurfaceTuning& tuning,
    const DecorTuning& decorTuning, std::span<const ExitShape> caveShapes,
    std::span<const ExitShape> capShapes, bool vegetation, std::stop_token stop = {});

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

} // namespace rme::procedural
