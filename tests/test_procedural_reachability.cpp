// SPDX-License-Identifier: GPL-3.0-or-later
// Reachability of a generated surface (G4 iteration 13). Hand-counted layouts plus a cross-check against an
// independent union-find implementation on random grids.
#include "procedural/core/reachability.h"
#include "procedural/core/level_repair.h"

#include <algorithm>
#include <iostream>
#include <format>
#include <limits>
#include <iterator>
#include <numeric>
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

// 10 x 5 land, a lake column at x = 4: west 4 x 5 = 20 cells, east 5 x 5 = 25 cells.
void testLakeSplit() {
	SurfaceGrid g = grid(10, 5, SurfaceKind::Grass);
	fillRect(g, 4, 0, 4, 4, SurfaceKind::Lake);
	const auto r = reachabilityOf(g, false);
	check(r.has_value(), "lake split: result");
	if (!r) return;
	check(r->walkable == 45 && r->components == 2 && r->largest == 25 && r->cutOff == 20 && r->pockets == 1, "lake split: 45 walkable, 2 components, largest 25, cut off 20, 1 pocket");
	check(r->buildings == 0 && r->sealed == 0, "lake split: no buildings");
}

// 12 x 7 land, building x 2..6, y 1..5 (5 x 5: 16 outline cells, 9 inside).
//   walls, no door: walkable = 84 - 25 + 9 = 68, components 2 (59 outside, 9 inside), cut off 9, no pocket (9 < 16), 1 sealed.
//   walls, door at (4,5): 69 walkable, 1 component, 0 sealed.
//   no walls: 84 walkable, 1 component, 0 sealed, the door changes nothing.
void testBuilding() {
	SurfaceGrid g = grid(12, 7, SurfaceKind::Grass);
	fillRect(g, 2, 1, 6, 5, SurfaceKind::Floor);
	auto r = reachabilityOf(g, true);
	check(r.has_value() && r->walkable == 68 && r->components == 2 && r->largest == 59 && r->cutOff == 9 && r->pockets == 0 && r->buildings == 1 && r->sealed == 1,
		"building with walls and no door is sealed");
	g.doors = {5 * 12 + 4};
	r = reachabilityOf(g, true);
	check(r.has_value() && r->walkable == 69 && r->components == 1 && r->largest == 69 && r->buildings == 1 && r->sealed == 0, "door joins inside and outside");
	r = reachabilityOf(g, false);
	check(r.has_value() && r->walkable == 84 && r->components == 1 && r->sealed == 0, "no walls: everything walkable");
	g.doors.clear();
	r = reachabilityOf(g, false);
	check(r.has_value() && r->walkable == 84 && r->sealed == 0, "no walls and no doors: not sealed");
}

// A door whose outside is water joins only the inside: still sealed.
void testDoorIntoWater() {
	SurfaceGrid g = grid(12, 7, SurfaceKind::Grass);
	fillRect(g, 2, 1, 6, 5, SurfaceKind::Floor);
	fillRect(g, 0, 6, 11, 6, SurfaceKind::Lake);
	g.doors = {5 * 12 + 4};
	const auto r = reachabilityOf(g, true);
	// Outside land: 84 - 12 (lake row) - 25 (building) = 47; inside 9 + door 1 = 10 (the door's south neighbour is water).
	check(r.has_value() && r->walkable == 57 && r->components == 2 && r->largest == 47 && r->sealed == 1, "door into water leaves the building sealed");
}

void testPreserve() {
	SurfaceGrid g = grid(5, 1, SurfaceKind::Grass);
	g.cells[2] = SurfaceKind::Preserve;
	const auto r = reachabilityOf(g, false);
	check(r.has_value() && r->walkable == 4 && r->components == 2 && r->largest == 2 && r->cutOff == 2 && r->pockets == 0, "Preserve blocks");
}

void testErrors() {
	SurfaceGrid bad;
	check(reachabilityOf(bad, false).error_or(ReachabilityError::Cancelled) == ReachabilityError::InvalidGrid, "empty grid is invalid");
	SurfaceGrid g = grid(8, 8, SurfaceKind::Grass);
	g.doors = {3}; // not a Floor cell
	check(reachabilityOf(g, true).error_or(ReachabilityError::Cancelled) == ReachabilityError::InvalidGrid, "door off Floor is invalid");
	g.doors.clear();
	std::stop_source source;
	source.request_stop();
	check(reachabilityOf(g, false, source.get_token()).error_or(ReachabilityError::InvalidGrid) == ReachabilityError::Cancelled, "stop is honoured");
}

// Independent implementation: union-find over the same walkability rule; the largest component is the biggest one, ties
// going to the component whose first cell comes first in row-major order.
struct Reference { uint64_t walkable = 0, largest = 0, cutOff = 0; uint32_t components = 0, pockets = 0, buildings = 0, sealed = 0; };

