// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/style_profile.h"
#include "procedural/core/surface.h"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>

namespace rme::procedural {

// Large-scale structure (G4 iteration 6): how often two natural-kind cells d apart have the same kind, for
// several d, and how much that exceeds a random layout with the same kind shares. Integer math only; the
// definitions are fixed in docs/procedural/G4_METRICS.md.
inline constexpr std::array<uint32_t, 3> scaleLags {4, 16, 64};

struct LagStats {
	std::array<uint64_t, 3> same {};   // pairs (horizontal and vertical) with equal kinds, per lag
	std::array<uint64_t, 3> pairs {};  // pairs where both cells are natural kinds, per lag
	std::array<uint64_t, 5> cells {};  // natural cells per kind: Grass, Dirt, Rock, Shore, Water
	bool operator==(const LagStats&) const = default;
	LagStats& operator+=(const LagStats& other) noexcept;
};

// `kinds` has width * height cells: 0..4 = Grass, Dirt, Rock, Shore, Water; anything else is skipped.
void addLagStats(std::span<const uint8_t> kinds, uint32_t width, uint32_t height, LagStats& out);

// Reference side: floors zMin..zMax through `keymap`; absent tiles are skipped.
[[nodiscard]] std::expected<LagStats, StatsError> lagStatsOf(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop);
// Generator side: Lake and River count as Water, Preserve is skipped.
[[nodiscard]] LagStats lagStatsOf(const SurfaceGrid& grid);

// Excess over chance in permille per lag: 1000 * same / pairs - 1000 * sum(share^2). 0 when there are no pairs.
[[nodiscard]] std::array<int32_t, 3> lagExcessPermille(const LagStats& stats);

// Shape of patches (G4 iteration 7), same natural-kind grids: how ragged the dirt is, how compact its patches are and
// how thick the beach is. Definitions in docs/procedural/G4_METRICS.md.
inline constexpr uint32_t beachReach = 8;       // steps through Shore that still count as beach
inline constexpr uint32_t rockLargePatch = 500;
inline constexpr uint32_t rockDustPatch = 20;
inline constexpr uint32_t dirtLargePatch = 100; // cells of a 4-connected Dirt component counted as compact

struct PatchStats {
	uint64_t dirtCells = 0;
	uint64_t dirtGrassPairs = 0;     // 4-adjacent Dirt-Grass pairs
	uint64_t dirtInLarge = 0;        // Dirt cells in components of at least dirtLargePatch cells
	uint64_t shoreCells = 0;
	uint64_t shoreNextToWater = 0;   // Shore cells with a 4-neighbour of kind Water
	uint64_t rockCells = 0;
	uint64_t rockPairs = 0;          // 4-adjacent pairs of Rock and another natural kind
	uint64_t rockInLarge = 0;        // Rock cells in components of at least rockLargePatch cells
	uint64_t rockDust = 0;           // Rock cells in components of fewer than rockDustPatch cells
	uint64_t shoreNearWater = 0;     // Shore cells within beachReach steps through Shore of a Shore cell next to Water (those included)
	std::array<uint64_t, 5> cells {};
	bool operator==(const PatchStats&) const = default;
	PatchStats& operator+=(const PatchStats& other) noexcept;
};

void addPatchStats(std::span<const uint8_t> kinds, uint32_t width, uint32_t height, PatchStats& out);
[[nodiscard]] std::expected<PatchStats, StatsError> patchStatsOf(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop);
[[nodiscard]] PatchStats patchStatsOf(const SurfaceGrid& grid);

inline constexpr uint64_t minDirtCells = 1000;
inline constexpr uint64_t minShoreCells = 1000;
inline constexpr uint64_t minRockCellsForStats = 1000;
struct PatchSummary {
	std::optional<uint32_t> boundary;   // D: 1000 * dirtGrassPairs / dirtCells; empty when dirtCells < minDirtCells
	std::optional<uint32_t> compact;    // F: permille of Dirt cells in large patches; same condition
	std::optional<uint32_t> beach;      // P': 1000 * shoreNearWater / shoreNextToWater; empty when shoreNearWater < minShoreCells
	std::optional<uint32_t> rockBoundary; // RB: 1000 * rockPairs / rockCells; empty when rockCells < minRockCellsForStats
	std::optional<uint32_t> rockCompact;  // RF: permille of Rock cells in large patches
	std::optional<uint32_t> rockDustShare; // RS: permille of Rock cells in tiny patches
	std::array<uint32_t, 5> sharePermille {}; // of natural cells
	bool operator==(const PatchSummary&) const = default;
};
[[nodiscard]] PatchSummary summarizePatches(const PatchStats& stats);

} // namespace rme::procedural
