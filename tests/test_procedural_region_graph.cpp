// SPDX-License-Identifier: GPL-3.0-or-later
// Region graph of a generated surface (G5 iteration 24). Every expected number was counted by hand from the layout.
#include "procedural/core/region_graph.h"

#include <iostream>
#include <stop_token>
#include <string>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

SurfaceGrid grid(uint32_t width, uint32_t height, SurfaceKind fill) {
	SurfaceGrid g;
	g.region = {0, 0, 7, width, height};
	g.cells.assign(size_t(width) * height, fill);
	return g;
}

void fillRect(SurfaceGrid& g, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, SurfaceKind kind) {
	for (uint32_t y = y0; y <= y1; ++y) for (uint32_t x = x0; x <= x1; ++x) g.cells[size_t(y) * g.region.width + x] = kind;
}

// 14 x 6: Water x 6..8 (18 cells), Rock x 0..3, y 1..4 (16), Land on the left = 6 x 6 - 16 = 20, Land on the right x 9..13 = 30.
//   Nodes by first cell: Land 20 (cell 0), Water 18 (cell 6), Land 30 (cell 9), Rock 16 (cell 14) -> 0, 1, 2, 3.
//   Boundaries: Land-Rock: above row 0 x 0..3 = 4, below row 5 x 0..3 = 4, right x = 4 for y 1..4 = 4 -> 12; Land-Water: x 5 | x 6 on 6 rows = 6;
//   Water-Land right: x 8 | x 9 on 6 rows = 6. Edges (0,1,6), (0,3,12), (1,2,6).
//   Start: the middle (6.5, 2.5) is water; the nearest walkable cells are (5,2) and (5,3) (distance 10), the lower index (5,2) wins.
//   Exits: north (5,0) (x = 5 is nearer the middle than x = 9), south (5,5), west (0,2) (rock is walkable), east (13,2).
//   Distances over walkable cells: north 2, south 3, west 5, east unreachable.
void testHandCounted() {
	SurfaceGrid g = grid(14, 6, SurfaceKind::Grass);
	fillRect(g, 6, 0, 8, 5, SurfaceKind::Lake);
	fillRect(g, 0, 1, 3, 4, SurfaceKind::Rock);
	const auto r = buildRegionGraph(g, true);
	check(r.has_value(), "hand counted: result");
	if (!r) return;
	check(r->regions.size() == 4, "hand counted: four regions");
	if (r->regions.size() != 4) return;
	check(r->regions[0].kind == RegionClass::Land && r->regions[0].cells == 20, "region 0 is Land with 20 cells");
	check(r->regions[1].kind == RegionClass::Water && r->regions[1].cells == 18, "region 1 is Water with 18 cells");
	check(r->regions[2].kind == RegionClass::Land && r->regions[2].cells == 30, "region 2 is Land with 30 cells");
	check(r->regions[3].kind == RegionClass::Rock && r->regions[3].cells == 16, "region 3 is Rock with 16 cells");
	check(r->regions[3].minX == 0 && r->regions[3].maxX == 3 && r->regions[3].minY == 1 && r->regions[3].maxY == 4, "rock box 0..3 x 1..4");
	check(r->regions[3].centroidX == 1 && r->regions[3].centroidY == 2, "rock centroid (1, 2): mean of 1.5 and 2.5 rounded down");
	check(r->edges.size() == 3, "three edges");
	if (r->edges.size() == 3) {
		check(r->edges[0].a == 0 && r->edges[0].b == 1 && r->edges[0].boundary == 6, "edge Land-Water 6");
		check(r->edges[1].a == 0 && r->edges[1].b == 3 && r->edges[1].boundary == 12, "edge Land-Rock 12");
		check(r->edges[2].a == 1 && r->edges[2].b == 2 && r->edges[2].boundary == 6, "edge Water-Land 6");
	}
	check(r->smallFragments == 0 && r->smallFragmentCells == 0 && r->excludedCells == 0, "no fragments and no excluded cells");
	check(r->pois.size() == 5, "start and four exits");
	if (r->pois.size() == 5) {
		check(r->pois[0].kind == PoiKind::Start && r->pois[0].cell == 2 * 14 + 5 && r->pois[0].region == 0 && r->pois[0].distance == 0, "start (5,2) in region 0");
		check(r->pois[1].kind == PoiKind::Exit && r->pois[1].cell == 5 && r->pois[1].distance == 2, "north exit (5,0), distance 2");
		check(r->pois[2].kind == PoiKind::Exit && r->pois[2].cell == 5 * 14 + 5 && r->pois[2].distance == 3, "south exit (5,5), distance 3");
		check(r->pois[3].kind == PoiKind::Exit && r->pois[3].cell == 2 * 14 && r->pois[3].region == 3 && r->pois[3].distance == 5, "west exit (0,2) in the rock, distance 5");
		check(r->pois[4].kind == PoiKind::Exit && r->pois[4].cell == 2 * 14 + 13 && r->pois[4].region == 2 && r->pois[4].distance == -1, "east exit (13,2) across the water, unreachable");
	}
}