Reference referenceOf(const SurfaceGrid& g, bool walls) {
	const size_t w = g.region.width, h = g.region.height, n = g.cells.size();
	std::vector<size_t> parent(n);
	std::iota(parent.begin(), parent.end(), 0);
	auto find = [&](size_t i) { while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
	std::vector<char> door(n, 0);
	for (const auto d : g.doors) door[d] = 1;
	auto outline = [&](size_t i) {
		const size_t x = i % w, y = i / w;
		if (x == 0 || y == 0 || x + 1 == w || y + 1 == h) return true;
		return g.cells[i - 1] != SurfaceKind::Floor || g.cells[i + 1] != SurfaceKind::Floor || g.cells[i - w] != SurfaceKind::Floor || g.cells[i + w] != SurfaceKind::Floor;
	};
	std::vector<char> ok(n, 0);
	for (size_t i = 0; i < n; ++i) {
		const auto k = g.cells[i];
		if (k == SurfaceKind::Floor) ok[i] = !walls || door[i] || !outline(i);
		else ok[i] = k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Rock || k == SurfaceKind::Shore || k == SurfaceKind::Path;
	}
	for (size_t i = 0; i < n; ++i) {
		if (!ok[i]) continue;
		if (i % w + 1 < w && ok[i + 1]) parent[find(i)] = find(i + 1);
		if (i + w < n && ok[i + w]) parent[find(i)] = find(i + w);
	}
	std::vector<uint64_t> size(n, 0);
	std::vector<size_t> first(n, n);
	Reference r;
	for (size_t i = 0; i < n; ++i) if (ok[i]) { const size_t root = find(i); ++size[root]; first[root] = std::min(first[root], i); ++r.walkable; }
	size_t best = n;
	for (size_t i = 0; i < n; ++i) {
		if (!size[i]) continue;
		++r.components;
		if (best == n || size[i] > size[best] || (size[i] == size[best] && first[i] < first[best])) best = i;
	}
	r.largest = best == n ? 0 : size[best];
	r.cutOff = r.walkable - r.largest;
	for (size_t i = 0; i < n; ++i) if (size[i] && i != best && size[i] >= reachabilityPocketCells) ++r.pockets;
	// Buildings: Floor components with their own union-find.
	std::vector<size_t> bp(n);
	std::iota(bp.begin(), bp.end(), 0);
	auto bfind = [&](size_t i) { while (bp[i] != i) { bp[i] = bp[bp[i]]; i = bp[i]; } return i; };
	for (size_t i = 0; i < n; ++i) {
		if (g.cells[i] != SurfaceKind::Floor) continue;
		if (i % w + 1 < w && g.cells[i + 1] == SurfaceKind::Floor) bp[bfind(i)] = bfind(i + 1);
		if (i + w < n && g.cells[i + w] == SurfaceKind::Floor) bp[bfind(i)] = bfind(i + w);
	}
	std::vector<char> seen(n, 0), reach(n, 0);
	for (size_t i = 0; i < n; ++i) {
		if (g.cells[i] != SurfaceKind::Floor) continue;
		const size_t root = bfind(i);
		seen[root] = 1;
		if (best != n && ok[i] && find(i) == best && (!walls || door[i])) reach[root] = 1;
	}
	for (size_t i = 0; i < n; ++i) if (seen[i]) { ++r.buildings; if (!reach[i]) ++r.sealed; }
	return r;
}

// 20 x 8: land x 0..7 (64 cells), river x 8..10 (3 wide), land x 11..19 (72 cells, the largest).
//   reach 3: the smallest crossing is the three water cells of row 0 (x 8..10); the water beside them (row 1) is stamped too and
//   both ends get a mouth of radius 3 (rows 0..3 at the top edge), so the ford is x 8..10 by rows 0..3 = 12 cells of sand, the
//   river keeps 12, the land is one component of 64 + 72 + 12 = 148 cells and each end touches the land in 4 cells.
//   reach 2: no crossing is short enough, nothing changes.
SurfaceGrid riverGrid() {
	SurfaceGrid g = grid(20, 8, SurfaceKind::Grass);
	fillRect(g, 8, 0, 10, 7, SurfaceKind::River);
	return g;
}

size_t countKind(const SurfaceGrid& g, SurfaceKind kind) { return size_t(std::count(g.cells.begin(), g.cells.end(), kind)); }

void testFords() {
	SurfaceGrid g = riverGrid();
	check(placeFords(g.cells, 20, 8, 2, g.doors).has_value() && countKind(g, SurfaceKind::Shore) == 0 && countKind(g, SurfaceKind::River) == 24, "reach 2 does not cross three water cells");
	check(placeFords(g.cells, 20, 8, 3, g.doors).has_value(), "reach 3 runs");
	check(countKind(g, SurfaceKind::Shore) == 12 && countKind(g, SurfaceKind::River) == 12, "reach 3 stamps 12 Shore cells");
	bool rowZero = true;
	for (uint32_t x = 8; x <= 10; ++x) rowZero = rowZero && g.cells[x] == SurfaceKind::Shore && g.cells[20 + x] == SurfaceKind::Shore;
	check(rowZero, "the ford is rows 0 and 1");
	const auto r = reachabilityOf(g, false);
	check(r.has_value() && r->components == 1 && r->largest == 148, "one component after the ford");
	SurfaceGrid again = riverGrid();
	check(placeFords(again.cells, 20, 8, 3, again.doors).has_value() && again.cells == g.cells, "deterministic");
	check(placeFords(g.cells, 20, 8, 3, g.doors).has_value() && countKind(g, SurfaceKind::Shore) == 12, "a second pass changes nothing");
}

// Contact of the ford with the land: ford cells (formerly water, now Shore) that touch a land cell of the original grid.
//   20 x 8 river x 8..10: 4 on each side (rows 0..3 at the top edge, see above).
//   20 x 16: land x 11..19 on every row, river x 8..10, left land only rows 6..9 of x 0..7 (32 cells) with river above and below.
//   The crossing runs along row 6; mouths of radius 3 cover rows 3..9. East end: ford cells x 10, rows 3..9 touch the land at
//   x 11 = 7 cells. West end: the land has only rows 6..9 at x 7, so 4. Sand in total: x 8..10 rows 3..9 (21) plus the water of
//   x 5..7 rows 3..5 (9) = 30.
size_t contacts(const SurfaceGrid& before, const SurfaceGrid& after, uint32_t landColumn, uint32_t fordColumn) {
	size_t found = 0;
	const uint32_t width = before.region.width;
	for (uint32_t y = 0; y < before.region.height; ++y) {
		const size_t ford = size_t(y) * width + fordColumn, land = size_t(y) * width + landColumn;
		found += before.cells[ford] == SurfaceKind::River && after.cells[ford] == SurfaceKind::Shore && before.cells[land] == SurfaceKind::Grass;
	}
	return found;
}

void testMouths() {
	const SurfaceGrid before = riverGrid();
	SurfaceGrid after = before;
	check(placeFords(after.cells, 20, 8, 3, after.doors).has_value() && contacts(before, after, 7, 8) == 4 && contacts(before, after, 11, 10) == 4, "20 x 8: 4 contact cells at each end");
	SurfaceGrid big = grid(20, 16, SurfaceKind::Grass);
	fillRect(big, 8, 0, 10, 15, SurfaceKind::River);
	fillRect(big, 0, 0, 7, 5, SurfaceKind::River);
	fillRect(big, 0, 10, 7, 15, SurfaceKind::River);
	const SurfaceGrid bigBefore = big;
	check(placeFords(big.cells, 20, 16, 3, big.doors).has_value(), "20 x 16 ford runs");
	check(countKind(big, SurfaceKind::Shore) == 30, "20 x 16: 30 sand cells");
	check(contacts(bigBefore, big, 11, 10) == 7 && contacts(bigBefore, big, 7, 8) == 4, "20 x 16: 7 contact cells east, 4 west");
}

// Natural ford (iteration 14d): 64 x 24, land x 0..7 and x 56..63 on rows 8..15 only (64 cells each), river everywhere else.
// The crossing is 48 water cells long, so the ford swings and varies in width. Properties, not hand counts:
//   one component afterwards; same seed twice gives the same cells, another seed another shape; only River became Shore;
//   in the middle third (columns 24..40) every column holds 3 to 9 sand cells and the mean row of the sand moves by at least 2
//   rows between columns (the ford is not a straight line).
SurfaceGrid wideSea() {
	SurfaceGrid g = grid(64, 24, SurfaceKind::River);
	fillRect(g, 0, 8, 7, 15, SurfaceKind::Grass);
	fillRect(g, 56, 8, 63, 15, SurfaceKind::Grass);
	return g;
}

void testNaturalFord() {
	const SurfaceGrid before = wideSea();
	SurfaceGrid a = before, again = before, other = before;
	check(placeFords(a.cells, 64, 24, 64, a.doors, {}, FordShape {7, 0, 0}).has_value(), "wide sea: ford runs");
	check(placeFords(again.cells, 64, 24, 64, again.doors, {}, FordShape {7, 0, 0}).has_value() && again.cells == a.cells, "wide sea: same seed, same ford");
	check(placeFords(other.cells, 64, 24, 64, other.doors, {}, FordShape {8, 0, 0}).has_value() && other.cells != a.cells, "wide sea: another seed, another ford");
	const auto joined = reachabilityOf(a, false);
	check(joined.has_value() && joined->components == 1, "wide sea: one component after the ford");
	bool onlyWater = true;
	for (size_t i = 0; i < a.cells.size(); ++i) onlyWater = onlyWater && (a.cells[i] == before.cells[i] || (before.cells[i] == SurfaceKind::River && a.cells[i] == SurfaceKind::Shore));
	check(onlyWater, "wide sea: only river cells became sand");
	double lowest = 1e9, highest = -1e9;
	bool widthsOk = true;
	for (uint32_t x = 24; x <= 40; ++x) {
		size_t sand = 0;
		double rows = 0;
		for (uint32_t y = 0; y < 24; ++y) if (a.cells[size_t(y) * 64 + x] == SurfaceKind::Shore) { ++sand; rows += y; }
		widthsOk = widthsOk && sand >= 3 && sand <= 9;
		if (sand) { lowest = std::min(lowest, rows / double(sand)); highest = std::max(highest, rows / double(sand)); }
	}
	check(widthsOk, "wide sea: 3 to 9 sand cells in every middle column");
	check(highest - lowest >= 2.0, "wide sea: the ford is not a straight line (mean row moves by " + std::to_string(highest - lowest) + ")");
}

// Shore smoothing (iteration 18), hand counted on 12 x 8 grids with Lake x 0..1 (cells within 9 of the lake: x <= 10; ring 0: x = 2),
// 7 x 7 windows clipped to the grid:
//   band of Shore x 2..4: an untouched band stays (band cells (4,3): 28 sand of 49, (4,0): 16 of 28; grass (5,0): 12 of 28, (5,3): 21 of 49);
//   a Grass hole at (3,4): window x 0..6, y 1..7 holds 14 Lake + 20 Shore = 34 sand of 49 -> filled;
//   a lone Shore at (4,3) in Grass (ring 0 at x = 2 only): window x 1..7, y 0..6 holds 7 Lake + ring 0 + itself = 9 sand of 49 -> Grass (Dirt in Dirt);
//   a ring 0 Shore at (2,3) alone in Grass stays.
void testSmoothShores() {
	const auto lakeGrid = [](SurfaceKind land) {
		SurfaceGrid g = grid(12, 8, land);
		fillRect(g, 0, 0, 1, 7, SurfaceKind::Lake);
		return g;
	};
	SurfaceGrid band = lakeGrid(SurfaceKind::Grass);
	fillRect(band, 2, 0, 4, 7, SurfaceKind::Shore);
	const SurfaceGrid bandBefore = band;
	smoothShores(band.cells, 12, 8, 2);
	check(band.cells == bandBefore.cells, "smoothing: an even band stays as it is");

	SurfaceGrid hole = bandBefore;
	hole.cells[4 * 12 + 3] = SurfaceKind::Grass;
	smoothShores(hole.cells, 12, 8, 1);
	check(hole.cells == bandBefore.cells, "smoothing: a hole in the band is filled");

	SurfaceGrid lone = lakeGrid(SurfaceKind::Grass);
	fillRect(lone, 2, 3, 2, 3, SurfaceKind::Shore);
	lone.cells[3 * 12 + 4] = SurfaceKind::Shore;
	smoothShores(lone.cells, 12, 8, 1);
	check(lone.cells[3 * 12 + 4] == SurfaceKind::Grass && lone.cells[3 * 12 + 2] == SurfaceKind::Shore, "smoothing: a lone sand cell turns to grass, ring 0 stays");

	SurfaceGrid loneInDirt = lakeGrid(SurfaceKind::Dirt);
	loneInDirt.cells[3 * 12 + 4] = SurfaceKind::Shore;
	smoothShores(loneInDirt.cells, 12, 8, 1);
	check(loneInDirt.cells[3 * 12 + 4] == SurfaceKind::Dirt, "smoothing: a lone sand cell in dirt turns to dirt");

	SurfaceGrid again = lakeGrid(SurfaceKind::Grass);
	again.cells[3 * 12 + 4] = SurfaceKind::Shore;
	SurfaceGrid second = again;
	smoothShores(again.cells, 12, 8, 3);
	smoothShores(second.cells, 12, 8, 3);
	check(again.cells == second.cells, "smoothing: deterministic");
	SurfaceGrid none = lakeGrid(SurfaceKind::Grass);
	none.cells[3 * 12 + 4] = SurfaceKind::Shore;
	const SurfaceGrid noneBefore = none;
	smoothShores(none.cells, 12, 8, 0);
	check(none.cells == noneBefore.cells, "smoothing: 0 passes change nothing");
}

// Shore smoothing radius (iteration 20), 20 x 15: Lake x 0..1, Shore x 2..8, Grass x 9..19, plus a sand bump of depth 1 and length 5
// at (9, 5..9). Radius 3 (7 x 7 windows) keeps the bump (e.g. (9,6): 26 sand of 49), radius 5 (11 x 11) removes it ((9,6): 60 sand of 121,
// below the majority) and leaves the straight edge alone (e.g. (9,4): 55 of 110, not above half; (8,0): 37 of 66).
void testShoreRadius() {
	SurfaceGrid plain = grid(20, 15, SurfaceKind::Grass);
	fillRect(plain, 0, 0, 1, 14, SurfaceKind::Lake);
	fillRect(plain, 2, 0, 8, 14, SurfaceKind::Shore);
	SurfaceGrid bump = plain;
	fillRect(bump, 9, 5, 9, 9, SurfaceKind::Shore);
	SurfaceGrid narrow = bump;
	smoothShores(narrow.cells, 20, 15, 1, 3);
	check(narrow.cells == bump.cells, "radius 3 keeps a bump of depth 1 and length 5");
	SurfaceGrid zero = bump;
	smoothShores(zero.cells, 20, 15, 1, 0);
	check(zero.cells == narrow.cells, "radius 0 means 3");
	SurfaceGrid wide = bump;
	smoothShores(wide.cells, 20, 15, 1, 5);
	check(wide.cells == plain.cells, "radius 5 removes the bump");
	SurfaceGrid straight = plain;
	smoothShores(straight.cells, 20, 15, 2, 5);
	check(straight.cells == plain.cells, "radius 5 leaves a straight edge alone");
	SurfaceGrid again = bump;
	smoothShores(again.cells, 20, 15, 1, 5);
	check(again.cells == wide.cells, "radius 5 is deterministic");
}

// Spit trimming (iteration 18/19), hand counted on 10 x 5 grids of Lake:
//   Shore x 4..5 over the whole height (2 wide, water both sides): all 10 cells become Lake;
//   Shore x 3..5 (3 wide): stays; a single Shore cell in the water becomes Lake; a run of 2 with Grass at one end stays;
//   a run bounded by River on one side and Lake on the other becomes River.
void testTrimSpits() {
	SurfaceGrid two = grid(10, 5, SurfaceKind::Lake);
	fillRect(two, 4, 0, 5, 4, SurfaceKind::Shore);
	trimSpits(two.cells, 10, 5);
	check(countKind(two, SurfaceKind::Lake) == 50 && countKind(two, SurfaceKind::Shore) == 0, "spits: a 2 wide strip between water becomes water");

	SurfaceGrid three = grid(10, 5, SurfaceKind::Lake);
	fillRect(three, 3, 0, 5, 4, SurfaceKind::Shore);
	const SurfaceGrid threeBefore = three;
	trimSpits(three.cells, 10, 5);
	check(three.cells == threeBefore.cells, "spits: a 3 wide strip stays");

	SurfaceGrid single = grid(10, 5, SurfaceKind::Lake);
	single.cells[2 * 10 + 4] = SurfaceKind::Shore;
	trimSpits(single.cells, 10, 5);
	check(countKind(single, SurfaceKind::Shore) == 0, "spits: a single sand cell in the water becomes water");

	SurfaceGrid grassEnd = grid(10, 5, SurfaceKind::Lake);
	fillRect(grassEnd, 0, 0, 3, 4, SurfaceKind::Grass);
	fillRect(grassEnd, 4, 0, 5, 4, SurfaceKind::Shore);
	const SurfaceGrid grassEndBefore = grassEnd;
	trimSpits(grassEnd.cells, 10, 5);
	check(grassEnd.cells == grassEndBefore.cells, "spits: a strip with grass at one end stays");

	SurfaceGrid river = grid(10, 5, SurfaceKind::Lake);
	fillRect(river, 0, 0, 3, 4, SurfaceKind::River);
	fillRect(river, 4, 0, 5, 4, SurfaceKind::Shore);
	trimSpits(river.cells, 10, 5);
	check(countKind(river, SurfaceKind::Shore) == 0 && countKind(river, SurfaceKind::River) == 20 + 10, "spits: a strip between river and lake becomes river");
}

// POI reachability from a fixed start (iteration 22), hand counted.
//   9 x 5 grass with a lake column at x = 4: the middle (4,2) is water; (3,2) and (5,2) tie, the lower index (3,2) is the start.
//   Exits: north (3,0) (x = 3 and 5 tie, lower index), south (3,4), west (0,2), east (8,2): 4 exits, the east one is across the lake.
//   Component of the start: 4 x 5 = 20 of 40 walkable cells.
void testPoi() {
	SurfaceGrid lake = grid(9, 5, SurfaceKind::Grass);
	fillRect(lake, 4, 0, 4, 4, SurfaceKind::Lake);
	auto r = poiReachabilityOf(lake, false);
	check(r.has_value() && r->hasStart && r->start == 2 * 9 + 3, "poi: the start is the lower index of two nearest cells");
	check(r.has_value() && r->entrances == 0 && r->exits == 4 && r->exitsReached == 3, "poi: four exits, three reached");
	check(r.has_value() && r->walkable == 40 && r->startComponent == 20, "poi: the start component holds 20 of 40 cells");

	// 12 x 7 land with a building x 2..6, y 1..5 (interior x 3..5, y 2..4); the middle (5.5, 3) has (5,3) and (6,3) at the same distance;
	// (5,3) is inside the building. With a door at (4,5) everything is one component; without one the start sits in a sealed room.
	SurfaceGrid house = grid(12, 7, SurfaceKind::Grass);
	fillRect(house, 2, 1, 6, 5, SurfaceKind::Floor);
	house.doors = {5 * 12 + 4};
	r = poiReachabilityOf(house, true);
	check(r.has_value() && r->start == 3 * 12 + 5 && r->entrances == 1 && r->entrancesReached == 1 && r->exits == 4 && r->exitsReached == 4, "poi: door joins the start room with the outside");
	house.doors.clear();
	r = poiReachabilityOf(house, true);
	check(r.has_value() && r->start == 3 * 12 + 5 && r->entrances == 0 && r->exits == 4 && r->exitsReached == 0 && r->startComponent == 9, "poi: a sealed start reaches no exit");

	SurfaceGrid northWater = grid(6, 4, SurfaceKind::Grass);
	fillRect(northWater, 0, 0, 5, 0, SurfaceKind::Lake);
	r = poiReachabilityOf(northWater, false);
	check(r.has_value() && r->exits == 3 && r->exitsReached == 3, "poi: a side without land has no exit");

	SurfaceGrid sea = grid(5, 5, SurfaceKind::Lake);
	r = poiReachabilityOf(sea, false);
	check(r.has_value() && !r->hasStart && r->exits == 0 && r->entrances == 0, "poi: no start in an all water grid");
	SurfaceGrid bad;
	check(poiReachabilityOf(bad, false).error_or(ReachabilityError::Cancelled) == ReachabilityError::InvalidGrid, "poi: empty grid is invalid");
}

// Repair (iteration 23), 24 x 9: Grass x 0..12 (the start is (11,4): the middle (11.5, 4) ties between x = 11 and 12, lower index),
// River x 13..18 (6 wide), Grass x 19..23 with a 3 x 3 building Floor x 20..22, y 2..4 and its door at (21,4).
//   Exits: north (11,0), south (11,8), west (0,4) on the start's side; east (23,4) and the door lie across the river.
//   A normal ford of reach 3 cannot cross 6 water cells; repair (reach 256) joins the right bank: crossing row 0 x 13..18, mouths of radius 3
//   cover rows 0..3, so 6 x 4 = 24 sand cells and 54 - 24 = 30 river cells remain; afterwards all 1 + 4 POIs are reached.
SurfaceGrid repairGrid() {
	SurfaceGrid g = grid(24, 9, SurfaceKind::Grass);
	fillRect(g, 13, 0, 18, 8, SurfaceKind::River);
	fillRect(g, 20, 2, 22, 4, SurfaceKind::Floor);
	g.doors = {4 * 24 + 21};
	return g;
}

void testRepair() {
	SurfaceGrid g = repairGrid();
	auto before = poiReachabilityOf(g, true);
	check(before.has_value() && before->start == 4 * 24 + 11 && before->entrances == 1 && before->entrancesReached == 0 && before->exits == 4 && before->exitsReached == 3, "repair: before, 3 of 4 exits and no entrance reached");
	check(placeFords(g.cells, 24, 9, 3, g.doors).has_value() && countKind(g, SurfaceKind::Shore) == 0, "repair: a normal ford of reach 3 does nothing");
	check(placeFords(g.cells, 24, 9, 256, g.doors, {}, FordShape {1, 0, 0, true}).has_value(), "repair: runs");
	check(countKind(g, SurfaceKind::Shore) == 24 && countKind(g, SurfaceKind::River) == 30, "repair: 24 sand cells, 30 river cells");
	const auto after = poiReachabilityOf(g, true);
	check(after.has_value() && after->entrancesReached == 1 && after->exitsReached == 4, "repair: everything is reached afterwards");
	SurfaceGrid again = repairGrid();
	check(placeFords(again.cells, 24, 9, 256, again.doors, {}, FordShape {1, 0, 0, true}).has_value() && again.cells == g.cells, "repair: deterministic");

	// An island with nothing on it stays alone; a tiny room (door + one interior cell) in the water gets its ford.
	SurfaceGrid island = grid(24, 9, SurfaceKind::River);
	fillRect(island, 0, 0, 12, 8, SurfaceKind::Grass);
	fillRect(island, 16, 3, 17, 4, SurfaceKind::Grass);
	check(placeFords(island.cells, 24, 9, 256, island.doors, {}, FordShape {1, 0, 0, true}).has_value() && countKind(island, SurfaceKind::Shore) == 0, "repair: an island without a door or exit is left alone");
	SurfaceGrid room = grid(24, 9, SurfaceKind::River);
	fillRect(room, 0, 0, 12, 8, SurfaceKind::Grass);
	fillRect(room, 16, 3, 18, 5, SurfaceKind::Floor);
	room.doors = {5 * 24 + 17};
	check(poiReachabilityOf(room, true)->entrancesReached == 0, "repair: the room is cut off before");
	check(placeFords(room.cells, 24, 9, 256, room.doors, {}, FordShape {1, 0, 0, true}).has_value(), "repair: the room case runs");
	const auto joined = poiReachabilityOf(room, true);
	check(joined.has_value() && joined->entrances == 1 && joined->entrancesReached == 1, "repair: a 2 cell room with a door gets its ford");
}

// Ruins and camps (iteration 25), hand counted on 12 x 7 grass with a building Floor x 2..6, y 1..5 (outline 16 cells, interior x 3..5, y 2..4):
//   no door, no gap: sealed; the same building with a gap at (4,5): the gap cell is walkable, the building is reachable (1 component, 69 cells);
//   the site (4,3) is reached; a gap or a site outside Floor makes the grid invalid; the fingerprint changes with the gap; reachabilityOf and the
//   POI report agree (0 entrances, 1 site, 1 reached).
void testRuin() {
	SurfaceGrid g = grid(12, 7, SurfaceKind::Grass);
	fillRect(g, 2, 1, 6, 5, SurfaceKind::Floor);
	g.sites = {3 * 12 + 4};
	const uint64_t plain = g.fingerprint();
	auto r = reachabilityOf(g, true);
	check(r.has_value() && r->buildings == 1 && r->sealed == 1, "ruin: no door and no gap means sealed");
	auto poi = poiReachabilityOf(g, true);
	check(poi.has_value() && poi->entrances == 0 && poi->sites == 1, "ruin: no entrance, one site");
	g.gaps = {5 * 12 + 4};
	r = reachabilityOf(g, true);
	check(r.has_value() && r->sealed == 0 && r->components == 1 && r->walkable == 69, "ruin: a gap makes the building reachable");
	poi = poiReachabilityOf(g, true);
	check(poi.has_value() && poi->sites == 1 && poi->sitesReached == 1 && poi->entrances == 0, "ruin: the site is reached through the gap");
	check(g.fingerprint() != plain, "ruin: the gap changes the fingerprint");
	check(g.valid(), "ruin: a grid with a gap and a site on Floor is valid");
	SurfaceGrid bad = g;
	bad.gaps = {0};
	check(!bad.valid(), "ruin: a gap off Floor is invalid");
	bad = g;
	bad.sites = {0};
	check(!bad.valid(), "ruin: a site off Floor is invalid");
}

// Independent check: brute force start, flood fill from it over the same walkability rule.
void testPoiCrossCheck() {
	uint64_t state = 0x9e3779b97f4a7c15ULL;
	auto next = [&] { state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; };
	const SurfaceKind kinds[] = {SurfaceKind::Grass, SurfaceKind::Grass, SurfaceKind::Dirt, SurfaceKind::Rock, SurfaceKind::Shore, SurfaceKind::Lake,
		SurfaceKind::River, SurfaceKind::Path, SurfaceKind::Floor, SurfaceKind::Floor, SurfaceKind::Floor, SurfaceKind::Preserve};
	int matched = 0;
	for (int round = 0; round < 80; ++round) {
		const uint32_t w = 9 + uint32_t(next() % 8), h = 9 + uint32_t(next() % 8);
		SurfaceGrid g = grid(w, h, SurfaceKind::Grass);
		for (auto& c : g.cells) c = kinds[next() % std::size(kinds)];
		for (size_t i = 0; i < g.cells.size(); ++i) if (g.cells[i] == SurfaceKind::Floor && next() % 5 == 0) g.doors.push_back(uint32_t(i));
		for (const bool walls : {false, true}) {
			const size_t n = g.cells.size();
			auto outline = [&](size_t i) {
				const size_t x = i % w, y = i / w;
				return x == 0 || y == 0 || x + 1 == w || y + 1 == h || g.cells[i - 1] != SurfaceKind::Floor || g.cells[i + 1] != SurfaceKind::Floor || g.cells[i - w] != SurfaceKind::Floor || g.cells[i + w] != SurfaceKind::Floor;
			};
			std::vector<char> door(n, 0), ok(n, 0);
			for (const auto d : g.doors) door[d] = 1;
			for (size_t i = 0; i < n; ++i) {
				const auto k = g.cells[i];
				if (k == SurfaceKind::Floor) ok[i] = !walls || door[i] || !outline(i);
				else ok[i] = k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Rock || k == SurfaceKind::Shore || k == SurfaceKind::Path;
			}
			long long bestD = -1; size_t start = n;
			for (size_t i = 0; i < n; ++i) {
				if (!ok[i]) continue;
				const long long dx = 2LL * (long long)(i % w) - (long long)(w - 1), dy = 2LL * (long long)(i / w) - (long long)(h - 1);
				if (bestD < 0 || dx * dx + dy * dy < bestD) { bestD = dx * dx + dy * dy; start = i; }
			}
			const auto got = poiReachabilityOf(g, walls);
			if (!got) { check(false, "poi cross-check: result"); continue; }
			if (start == n) { check(!got->hasStart, "poi cross-check: no start"); ++matched; continue; }
			std::vector<char> seen(n, 0);
			std::vector<size_t> queue {start};
			seen[start] = 1;
			for (size_t head = 0; head < queue.size(); ++head) {
				const size_t i = queue[head];
				const size_t x = i % w, y = i / w;
				const size_t around[4] = {x > 0 ? i - 1 : n, x + 1 < w ? i + 1 : n, y > 0 ? i - w : n, y + 1 < h ? i + w : n};
				for (const size_t m : around) if (m != n && ok[m] && !seen[m]) { seen[m] = 1; queue.push_back(m); }
			}
			uint32_t entrances = 0, entrancesReached = 0, exits = 0, exitsReached = 0;
			for (const auto d : g.doors) { ++entrances; entrancesReached += seen[d]; }
			for (int side = 0; side < 4; ++side) {
				const long long length = side < 2 ? w : h;
				long long best = -1, bestOffset = 0;
				for (long long k = 0; k < length; ++k) {
					const long long x = side == 0 ? k : side == 1 ? k : side == 2 ? 0 : (long long)w - 1;
					const long long y = side == 0 ? 0 : side == 1 ? (long long)h - 1 : k;
					const size_t cell = size_t(y * (long long)w + x);
					if (!ok[cell]) continue;
					const long long offset = std::llabs(2 * k - (length - 1));
					if (best < 0 || offset < bestOffset) { best = (long long)cell; bestOffset = offset; }
				}
				if (best >= 0) { ++exits; exitsReached += seen[size_t(best)]; }
			}
			uint64_t component = 0;
			for (size_t i = 0; i < n; ++i) component += seen[i];
			const bool same = got->hasStart && got->start == start && got->entrances == entrances && got->entrancesReached == entrancesReached &&
				got->exits == exits && got->exitsReached == exitsReached && got->startComponent == component;
			check(same, "poi cross-check round " + std::to_string(round) + (walls ? " walls" : " open"));
			matched += same;
		}
	}
	check(matched == 160, "all 160 poi comparisons matched");
}

// A pocket of 12 cells (< 16) is not worth a ford: land x 0..2 (3 x 4), river x 3..5, land x 6..19.
void testSmallPocket() {
	SurfaceGrid g = grid(20, 4, SurfaceKind::Grass);
	fillRect(g, 3, 0, 5, 3, SurfaceKind::River);
	check(placeFords(g.cells, 20, 4, 64, g.doors).has_value() && countKind(g, SurfaceKind::Shore) == 0, "pockets below 16 cells get no ford");
}

// Lakes are crossed like rivers; a sealed pocket behind rock-free water that is wider than reach stays alone.
void testLakeAndFar() {
	SurfaceGrid g = grid(20, 8, SurfaceKind::Grass);
	fillRect(g, 8, 0, 11, 7, SurfaceKind::Lake); // 4 wide: land 0..7 (64) and 12..19 (64); tie goes to the first
	check(placeFords(g.cells, 20, 8, 3, g.doors).has_value() && countKind(g, SurfaceKind::Shore) == 0, "four wide lake with reach 3 stays open");
	check(placeFords(g.cells, 20, 8, 4, g.doors).has_value() && countKind(g, SurfaceKind::Shore) == 16 && countKind(g, SurfaceKind::Lake) == 16, "four wide lake with reach 4 gets 16 Shore cells");
}

void testCrossCheck() {
	uint64_t state = 88172645463325252ULL;
	auto next = [&] { state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; };
	const SurfaceKind kinds[] = {SurfaceKind::Grass, SurfaceKind::Grass, SurfaceKind::Dirt, SurfaceKind::Rock, SurfaceKind::Shore, SurfaceKind::Lake,
		SurfaceKind::River, SurfaceKind::Path, SurfaceKind::Floor, SurfaceKind::Floor, SurfaceKind::Floor, SurfaceKind::Preserve};
	int compared = 0;
	for (int round = 0; round < 80; ++round) {
		SurfaceGrid g = grid(16, 16, SurfaceKind::Grass);
		for (auto& c : g.cells) c = kinds[next() % std::size(kinds)];
		for (size_t i = 0; i < g.cells.size(); ++i) if (g.cells[i] == SurfaceKind::Floor && next() % 5 == 0) g.doors.push_back(uint32_t(i));
		for (const bool walls : {false, true}) {
			const auto got = reachabilityOf(g, walls);
			const Reference want = referenceOf(g, walls);
			const bool same = got.has_value() && got->walkable == want.walkable && got->components == want.components && got->largest == want.largest &&
				got->cutOff == want.cutOff && got->pockets == want.pockets && got->buildings == want.buildings && got->sealed == want.sealed;
			check(same, "cross-check round " + std::to_string(round) + (walls ? " walls" : " open"));
			compared += same;
			const auto again = reachabilityOf(g, walls);
			check(got.has_value() && again.has_value() && got->components == again->components && got->sealed == again->sealed && got->largest == again->largest, "deterministic");
		}
	}
	check(compared == 160, "all 160 comparisons matched");
}
void testLevels() {
	LevelPlan p {{10, 20, 7, 5, 5}};
	for (int z : {7, 8, 6, 5, 4}) p.floors.push_back({z, std::vector<uint8_t>(25, 1)});
	p.edges = {{{0, 12}, {1, 12}}, {{1, 13}, {0, 14}}, {{0, 1}, {2, 2}}, {{2, 3}, {3, 4}}, {{3, 5}, {4, 6}}};
	for (uint32_t f = 0; f < 5; ++f) for (uint32_t i = 0; i < 25; ++i)
		p.pois.push_back({f == 1 ? LevelPoiKind::CaveCell : LevelPoiKind::Cap, {f, i}});
	auto r = levelReachabilityOf(p, {});
	check(r && r->allReached() && r->walkableEdges == 5, "all five floors and all Z transition kinds");
	p.floors[3].walkable.assign(25, 0); p.floors[3].walkable[24] = 1;
	r = levelReachabilityOf(p, {});
	check(r && !r->allReached() && r->pockets[3] == 1 && r->pockets[4] == 1, "isolated z-2 pocket and disconnected z-3");
	check(r && r->categories[size_t(LevelPoiKind::Cap)].reached == 50 &&
		r->categories[size_t(LevelPoiKind::Cap)].total == 76 && r->firstUnreachable.size() == 16 &&
		r->firstUnreachable.front().z == 4 && r->firstUnreachable.front().x == 10 &&
		r->firstUnreachable.front().y == 20, "hand-counted cap category and bounded category/z/y/x report order");
	LevelPlan d {{0, 0, 7, 3, 1}, {{7, {1, 1, 1}, {DecorKind::None, DecorKind::Stone, DecorKind::None}}}};
	d.pois.push_back({LevelPoiKind::Door, {0, 2}});
	r = levelReachabilityOf(d, {true, true, true});
	check(r && !r->allReached() && !r->walkable[1], "conservative mixed decoration blocks");
	r = levelReachabilityOf(d, {true, true, true}, std::vector<uint8_t>(3));
	check(r && r->allReached(), "actual nonblocking variant overrides class");
	d.floors[0].walkable[1] = 0;
	r = levelReachabilityOf(d, {}, std::vector<uint8_t>(3));
	check(r && !r->walkable[1], "mask cannot open slope or wall");
	d.pois.push_back({LevelPoiKind::Site, {99, 0}});
	check(!levelReachabilityOf(d, {}), "invalid POI rejected before sorting");
	std::stop_source cancel; cancel.request_stop();
	check(!levelReachabilityOf(p, {}, {}, cancel.get_token()), "level cancellation");
	auto s = grid(7, 7, SurfaceKind::Grass);
	fillRect(s, 1, 1, 5, 5, SurfaceKind::Floor);
	s.doors = {3}; // invalid door outside Floor
	check(!levelPlanOf(s, true, false, nullptr, {}, {}, {}), "semantic invalid surface");
	s.doors = {10};
	auto plan = levelPlanOf(s, true, false, nullptr, {}, {}, {});
	check(plan && plan->floors[0].walkable[10] && !plan->floors[0].walkable[8] && plan->floors[0].walkable[24], "door, wall and interior semantics");
	s = grid(7, 7, SurfaceKind::Rock);
	plan = levelPlanOf(s, false, true, nullptr, {}, {}, {});
	check(plan && std::ranges::count(plan->floors[0].walkable, 1) == 0, "blocking client rock");
	plan = levelPlanOf(s, false, false, nullptr, {}, {}, {});
	check(plan && std::ranges::count(plan->floors[0].walkable, 1) == 49, "open rocky ground");
	s.entrances = {24};
	CaveGrid cave;
	cave.region = s.region; ++cave.region.z;
	cave.cells.assign(49, CaveKind::Floor);
	constexpr ExitShape shapes[] = {{ExitDirection::North, 0, 1, true}};
	CaveExitPlan exits; exits.exits.push_back({0, 24, 0});
	std::vector<PlateauGrid> caps;
	for (int z : {6, 5, 4}) {
		PlateauGrid cap;
		cap.region = s.region; cap.region.z = z;
		cap.cells.assign(49, PlateauKind::None); cap.landings.assign(49, 0);
		cap.cells[25] = PlateauKind::Top;
		cap.ramps.push_back({24, 25, 0, 0});
		caps.push_back(std::move(cap));
	}
	plan = levelPlanOf(s, false, false, &cave, exits, shapes, caps);
	check(plan && plan->edges.size() == 5 && plan->floors.size() == 5, "semantic cave entrance, exit and three cap edges");
	check(plan && !plan->floors[0].walkable[25] && !plan->floors[2].walkable[25] &&
		plan->floors[2].walkable[24] && plan->floors[4].walkable[25], "same-xy supporting slopes and upper openings");
}

// Independent graph traversal: deliberately scans pairs and Z edges instead of
// sharing the production neighbour builder, CSR storage or start selection.
void testLevelCrossCheck() {
	uint64_t random = 1729;
	const auto next = [&]() { random = random * 6364136223846793005ULL + 1; return random >> 32; };
	for (int round = 0; round < 100; ++round) {
		LevelPlan p {{100, 200, 7, 7, 6}};
		for (int z : {7, 8, 6, 5, 4}) {
			LevelFloor f {z, std::vector<uint8_t>(42)};
			for (auto& cell : f.walkable) cell = next() % 3 != 0;
			p.floors.push_back(std::move(f));
		}
		for (uint32_t f : {1, 2, 3, 4}) for (int e = 0; e < 4; ++e)
			p.edges.push_back({{f < 3 ? 0u : f - 1, uint32_t(next() % 42)}, {f, uint32_t(next() % 42)}});
		std::vector<uint8_t> mask(210), open(210), seen(210);
		for (size_t i = 0; i < 210; ++i) {
			mask[i] = next() % 7 == 0;
			open[i] = p.floors[i / 42].walkable[i % 42] && !mask[i];
			p.pois.push_back({LevelPoiKind(i % levelPoiKindCount), {uint32_t(i / 42), uint32_t(i % 42)}});
		}
		std::optional<uint32_t> start;
		int best = std::numeric_limits<int>::max();
		for (uint32_t i = 0; i < 42; ++i) if (open[i]) {
			const int x = 2 * int(i % 7) - 6, y = 2 * int(i / 7) - 5;
			if (x*x + y*y < best) { best = x*x + y*y; start = i; }
		}
		std::vector<uint32_t> queue;
		if (start) { queue.push_back(*start); seen[*start] = 1; }
		for (size_t h = 0; h < queue.size(); ++h) for (uint32_t j = 0; j < 210; ++j) {
			if (!open[j] || seen[j]) continue;
			const auto i = queue[h];
			bool edge = i / 42 == j / 42 && std::abs(int(i % 7) - int(j % 7)) + std::abs(int((i % 42) / 7) - int((j % 42) / 7)) == 1;
			for (const auto& e : p.edges) {
				const auto a = e.a.floor * 42 + e.a.cell, b = e.b.floor * 42 + e.b.cell;
				edge |= (i == a && j == b) || (i == b && j == a);
			}
			if (edge) { seen[j] = 1; queue.push_back(j); }
		}
		const auto got = levelReachabilityOf(p, {}, mask), again = levelReachabilityOf(p, {}, mask);
		check(got && got->start == start && got->walkable == open && got->reached == seen, std::format("3D independent cross-check {}", round));
		check(got && again && got->reached == again->reached && got->pockets == again->pockets, "3D deterministic");
	}
	std::cout << "LEVEL_REACHABILITY_PASS randomPlans=100\n";
}
} // namespace

