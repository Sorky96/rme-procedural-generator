// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/semantic_map.h"

#include <iostream>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
}

int main() {
	const std::vector<FloorBounds> bounds {{7, 100, 200, 4, 3}, {6, 0, 0, 2, 2}};
	const auto tooSmall = SemanticMap::create(bounds, {15});
	check(!tooSmall.has_value() && tooSmall.error() == SemanticError::CellLimit, "budget below 16 cells refused");
	auto created = SemanticMap::create(bounds, {16});
	check(created.has_value() && created->cellCount() == 16, "exact budget accepted");
	check(created->floors().front().bounds.z == 6, "floors ordered by z");

	const std::vector<FloorBounds> duplicate {{7, 0, 0, 1, 1}, {7, 5, 5, 1, 1}};
	check(SemanticMap::create(duplicate, {10}).error() == SemanticError::InvalidBounds, "duplicate floor refused");
	const std::vector<FloorBounds> empty {{7, 0, 0, 0, 1}};
	check(SemanticMap::create(empty, {10}).error() == SemanticError::InvalidBounds, "empty box refused");
	const std::vector<FloorBounds> deep {{16, 0, 0, 1, 1}};
	check(SemanticMap::create(deep, {10}).error() == SemanticError::InvalidBounds, "floor 16 refused");

	auto map = std::move(*created);
	const auto grass = map.internGroundKey("grass");
	const auto water = map.internGroundKey("water");
	check(grass && *grass == 1 && water && *water == 2, "keys interned in first-seen order");
	check(map.internGroundKey("grass").value() == 1, "key reused");
	check(map.internGroundKey("").value() == noGroundKey, "empty name is no-ground key");

	const uint32_t walk = bit(Trait::HasGround);
	const uint32_t wall = bit(Trait::HasGround) | bit(Trait::Blocking) | bit(Trait::Wall);
	check(map.set(100, 200, 7, 1, walk).has_value(), "set in bounds");
	check(map.set(103, 202, 7, 2, wall).has_value(), "set at far corner");
	check(map.set(104, 200, 7, 1, walk).error() == SemanticError::OutOfBounds, "x outside box");
	check(map.set(100, 199, 7, 1, walk).error() == SemanticError::OutOfBounds, "y outside box");
	check(map.set(100, 200, 5, 1, walk).error() == SemanticError::OutOfBounds, "missing floor");
	check(map.set(100, 200, 7, 9, walk).error() == SemanticError::OutOfBounds, "unknown ground key");
	check(map.set(0, 1, 6, noGroundKey, bit(Trait::Unknown)).has_value(), "tile without ground");
	check(map.set(1, 1, 6, noGroundKey, 0).has_value(), "tile with no traits still exists");

	const SemanticSummary summary = summarize(map);
	check(summary.tilesPerFloor[7] == 2 && summary.tilesPerFloor[6] == 2, "tiles per floor");
	check(summary.tilesPerTrait[0] == 2 && summary.tilesPerTrait[4] == 1 && summary.tilesPerTrait[15] == 1, "tiles per trait");
	check(summary.groundKeyStats[1].tiles == 1 && summary.groundKeyStats[2].tiles == 1 &&
		summary.groundKeyStats[2].blockingTiles == 1 && summary.groundKeyStats[0].tiles == 2, "ground key stats");
	const SemanticSummary again = summarize(map);
	check(again.tilesPerTrait == summary.tilesPerTrait && again.tilesPerFloor == summary.tilesPerFloor, "summary repeatable");

	// Analysis memory bound: 6 B per cell plus 16 B per cell of the largest floor, saturating.
	check(analysisWorkingBytes(100, 10) == 100 * 6 + 10 * 16, "analysis bytes");
	check(analysisWorkingBytes(0, 0) == 0, "analysis bytes of nothing");
	check(analysisWorkingBytes(UINT64_MAX / 6 + 1, 1) == UINT64_MAX, "cells overflow saturates");
	check(analysisWorkingBytes(1, UINT64_MAX / 16 + 1) == UINT64_MAX, "floor overflow saturates");
	check(analysisWorkingBytes(UINT64_MAX / 6, UINT64_MAX / 16) == UINT64_MAX, "sum overflow saturates");

	if (failures == 0) std::cout << "SEMANTIC_MAP_PASS\n";
	return failures == 0 ? 0 : 1;
}
