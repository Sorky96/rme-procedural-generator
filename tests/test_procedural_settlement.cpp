// SPDX-License-Identifier: GPL-3.0-or-later
// Settlement statistics (G4 iteration 12). Every expected number below was counted by hand from the layout.
#include "procedural/core/settlement_stats.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

BuildingBox box(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, std::vector<uint32_t> doors = {}) {
	BuildingBox b;
	b.x0 = x0; b.y0 = y0; b.x1 = x1; b.y1 = y1;
	b.cells = 2 * ((x1 - x0 + 1) + (y1 - y0 + 1)) - 4;
	b.doorCells = std::move(doors);
	return b;
}

// Floor 60 x 40, all present. B1 (5..10, 5..9) has a door at (7,9) on its south edge; B2 (14..19, 5..9) has an
// interior door at (16,7); B3 (50..55, 30..34) is far away. One trail cell at (7,12), one water cell at (25,7).
//   gaps: B1-B2 = 14 - 10 = 4; B3 to B2 = max(50-19, 30-9) = 31, to B1 = 40, so nearest = {4, 4, 31}, median 4.
//   clusters (gap <= 12): {B1, B2} and {B3} -> sizes {2, 1}.
//   trail within 3 of the box: B1 rows 2..12 contain (7,12) -> yes; B2 columns 11..22 do not; B3 no -> 1.
//   water within 8: B2 columns 6..27 contain (25,7) -> yes; B1 columns -3..18 do not -> 1.
//   B1: trail line south of the edge cell (7,9) reaches (7,12) at step 3 -> trail on side S; door on side S -> facing.
//   Controls: water control = 2400 - 1 (water) = 2399 cells, of which those within 8 of (25,7): columns 17..33 x
//   rows 0..15 = 17 * 16 = 272, minus the water cell itself = 271. Trail control = 2399 - 1 (trail) = 2398 cells,
//   of which within 3 of (7,12): 7 x 7 = 49 minus the trail cell = 48.
void testHandCounted() {
	SettlementFloor floor;
	floor.width = 60; floor.height = 40;
	floor.present.assign(2400, 1); floor.water.assign(2400, 0); floor.path.assign(2400, 0);
	floor.path[12 * 60 + 7] = 1;
	floor.water[7 * 60 + 25] = 1;
	floor.buildings = {box(5, 5, 10, 9, {9 * 60 + 7}), box(14, 5, 19, 9, {7 * 60 + 16}), box(50, 30, 55, 34)};
	SettlementStats stats;
	addSettlementFloor(floor, stats);
	check(stats.buildings == 3 && stats.regionCells == 2400, "counts");
	check(stats.nearestGap == std::vector<uint32_t> {4, 4, 31}, "nearest gaps");
	check(stats.clusterSizes.size() == 2, "two clusters");
	check((stats.clusterSizes[0] == 2 && stats.clusterSizes[1] == 1) || (stats.clusterSizes[0] == 1 && stats.clusterSizes[1] == 2), "cluster sizes 2 and 1");
	check(stats.nearPath == 1 && stats.nearWater == 1, "near trail and water");
	check(stats.waterControlCells == 2399 && stats.waterControlHit == 271, "water control");
	check(stats.pathControlCells == 2398 && stats.pathControlHit == 48, "trail control");
	check(stats.withDoor == 2 && stats.doorCells == 2 && stats.boundaryDoorCells == 1, "doors: 2 buildings, 2 cells, 1 on the boundary");
	check(stats.facingSet == 1 && stats.facingHit == 1 && stats.facingTrailSides == 1, "door faces the trail");
	const SettlementSummary summary = summarizeSettlements(stats);
	check(summary.buildings == 3 && !summary.gapMedian && !summary.facingP, "fewer than 30 buildings is N/A");
}

// 30 boxes 4 x 4 in a row, 20 apart (gap 20 - 3 = 17 > 12): all alone, no doors, no trail, no water.
//   region 620 x 10 = 6200 cells; clusters 30 -> 1000 * 30 * 65536 / 6200 = 317109 (integer division).
void testSummary() {
	SettlementFloor floor;
	floor.width = 620; floor.height = 10;
	floor.present.assign(6200, 1); floor.water.assign(6200, 0); floor.path.assign(6200, 0);
	for (uint32_t i = 0; i < 30; ++i) floor.buildings.push_back(box(20 * i, 2, 20 * i + 3, 5));
	SettlementStats stats;
	addSettlementFloor(floor, stats);
	const SettlementSummary summary = summarizeSettlements(stats);
	check(summary.gapMedian == 17u, "gap 17");
	check(summary.clustersPer64kMilli == 317109u, "clusters per 64k");
	check(summary.clusterSizeMedian == 1u && summary.aloneP == 1000u && summary.inLargeP == 0u, "all alone");
	check(summary.nearPathP == 0u && summary.nearWaterP == 0u && summary.withDoorP == 0u, "no trail, water or door");
	check(!summary.doorsMedian && !summary.boundaryDoorP && !summary.facingP, "door features absent");
	check(summary.pathControlP == 0u && summary.waterControlP == 0u, "controls zero");
}

