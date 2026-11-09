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
//   reach 3: the smallest crossing is the three water cells of row 0 (x 8..10); the water beside them (row 1) is stamped too,
//   so 6 cells become Path, the river keeps 18 and the land is one component.
//   reach 2: no crossing is short enough, nothing changes.
SurfaceGrid riverGrid() {
	SurfaceGrid g = grid(20, 8, SurfaceKind::Grass);
	fillRect(g, 8, 0, 10, 7, SurfaceKind::River);
	return g;
}

size_t countKind(const SurfaceGrid& g, SurfaceKind kind) { return size_t(std::count(g.cells.begin(), g.cells.end(), kind)); }

void testFords() {
	SurfaceGrid g = riverGrid();
	check(placeFords(g.cells, 20, 8, 2, g.doors).has_value() && countKind(g, SurfaceKind::Path) == 0 && countKind(g, SurfaceKind::River) == 24, "reach 2 does not cross three water cells");
	check(placeFords(g.cells, 20, 8, 3, g.doors).has_value(), "reach 3 runs");
	check(countKind(g, SurfaceKind::Path) == 6 && countKind(g, SurfaceKind::River) == 18, "reach 3 stamps 6 Path cells");
	bool rowZero = true;
	for (uint32_t x = 8; x <= 10; ++x) rowZero = rowZero && g.cells[x] == SurfaceKind::Path && g.cells[20 + x] == SurfaceKind::Path;
	check(rowZero, "the ford is rows 0 and 1");
	const auto r = reachabilityOf(g, false);
	check(r.has_value() && r->components == 1 && r->largest == 160 - 18, "one component after the ford");
	SurfaceGrid again = riverGrid();
	check(placeFords(again.cells, 20, 8, 3, again.doors).has_value() && again.cells == g.cells, "deterministic");
	check(placeFords(g.cells, 20, 8, 3, g.doors).has_value() && countKind(g, SurfaceKind::Path) == 6, "a second pass changes nothing");
}

// A pocket of 12 cells (< 16) is not worth a ford: land x 0..2 (3 x 4), river x 3..5, land x 6..19.
void testSmallPocket() {
	SurfaceGrid g = grid(20, 4, SurfaceKind::Grass);
	fillRect(g, 3, 0, 5, 3, SurfaceKind::River);
	check(placeFords(g.cells, 20, 4, 64, g.doors).has_value() && countKind(g, SurfaceKind::Path) == 0, "pockets below 16 cells get no ford");
}

// Lakes are crossed like rivers; a sealed pocket behind rock-free water that is wider than reach stays alone.
void testLakeAndFar() {
	SurfaceGrid g = grid(20, 8, SurfaceKind::Grass);
	fillRect(g, 8, 0, 11, 7, SurfaceKind::Lake); // 4 wide: land 0..7 (64) and 12..19 (64); tie goes to the first
	check(placeFords(g.cells, 20, 8, 3, g.doors).has_value() && countKind(g, SurfaceKind::Path) == 0, "four wide lake with reach 3 stays open");
	check(placeFords(g.cells, 20, 8, 4, g.doors).has_value() && countKind(g, SurfaceKind::Path) == 8 && countKind(g, SurfaceKind::Lake) == 24, "four wide lake with reach 4 gets 8 Path cells");
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
	testSmallPocket();
	testLakeAndFar();
	testCrossCheck();
	if (failures) { std::cerr << failures << " failures\n"; return 1; }
	std::cout << "REACHABILITY_PASS\n";
	return 0;
}