void testRepairPaths() {
    uint64_t random = 37;
    const auto next = [&]() { random = random * 6364136223846793005ULL + 1; return uint32_t(random >> 32); };
    for (uint32_t round = 0; round < 120; ++round) {
        constexpr uint32_t w = 8, h = 7, n = w * h;
        const uint32_t limit = round % 5;
        std::vector<uint8_t> costs(n), sources(n), goals(n);
        for (auto& c : costs) { constexpr uint8_t kinds[] = {0, 1, 1, 3, 6}; c = kinds[next() % 5]; }
        sources[next() % n] = 1; goals[next() % n] = 1;
        // Independent expanded graph with a linear minimum scan. No production
        // heap, compact state encoding, neighbour builder or predecessor code.
        const auto stride = limit + 1;
        std::vector<uint64_t> d(n * stride, UINT64_MAX);
        std::vector<bool> used(d.size());
        for (uint32_t i = 0; i < n; ++i) if (sources[i] && costs[i] && (costs[i] != 6 || limit))
            d[i * stride + (costs[i] == 6)] = costs[i] == 1 ? 0 : costs[i];
        uint64_t best = UINT64_MAX;
        for (size_t step = 0; step < d.size(); ++step) {
            size_t at = SIZE_MAX;
            for (size_t j = 0; j < d.size(); ++j) if (!used[j] && d[j] != UINT64_MAX && (at == SIZE_MAX || d[j] < d[at])) at = j;
            if (at == SIZE_MAX) break;
            used[at] = true;
            const auto i = uint32_t(at / stride), run = uint32_t(at % stride);
            if (goals[i]) best = std::min(best, d[at]);
            for (uint32_t j = 0; j < n; ++j) {
                if (!costs[j] || std::abs(int(i % w) - int(j % w)) + std::abs(int(i / w) - int(j / w)) != 1) continue;
                const auto r = costs[j] == 6 ? run + 1 : 0;
                if (r <= limit) d[j * stride + r] = std::min(d[j * stride + r], d[at] + costs[j]);
            }
        }
        const auto path = cheapestLevelPath(costs, w, h, sources, goals, limit);
        const auto again = cheapestLevelPath(costs, w, h, sources, goals, limit);
        check(path && (path->cells.empty() ? UINT64_MAX : path->cost) == best, std::format("independent Dijkstra {}", round));
        check(path && again && path->cells == again->cells, "repair path deterministic");
        if (path && !path->cells.empty()) {
            check(sources[path->cells.front()] && goals[path->cells.back()], "path endpoints");
            uint32_t run = 0;
            for (size_t j = 0; j < path->cells.size(); ++j) {
                const auto i = path->cells[j];
                run = costs[i] == 6 ? run + 1 : 0;
                check(costs[i] && run <= limit, "path respects forbidden cells and water limit");
                if (j) { const auto previous = path->cells[j - 1]; check(std::abs(int(i % w) - int(previous % w)) + std::abs(int(i / w) - int(previous / w)) == 1, "path 4-neighbours"); }
            }
        }
    }
    std::cout << "LEVEL_REPAIR_DIJKSTRA_PASS randomPlans=120\n";
}

