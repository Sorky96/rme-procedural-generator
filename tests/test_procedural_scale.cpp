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
} // namespace

int main() {
	testHandCounted();
	testSkippedCells();
	testSurface();
	testAdd();
	if (failures) return 1;
	std::cout << "OK\n";
	return 0;
}