// 12 x 8 Grass with a 3 x 3 Rock (9 cells: a fragment) and one Preserve cell: the grass is one region of 96 - 9 - 1 = 86 cells.
void testFragmentsAndExcluded() {
	SurfaceGrid g = grid(12, 8, SurfaceKind::Grass);
	fillRect(g, 4, 3, 6, 5, SurfaceKind::Rock);
	g.cells[0] = SurfaceKind::Preserve;
	const auto r = buildRegionGraph(g, true);
	check(r.has_value() && r->regions.size() == 1 && r->regions[0].cells == 86, "fragment: one Land region of 86 cells");
	check(r.has_value() && r->smallFragments == 1 && r->smallFragmentCells == 9 && r->excludedCells == 1, "fragment: one fragment of 9 cells, one excluded cell");
	check(r.has_value() && r->edges.empty(), "fragment: no edges");
	uint64_t total = 0;
	if (r) { for (const auto& node : r->regions) total += node.cells; total += r->smallFragmentCells + r->excludedCells; }
	check(total == 96, "fragment: every cell is counted once");
}

// A building: Floor x 2..6, y 1..5 in grass, door at (4,5) -> the entrance is in the Land region, reachable; without the door it has no entrance.
void testEntrance() {
	SurfaceGrid g = grid(12, 7, SurfaceKind::Grass);
	fillRect(g, 2, 1, 6, 5, SurfaceKind::Floor);
	g.doors = {5 * 12 + 4};
	const auto r = buildRegionGraph(g, true);
	check(r.has_value() && r->regions.size() == 1 && r->regions[0].cells == 84, "entrance: one Land region (Floor counts as Land)");
	bool found = false;
	if (r) for (const auto& poi : r->pois) if (poi.kind == PoiKind::Entrance) { found = poi.cell == 5 * 12 + 4 && poi.region == 0 && poi.distance >= 0; }
	check(found, "entrance: the door is a reachable POI in region 0");
}

void testErrorsAndDeterminism() {
	SurfaceGrid bad;
	check(buildRegionGraph(bad, true).error_or(RegionGraphError::Cancelled) == RegionGraphError::InvalidGrid, "empty grid is invalid");
	SurfaceGrid sea = grid(5, 5, SurfaceKind::Lake);
	const auto r = buildRegionGraph(sea, true);
	check(r.has_value() && r->regions.size() == 1 && r->pois.empty(), "all water: one region, no POI");
	SurfaceGrid g = grid(14, 6, SurfaceKind::Grass);
	fillRect(g, 6, 0, 8, 5, SurfaceKind::Lake);
	std::stop_source source;
	source.request_stop();
	check(buildRegionGraph(g, true, source.get_token()).error_or(RegionGraphError::InvalidGrid) == RegionGraphError::Cancelled, "stop is honoured");
	const auto a = buildRegionGraph(g, true), b = buildRegionGraph(g, true);
	check(a.has_value() && b.has_value() && a->edges.size() == b->edges.size() && a->regions.size() == b->regions.size() && a->pois.size() == b->pois.size(), "deterministic");
}
} // namespace

int main() {
	testHandCounted();
	testFragmentsAndExcluded();
	testEntrance();
	testErrorsAndDeterminism();
	if (failures) { std::cerr << failures << " failures\n"; return 1; }
	std::cout << "REGION_GRAPH_PASS\n";
	return 0;
}