// Reference adapter: the same B1 outline as extractBuildingShapes' test, cobblestone ground south of it.
void testReference() {
	auto map = SemanticMap::create(std::vector<FloorBounds> {{7, 0, 0, 40, 30}}, {100000}).value();
	const uint16_t grass = map.internGroundKey("grass").value();
	const uint16_t stone = map.internGroundKey("cobblestone").value();
	const uint16_t sea = map.internGroundKey("water").value();
	const uint32_t ground = bit(Trait::HasGround) | bit(Trait::Present);
	for (int y = 0; y < 30; ++y) for (int x = 0; x < 40; ++x) check(map.set(x, y, 7, grass, ground).has_value(), "set ground");
	for (int x = 5; x < 11; ++x) { check(map.set(x, 5, 7, grass, ground | bit(Trait::Wall)).has_value(), "wall"); check(map.set(x, 9, 7, grass, ground | bit(Trait::Wall)).has_value(), "wall"); }
	for (int y = 6; y < 9; ++y) { check(map.set(5, y, 7, grass, ground | bit(Trait::Wall)).has_value(), "wall"); check(map.set(10, y, 7, grass, ground | bit(Trait::Wall)).has_value(), "wall"); }
	check(map.set(7, 9, 7, grass, ground | bit(Trait::Door)).has_value(), "door");
	check(map.set(7, 12, 7, stone, ground).has_value(), "trail");
	check(map.set(30, 20, 7, sea, ground).has_value(), "water");
	const std::vector<KeyMapEntry> keymap {{"grass", StyleKind::Grass, ""}, {"cobblestone", StyleKind::Other, ""}, {"water", StyleKind::Water, ""}};
	const std::vector<std::string_view> trailKeys {"cobblestone"};
	const auto stats = extractSettlementStats(map, keymap, trailKeys, 0, 7, {});
	check(stats.has_value(), "extracted");
	if (!stats) return;
	check(stats->buildings == 1 && stats->nearPath == 1 && stats->nearWater == 0, "one building near the trail, not near water");
	check(stats->facingSet == 1 && stats->facingHit == 1 && stats->doorCells == 1 && stats->boundaryDoorCells == 1, "door faces the trail");
	check(extractSettlementStats(map, keymap, trailKeys, 3, 2, {}).error() == StatsError::InvalidRange, "reversed range refused");
	const std::vector<KeyMapEntry> partial {{"grass", StyleKind::Grass, ""}};
	check(extractSettlementStats(map, partial, trailKeys, 0, 7, {}).error() == StatsError::KeyNotMapped, "unmapped key refused");
}

// Generator adapter: a 3 x 4 block of Floor in a 10 x 10 grid, and a Path cell and a Lake cell.
void testGrid() {
	SurfaceGrid grid;
	grid.region = {0, 0, 7, 10, 10};
	grid.cells.assign(100, SurfaceKind::Grass);
	for (int y = 2; y < 6; ++y) for (int x = 1; x < 4; ++x) grid.cells[size_t(y) * 10 + size_t(x)] = SurfaceKind::Floor;
	grid.cells[7 * 10 + 2] = SurfaceKind::Path;
	grid.cells[9 * 10 + 9] = SurfaceKind::Lake;
	const std::vector<BuildingBox> boxes = floorBoxesOf(grid);
	check(boxes.size() == 1 && boxes[0].x0 == 1 && boxes[0].x1 == 3 && boxes[0].y0 == 2 && boxes[0].y1 == 5 && boxes[0].cells == 12, "floor block box");
	const SettlementStats stats = settlementStatsOf(grid);
	check(stats.buildings == 1 && stats.regionCells == 100, "grid counts");
	check(stats.nearPath == 1, "path row 7 is within 3 of row 5");
	check(stats.nearWater == 1, "lake corner (9,9) is within 8 of the block");
}

} // namespace

int main() {
	testHandCounted();
	testSummary();
	testReference();
	testGrid();
	if (failures) return 1;
	std::cout << "OK\n";
	return 0;
}