void testLevelRepair() {
    const auto make = [] {
        LevelWorld world;
        world.surface = grid(9, 7, SurfaceKind::Grass);
        world.decor.push_back({world.surface.region, std::vector<DecorKind>(63)});
        return world;
    };
    auto world = make();
    // Center start on the left bank, target is the interior of a camp on the right.
    for (uint32_t y = 0; y < 7; ++y) world.surface.cells[y * 9 + 5] = SurfaceKind::River;
    world.surface.cells[35] = SurfaceKind::Floor; world.surface.sites = {35};
    auto repaired = repairLevelReach(world, false, false, {}, {}, 1, true);
    check(repaired && repaired->bridges == 2 && !repaired->unrepaired, "bridge across river with clipped water-border shoulder");
    auto ford = make();
    for (uint32_t y = 0; y < 7; ++y) ford.surface.cells[y * 9 + 5] = SurfaceKind::River;
    ford.surface.cells[35] = SurfaceKind::Floor; ford.surface.sites = {35};
    repaired = repairLevelReach(ford, false, false, {}, {}, 1, false);
    check(repaired && repaired->bridges == 0 && std::ranges::find(ford.surface.cells, SurfaceKind::Bridge) == ford.surface.cells.end(), "sand fallback");
    world = make();
    for (uint32_t y = 0; y < 7; ++y) world.decor[0].cells[y * 9 + 5] = DecorKind::Tree;
    world.surface.cells[35] = SurfaceKind::Floor; world.surface.sites = {35};
    repaired = repairLevelReach(world, false, false, {}, {true, false, false}, 0, true);
    check(repaired && repaired->removedDecor == 1 && !repaired->unrepaired, "clear one blocking tree");
    world = make();
    // A cap without a ramp is removed. Its surface support must become open rock.
    PlateauGrid cap; cap.region = world.surface.region; --cap.region.z;
    cap.cells.resize(63); cap.landings.resize(63); cap.cells[35] = PlateauKind::Top; cap.components = 1;
    world.surface.cells[35] = SurfaceKind::Rock; world.caps.push_back(cap);
    world.decor.push_back({cap.region, std::vector<DecorKind>(63)});
    repaired = repairLevelReach(world, false, false, {}, {}, 0, true);
    const auto p = levelPlanOf(world.surface, false, false, nullptr, {}, {}, world.caps, world.decor);
    check(repaired && repaired->removedCaps == 1 && p && p->floors[0].walkable[35], "remove cap and restore rock");
    world = make();
    cap.cells.assign(63, PlateauKind::None); cap.cells[30] = cap.cells[31] = PlateauKind::Top;
    world.surface.cells[30] = world.surface.cells[31] = SurfaceKind::Rock;
    world.caps.push_back(cap); world.decor.push_back({cap.region, std::vector<DecorKind>(63)});
    --cap.region.z; cap.cells[30] = PlateauKind::None;
    world.caps.push_back(cap); world.decor.push_back({cap.region, std::vector<DecorKind>(63)});
    repaired = repairLevelReach(world, false, false, {}, {}, 0, true);
    check(repaired && repaired->removedCaps == 2 && world.caps[0].cells[30] == PlateauKind::None &&
        world.caps[1].cells[31] == PlateauKind::None, "remove unsupported higher cap with its lower cap");
    world = make();
    for (uint32_t y = 0; y < 7; ++y) world.surface.cells[y * 9 + 5] = SurfaceKind::Preserve;
    world.surface.cells[35] = SurfaceKind::Floor; world.surface.sites = {35};
    repaired = repairLevelReach(world, false, false, {}, {}, 0, true);
    check(repaired && repaired->removedPois == 1 && world.surface.sites.empty(), "remove impossible POI");
    // Forbidden wall costs are never traversed, even when cheaper than water.
    std::vector<uint8_t> costs {1, 0, 1}, source {1, 0, 0}, goal {0, 0, 1};
    check(cheapestLevelPath(costs, 3, 1, source, goal, 256)->cells.empty(), "wall and slope forbidden");
    world = {};
    world.surface = grid(205, 5, SurfaceKind::Preserve);
    world.decor.push_back({world.surface.region, std::vector<DecorKind>(1025)});
    for (uint32_t x = 1; x < 205; x += 2) {
        world.surface.cells[410 + x] = SurfaceKind::Floor; world.surface.sites.push_back(410 + x);
    }
    repaired = repairLevelReach(world, false, false, {}, {}, 0, true);
    check(repaired && repaired->repairs == 64 && repaired->removedPois == 64 && repaired->unrepaired, "64 repair limit");
    // Mountain generation precedes the entrance filter; every retained entrance
    // and ramp arrival must have an open supporting surface cell.
    auto entranceSurface = grid(25, 25, SurfaceKind::Grass);
    for (uint32_t y = 4; y <= 20; ++y) for (uint32_t x = 4; x <= 20; ++x) entranceSurface.cells[y * 25 + x] = SurfaceKind::Rock;
    const uint32_t entry = 20 * 25 + 12;
    entranceSurface.entrances = {entry};
    check(caveEntranceAllowed(entranceSurface.cells, 25, 25, entry, CaveEntranceStyle::RockFace), "manual entrance initially legal");
    SurfaceTuning tuning; tuning.caveEntrances = 1; tuning.plateauMinTop = 64;
    constexpr ExitShape shapes[] = {{ExitDirection::North, 0, 1, true}};
    auto built = buildLevelWorld(entranceSurface, {}, 1729, tuning, {}, shapes, shapes, false);
    check(built && !built->caps.empty() && built->caps[0].cells[entry] == PlateauKind::Top &&
        built->surface.entrances.empty(), "manual entrance under generated cap is filtered");
    std::cout << "LEVEL_REPAIR_HAND_PASS\n";
}

int main() {
    testRepairPaths();
    testLevelRepair();
	testLevels();
	testLevelCrossCheck();
	testLakeSplit();
	testBuilding();
	testDoorIntoWater();
	testPreserve();
	testErrors();
	testFords();
	testMouths();
	testNaturalFord();
	testPoi();
	testRepair();
	testRuin();
	testPoiCrossCheck();
	testSmoothShores();
	testTrimSpits();
	testShoreRadius();
	testSmallPocket();
	testLakeAndFar();
	testCrossCheck();
	if (failures) { std::cerr << failures << " failures\n"; return 1; }
	std::cout << "REACHABILITY_PASS\n";
	return 0;
}
