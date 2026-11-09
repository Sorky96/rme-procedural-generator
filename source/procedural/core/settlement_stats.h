// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/scale_stats.h"
#include "procedural/core/style_profile.h"
#include "procedural/core/surface.h"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <vector>

namespace rme::procedural {

// Settlement structure (G4 iteration 12): how buildings sit relative to each other, to trails and to water, and which
// side their doors face. Definitions and constants are fixed in docs/procedural/G4_METRICS.md ("Iteracja 12").
inline constexpr uint32_t settlementClusterGap = 12;  // buildings closer than this (gap, see below) share a cluster
inline constexpr uint32_t settlementPathReach = 3;    // "next to a trail": within this Chebyshev distance
inline constexpr uint32_t settlementWaterReach = 8;
inline constexpr uint32_t settlementMinBuildings = 30; // fewer buildings: the map is N/A for the shape features

// One floor, reference or generated. All masks have width * height cells, row-major.
struct SettlementFloor {
	uint32_t width = 0, height = 0;
	std::vector<uint8_t> present, water, path;
	std::vector<BuildingBox> buildings;
};

struct SettlementStats {
	uint64_t regionCells = 0;                // present cells of the examined floors
	uint64_t buildings = 0;
	std::vector<uint32_t> nearestGap;        // per building that has a neighbour on its floor
	std::vector<uint32_t> clusterSizes;      // buildings per cluster
	uint64_t nearPath = 0, nearWater = 0;    // buildings with a trail within 3 / water within 8 of the bounding box
	uint64_t pathControlCells = 0, pathControlHit = 0;    // present cells that are neither water nor trail / those with a trail within 3
	uint64_t waterControlCells = 0, waterControlHit = 0;  // present cells that are not water / those with water within 8
	uint64_t withDoor = 0, doorCells = 0, boundaryDoorCells = 0;
	std::vector<uint32_t> doorsPerDoored;    // Door cells per building that has any
	uint64_t facingSet = 0;                  // S: buildings with a boundary door and a trail within 3
	uint64_t facingHit = 0;                  // of S: a door side is also a trail side
	uint64_t facingTrailSides = 0;           // of S: sum of the number of trail sides (0..4)
};

// Adds one floor to the totals.
void addSettlementFloor(const SettlementFloor& floor, SettlementStats& stats);

// Reference side. `pathKeys` are ground key names that make a trail; water comes from the key map like the channel
// statistics. Floors zMin..zMax, each on its own.
[[nodiscard]] std::expected<SettlementStats, StatsError> extractSettlementStats(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, std::span<const std::string_view> pathKeys,
	int zMin, int zMax, std::stop_token stop);

// Generator side: Floor components are the buildings (no doors yet), Path cells are trails, Lake and River are water.
[[nodiscard]] SettlementStats settlementStatsOf(const SurfaceGrid& grid);

// Every field is N/A (nullopt) when the map has fewer than settlementMinBuildings buildings (facing: |S| below it).
struct SettlementSummary {
	uint32_t buildings = 0;
	uint32_t facingSet = 0;
	std::optional<uint32_t> gapMedian;               // G
	std::optional<uint32_t> clustersPer64kMilli;     // Kn: 1000 * clusters per 65 536 present cells
	std::optional<uint32_t> clusterSizeMedian;       // Ks
	std::optional<uint32_t> aloneP;                  // Ki, permille of buildings
	std::optional<uint32_t> inLargeP;                // Kb: permille of buildings in clusters of at least 3
	std::optional<uint32_t> nearPathP, pathControlP; // Bp, Cp
	std::optional<uint32_t> nearWaterP, waterControlP; // Bv, Cv
	std::optional<uint32_t> withDoorP;               // Dh
	std::optional<uint32_t> doorsMedian;             // Dn: median Door cells among buildings with doors
	std::optional<uint32_t> boundaryDoorP;           // Db
	std::optional<uint32_t> facingP, facingChanceP;  // Dp, Dp0
	bool operator==(const SettlementSummary&) const = default;
};
[[nodiscard]] SettlementSummary summarizeSettlements(const SettlementStats& stats);

} // namespace rme::procedural
