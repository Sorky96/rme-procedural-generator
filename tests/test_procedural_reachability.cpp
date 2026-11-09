// SPDX-License-Identifier: GPL-3.0-or-later
// Reachability of a generated surface (G4 iteration 13). Hand-counted layouts plus a cross-check against an
// independent union-find implementation on random grids.
#include "procedural/core/reachability.h"

#include <algorithm>
#include <iostream>
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
} // namespace

int main() {
	testLakeSplit();
	testBuilding();
	testDoorIntoWater();
	testPreserve();
	testErrors();
	testFords();
	testMouths();
	testNaturalFord();
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
