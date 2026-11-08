// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/scale_stats.h"

#include <iostream>
#include <string>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

// One row of 128 cells: 64 of Grass, then 64 of Water. Counted by hand:
//   chance = (64^2 + 64^2) / 128^2 = 500 permille.
//   lag 4:  124 pairs, 4 differ  -> 120 same -> 967 -> excess 467.
//   lag 16: 112 pairs, 16 differ -> 96 same  -> 857 -> excess 357.
//   lag 64:  64 pairs, all differ -> 0 same  ->   0 -> excess -500.
void testHandCounted() {
	std::vector<uint8_t> kinds(128, 0);
	for (size_t i = 64; i < 128; ++i) kinds[i] = 4;
	LagStats stats;
	addLagStats(kinds, 128, 1, stats);
	check(stats.pairs[0] == 124 && stats.same[0] == 120, "lag 4 pairs and same");
	check(stats.pairs[1] == 112 && stats.same[1] == 96, "lag 16 pairs and same");
	check(stats.pairs[2] == 64 && stats.same[2] == 0, "lag 64 pairs and same");
	check(stats.cells[0] == 64 && stats.cells[4] == 64, "cells per kind");
	const auto excess = lagExcessPermille(stats);
	check(excess[0] == 467 && excess[1] == 357 && excess[2] == -500, "excess per lag");
}

void testSkippedCells() {
	std::vector<uint8_t> kinds(128, 255);
	LagStats stats;
	addLagStats(kinds, 128, 1, stats);
	check(stats.pairs == std::array<uint64_t, 3> {} && lagExcessPermille(stats) == std::array<int32_t, 3> {}, "skipped cells give no pairs and zero excess");
	kinds.assign(128, 0);
	kinds[10] = 255;
	LagStats one;
	addLagStats(kinds, 128, 1, one);
	check(one.pairs[0] == 122, "a skipped cell removes its two lag-4 pairs");
}

void testSurface() {
	SurfaceGrid grid;
	grid.region = {0, 0, 7, 128, 1};
	grid.cells.assign(128, SurfaceKind::Grass);
	for (size_t i = 64; i < 128; ++i) grid.cells[i] = i % 2 ? SurfaceKind::Lake : SurfaceKind::River;
	const LagStats stats = lagStatsOf(grid);
	check(stats.cells[0] == 64 && stats.cells[4] == 64, "Lake and River both count as Water");
	check(lagExcessPermille(stats)[2] == -500, "same result as the hand-counted row");
	grid.cells[3] = SurfaceKind::Preserve;
	check(lagStatsOf(grid).cells[0] == 63, "Preserve is skipped");
}

void testAdd() {
	LagStats a, b;
	a.same[1] = 3; a.pairs[1] = 5; a.cells[2] = 7;
	b.same[1] = 1; b.pairs[1] = 2; b.cells[2] = 1;
	a += b;
	check(a.same[1] == 4 && a.pairs[1] == 7 && a.cells[2] == 8, "sums add up");
}
// 20 x 20 grass with a 10 x 10 dirt square: 100 dirt cells, 4 * 10 dirt-grass pairs, one component of exactly 100.
void testPatches() {
	std::vector<uint8_t> kinds(400, 0);
	for (size_t y = 5; y < 15; ++y) for (size_t x = 5; x < 15; ++x) kinds[y * 20 + x] = 1;
	PatchStats stats;
	addPatchStats(kinds, 20, 20, stats);
	check(stats.dirtCells == 100 && stats.dirtGrassPairs == 40 && stats.dirtInLarge == 100, "dirt square counted by hand");
	check(stats.cells[0] == 300 && stats.cells[1] == 100, "cells per kind");
	check(!summarizePatches(stats).boundary && !summarizePatches(stats).compact, "100 dirt cells are below the minimum: not applicable");
	kinds[5 * 20 + 5] = 0; // a notch splits nothing but removes 1 cell and changes the border by 0 (corner)
	PatchStats notched;
	addPatchStats(kinds, 20, 20, notched);
	check(notched.dirtCells == 99 && notched.dirtInLarge == 0, "99 cells is below the large-patch size");
	// One row: 5 water, 3 shore, 4 grass. Only the first shore cell touches water.
	std::vector<uint8_t> row {4, 4, 4, 4, 4, 3, 3, 3, 0, 0, 0, 0};
	PatchStats beach;
	addPatchStats(row, 12, 1, beach);
	check(beach.shoreCells == 3 && beach.shoreNextToWater == 1 && beach.shoreNearWater == 3, "beach cells counted by hand: all 3 are reached through shore");
	std::vector<uint8_t> farSand(40, 3);
	for (size_t i = 0; i < 5; ++i) farSand[i] = 4;
	PatchStats desert;
	addPatchStats(farSand, 40, 1, desert);
	check(desert.shoreCells == 35 && desert.shoreNextToWater == 1 && desert.shoreNearWater == beachReach, "sand farther than beachReach steps from the water is not beach");
	PatchStats big;
	big.dirtCells = 2000; big.dirtGrassPairs = 500; big.dirtInLarge = 1500; big.shoreCells = 9000; big.shoreNearWater = 3000; big.shoreNextToWater = 1000;
	big.cells = {100, 2000, 100, 3000, 100};
	const PatchSummary summary = summarizePatches(big);
	check(summary.boundary == 250u && summary.compact == 750u && summary.beach == 3000u, "summary of large counts");
	check(summary.sharePermille[1] == 2000 * 1000 / 5300, "shares of natural cells");
	SurfaceGrid grid;
	grid.region = {0, 0, 7, 12, 1};
	grid.cells = {SurfaceKind::Lake, SurfaceKind::River, SurfaceKind::Shore, SurfaceKind::Preserve, SurfaceKind::Dirt, SurfaceKind::Grass,
		SurfaceKind::Grass, SurfaceKind::Grass, SurfaceKind::Grass, SurfaceKind::Grass, SurfaceKind::Grass, SurfaceKind::Grass};
	const PatchStats fromGrid = patchStatsOf(grid);
	check(fromGrid.cells[4] == 2 && fromGrid.shoreNextToWater == 1 && fromGrid.dirtGrassPairs == 1, "grid kinds map to the same counts");
}

} // namespace

int main() {
	testHandCounted();
	testSkippedCells();
	testSurface();
	testAdd();
	testPatches();
	if (failures) return 1;
	std::cout << "OK\n";
	return 0;
}
