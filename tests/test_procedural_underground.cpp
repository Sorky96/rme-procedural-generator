// SPDX-License-Identifier: GPL-3.0-or-later
// Z entrances, the cave below and the reachability through Z (G5 iteration 27). Hand counted layouts plus a cross-check.
#include "procedural/core/reachability.h"
#include "procedural/core/underground.h"
#include "procedural/core/level_repair.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <format>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <tuple>
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

CaveGrid emptyCave(const SurfaceGrid& surface) {
	CaveGrid cave;
	cave.region = surface.region;
	++cave.region.z;
	cave.cells.assign(surface.cells.size(), CaveKind::Solid);
	return cave;
}

void disc(CaveGrid& cave, int32_t cx, int32_t cy) {
	for (int32_t dy = -2; dy <= 2; ++dy) for (int32_t dx = -2; dx <= 2; ++dx) {
		if (dx * dx + dy * dy <= 5) cave.cells[size_t(cy + dy) * cave.region.width + size_t(cx + dx)] = CaveKind::Floor;
	}
}

// Entrances placed by generateSurface: on Rock, next to open ground, 3 from the edge, 24 apart; off by default.
void testEntrances() {
	size_t checkedSurfaces = 0;
	for (const auto style : {CaveEntranceStyle::RockFace, CaveEntranceStyle::FloorHole}) for (uint64_t seed = 1; seed <= 12; ++seed) {
		SurfaceConfig config {{100, 100, 7, 200, 200}, seed, {}};
		const SurfaceBudget budget {200 * 200, 200 * 200 * 8, 200 * 200 * 96 + (1u << 20)};
		const MapExtent extent {65001, 65001, 16};
		config.tuning.caveEntranceStyle = style;
		const auto off = generateSurface(config, extent, budget);
		auto defaultStyle = config; defaultStyle.tuning.caveEntranceStyle = CaveEntranceStyle::RockFace;
		const auto baseline = generateSurface(defaultStyle, extent, budget);
		check(off && baseline && off->fingerprint() == baseline->fingerprint(), "inactive style preserves fingerprint");
		auto invalid = config; invalid.tuning.caveEntranceStyle = static_cast<CaveEntranceStyle>(2);
		check(!generateSurface(invalid, extent, budget), "invalid style rejected");
		check(off.has_value() && off->entrances.empty(), "entrances: none with caveEntrances 0");
		config.tuning.caveEntrances = 20;
		config.tuning.caveEntranceStyle = style;
		const auto on = generateSurface(config, extent, budget);
		const auto again = generateSurface(config, extent, budget);
		check(on.has_value() && again.has_value() && on->entrances == again->entrances && on->cells == again->cells, "entrances: deterministic");
		if (!on) continue;
		check(on->valid(), "entrances: valid grid");
		if (on->entrances.size() >= 2) ++checkedSurfaces;
		for (size_t a = 0; a < on->entrances.size(); ++a) {
			const uint32_t cell = on->entrances[a];
			const uint32_t x = cell % 200, y = cell / 200;
			check(caveEntranceAllowed(on->cells, 200, 200, cell, style), "entrances: style geometry");
			check(x >= 3 && y >= 3 && x + 3 < 200 && y + 3 < 200, "entrances: 3 cells from the edge");
			const auto open = [&](size_t i) { const auto k = on->cells[i]; return k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Shore || k == SurfaceKind::Path; };
			check(open(cell - 1) || open(cell + 1) || open(cell - 200) || open(cell + 200), "entrances: next to open ground");
			for (size_t b = a + 1; b < on->entrances.size(); ++b) {
				const uint32_t other = on->entrances[b];
				check(std::max(std::abs(int(other % 200) - int(x)), std::abs(int(other / 200) - int(y))) >= 24, "entrances: 24 apart");
			}
		}
	}
	check(checkedSurfaces > 0, "entrances: no surface got two entrances (test setup)");
}

void testEntranceStyles() {
	for (const auto style : {CaveEntranceStyle::RockFace, CaveEntranceStyle::FloorHole}) {
		auto g = grid(11, 11, SurfaceKind::Grass);
		constexpr uint32_t e = 5 * 11 + 5;
		if (style == CaveEntranceStyle::RockFace) for (const auto i : {e, e - 11, e - 1, e + 1}) g.cells[i] = SurfaceKind::Rock;
		check(caveEntranceAllowed(g.cells, 11, 11, e, style), "hand entrance accepted");
		for (const auto k : {SurfaceKind::Rock, SurfaceKind::Lake, SurfaceKind::Floor}) {
			auto bad = g; bad.cells[e + 11] = k;
			check(!caveEntranceAllowed(bad.cells, 11, 11, e, style), "bad south neighbour rejected");
		}
		for (const auto k : {SurfaceKind::Path, SurfaceKind::Floor, SurfaceKind::Lake}) {
			auto bad = g; bad.cells[e] = k;
			check(!caveEntranceAllowed(bad.cells, 11, 11, e, style), "bad entrance cell rejected");
		}
		auto corner = g; corner.cells[e - 1] = SurfaceKind::Grass;
		if (style == CaveEntranceStyle::RockFace) {
            check(!caveEntranceAllowed(corner.cells, 11, 11, e, style), "legacy rock corner rejected");
            check(caveEntranceAllowed(corner.cells, 11, 11, e, style, true), "repair selector accepts two rock sides");
            corner.cells[e + 1] = SurfaceKind::Grass;
            check(!caveEntranceAllowed(corner.cells, 11, 11, e, style, true), "repair selector still requires two rock sides");
            corner = g; corner.cells[e + 11] = SurfaceKind::Rock;
            check(!caveEntranceAllowed(corner.cells, 11, 11, e, style, true), "repair selector keeps open south exit");
        }
		uint64_t state = 1729;
		for (int round = 0; round < 80; ++round) {
			for (auto& k : g.cells) { state = state * 6364136223846793005ULL + 1; k = static_cast<SurfaceKind>((state >> 32) % 9); }
			// Keep a mixture of positive and negative samples, not only random rejections.
			if (round % 2 == 0) {
				fillRect(g, 3, 3, 7, 7, SurfaceKind::Grass);
				if (style == CaveEntranceStyle::RockFace) for (const auto i : {e, e - 11, e - 1, e + 1}) g.cells[i] = SurfaceKind::Rock;
			}
			for (uint32_t y = 0; y < 11; ++y) for (uint32_t x = 0; x < 11; ++x) {
				const auto at = [&](int dx, int dy) { return g.cells[size_t(int(y) + dy) * 11 + size_t(int(x) + dx)]; };
				const auto open = [](SurfaceKind k) { return k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Shore || k == SurfaceKind::Path; };
				bool expected = x >= 3 && y >= 3 && x < 8 && y < 8;
				if (expected) {
					if (style == CaveEntranceStyle::RockFace) expected = at(0,0) == SurfaceKind::Rock && at(-1,0) == SurfaceKind::Rock && at(1,0) == SurfaceKind::Rock && at(0,-1) == SurfaceKind::Rock && open(at(0,1));
					else expected = (at(0,0) == SurfaceKind::Grass || at(0,0) == SurfaceKind::Dirt) && open(at(-1,0)) && open(at(1,0)) && open(at(0,-1)) && open(at(0,1));
					if (style == CaveEntranceStyle::FloorHole) for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) if (at(dx,dy) == SurfaceKind::Lake || at(dx,dy) == SurfaceKind::River || at(dx,dy) == SurfaceKind::Floor) expected = false;
				}
				check(caveEntranceAllowed(g.cells, 11, 11, y * 11 + x, style) == expected, "independent entrance geometry comparison");
			}
		}
	}
}

// Cave of two entrances on an all-rock 40 x 20 surface: entrances (10,10) and (30,10).
void testCave() {
	SurfaceGrid surface = grid(40, 20, SurfaceKind::Rock);
	surface.entrances = {10 * 40 + 10, 10 * 40 + 30};
	const auto cave = generateCave(surface, 7);
	check(cave.has_value() && cave->valid() && cave->region.z == 8, "cave: valid grid one floor lower");
	if (!cave) return;
	bool landings = true;
	for (const uint32_t entrance : surface.entrances) {
		const int32_t cx = int32_t(entrance % 40), cy = int32_t(entrance / 40);
		for (int32_t dy = -2; dy <= 2; ++dy) for (int32_t dx = -2; dx <= 2; ++dx) {
			if (dx * dx + dy * dy <= 5 && cave->cells[size_t(cy + dy) * 40 + size_t(cx + dx)] != CaveKind::Floor) landings = false;
		}
	}
	check(landings, "cave: both landings (21 cells each) are Floor");
	bool border = true;
	for (uint32_t y = 0; y < 20; ++y) for (uint32_t x = 0; x < 40; ++x) {
		if ((x <= 1 || y <= 1 || x >= 38 || y >= 18) && cave->cells[size_t(y) * 40 + x] != CaveKind::Solid) border = false;
	}
	check(border, "cave: the border (1 cell) stays Solid");
	const auto report = undergroundReachabilityOf(surface, *cave, false);
	check(report.has_value() && report->caveComponents == 1 && report->pockets == 0 && report->entrances == 2, "cave: two entrances give one component");
	check(report.has_value() && report->caveCells >= 21 * 2 + 15, "cave: landings and a tunnel between them");
	const auto again = generateCave(surface, 7), other = generateCave(surface, 8);
	check(again.has_value() && again->cells == cave->cells && again->fingerprint() == cave->fingerprint(), "cave: deterministic");
	check(other.has_value() && other->cells != cave->cells, "cave: another seed, another cave");

	SurfaceGrid none = grid(40, 20, SurfaceKind::Rock);
	const auto empty = generateCave(none, 7);
	check(empty.has_value() && std::ranges::all_of(empty->cells, [](CaveKind k) { return k == CaveKind::Solid; }), "cave: no entrance, no cave");

	SurfaceGrid single = grid(80, 60, SurfaceKind::Rock);
	single.entrances = {20 * 80 + 20};
	const auto lone = generateCave(single, 3);
	const auto loneReport = lone ? undergroundReachabilityOf(single, *lone, false) : std::unexpected(UndergroundError::InvalidGrid);
	check(loneReport.has_value() && loneReport->caveComponents == 1 && loneReport->caveCells > 21 + 20, "cave: one entrance gets a landing and a tunnel in one component");
}

// Iteration 47 (cave shape): the joins are a minimum spanning tree plus loops and do not depend on the order of the
// entrances; every entrance and cavern is in one component; no straight piece of an axis is longer than 20 cells;
// caverns make at least 25 % of the floor (median).
void testCaveShape() {
	const auto componentOf = [](const CaveGrid& cave, uint32_t from) {
		const auto w = cave.region.width;
		std::vector<uint8_t> seen(cave.cells.size());
		std::vector<uint32_t> queue {from};
		seen[from] = 1;
		for (size_t head = 0; head < queue.size(); ++head) {
			const auto i = queue[head];
			for (const auto j : {i - w, i - 1, i + 1, i + w})
				if (j < cave.cells.size() && (j / w == i / w || j % w == i % w) && !seen[j] && cave.cells[j] == CaveKind::Floor) { seen[j] = 1; queue.push_back(j); }
		}
		return seen;
	};
	// Kruskal on the squared distances, independent of the generator's Prim.
	const auto mstWeight = [](std::span<const uint32_t> cells, uint32_t w) {
		std::vector<std::tuple<int64_t, size_t, size_t>> pairs;
		for (size_t i = 0; i < cells.size(); ++i) for (size_t j = i + 1; j < cells.size(); ++j) {
			const int64_t dx = int64_t(cells[i] % w) - int64_t(cells[j] % w), dy = int64_t(cells[i] / w) - int64_t(cells[j] / w);
			pairs.push_back({dx * dx + dy * dy, i, j});
		}
		std::ranges::sort(pairs);
		std::vector<size_t> root(cells.size());
		for (size_t i = 0; i < root.size(); ++i) root[i] = i;
		const auto find = [&](size_t i) { while (root[i] != i) i = root[i] = root[root[i]]; return i; };
		double total = 0;
		for (const auto& [d, i, j] : pairs) if (find(i) != find(j)) { root[find(i)] = find(j); total += std::sqrt(double(d)); }
		return total;
	};
	const auto linkLength = [](const std::pair<uint32_t, uint32_t>& link, uint32_t w) {
		const double dx = double(link.first % w) - double(link.second % w), dy = double(link.first / w) - double(link.second / w);
		return std::sqrt(dx * dx + dy * dy);
	};

	// A 4 x 4 lattice of entrances: joining them in row order gives four parallel tunnels across the area.
	SurfaceGrid lattice = grid(200, 200, SurfaceKind::Rock);
	for (uint32_t row = 0; row < 4; ++row) for (uint32_t column = 0; column < 4; ++column)
		lattice.entrances.push_back((30 + row * 45 + column * 3) * 200 + 25 + column * 48 + row * 2);
	CaveLayout layout;
	const auto cave = generateCave(lattice, 47, {}, &layout);
	check(cave.has_value() && cave->valid(), "shape: lattice cave generated");
	if (!cave) return;
	auto shuffled = lattice;
	std::ranges::reverse(shuffled.entrances);
	std::swap(shuffled.entrances[3], shuffled.entrances[11]);
	CaveLayout shuffledLayout;
	const auto again = generateCave(shuffled, 47, {}, &shuffledLayout);
	check(again && again->cells == cave->cells && again->rooms == cave->rooms && shuffledLayout.links == layout.links,
		"shape: the order of the entrances does not change the cave");
	const size_t n = lattice.entrances.size();
	check(layout.loops == 2 && layout.links.size() == n - 1 + layout.loops, "shape: tree plus two loops for 16 entrances");
	double tree = 0;
	for (size_t k = 0; k + 1 < n && k < layout.links.size(); ++k) tree += linkLength(layout.links[k], 200);
	check(std::abs(tree - mstWeight(lattice.entrances, 200)) < 1e-6, "shape: tree edges have the minimum spanning tree's length");
	bool rowChained = false;
	auto sorted = lattice.entrances;
	std::ranges::sort(sorted);
	for (size_t k = 0; k + 1 < sorted.size(); ++k) if (k % 4 == 3)
		rowChained |= std::ranges::count(layout.links, std::pair(sorted[k], sorted[k + 1])) + std::ranges::count(layout.links, std::pair(sorted[k + 1], sorted[k])) > 0;
	check(!rowChained, "shape: no join from the end of one row to the start of the next");
	const auto seen = componentOf(*cave, lattice.entrances.front());
	check(std::ranges::all_of(lattice.entrances, [&](uint32_t e) { return seen[e] != 0; }), "shape: all entrances in one component");
	check(!cave->rooms.empty() && std::ranges::all_of(cave->rooms, [&](uint32_t r) { return seen[r] != 0; }), "shape: every cavern joined to the network");
	check(std::ranges::count(cave->cells, CaveKind::Floor) == std::ranges::count(seen, uint8_t(1)), "shape: no separate floor");
	CaveLayout reference;
	std::stop_source cancel; cancel.request_stop();
	const auto stopped = generateCave(lattice, 47, cancel.get_token(), &reference);
	check(!stopped && stopped.error() == SurfaceError::Cancelled, "shape: cancellation");

	// Generated surfaces: 24 seeds of 256 x 256 with 16 entrances requested.
	std::vector<double> shares;
	uint32_t longest = 0, branches = 0, tunnels = 0, components = 0;
	bool border = true, widths = true;
	for (uint64_t seed = 1; seed <= 24; ++seed) {
		SurfaceConfig config {{100, 100, 7, 256, 256}, seed, {}};
		config.tuning.caveEntrances = 16;
		const auto surface = generateSurface(config, {65001, 65001, 16}, {256 * 256, 256 * 256 * 8, 256 * 256 * 96 + (1u << 20)});
		if (!surface || surface->entrances.size() < 2) continue;
		CaveLayout l;
		const auto c = generateCave(*surface, seed, {}, &l);
		check(c.has_value(), "shape: generated cave");
		if (!c) continue;
		const auto s = componentOf(*c, surface->entrances.front());
		components += std::ranges::all_of(surface->entrances, [&](uint32_t e) { return s[e] != 0; });
		for (const auto& axis : l.axes) longest = std::max(longest, longestStraightAxis(axis, 256));		branches += l.branches; tunnels += uint32_t(l.links.size());
		check(l.branches <= 2 * l.links.size(), "shape: at most two branches per tunnel");
		const auto floor = std::ranges::count(c->cells, CaveKind::Floor);
		shares.push_back(double(caveCavernCells(*c)) / double(std::max<int64_t>(1, floor)));
		for (uint32_t i = 0; i < c->cells.size(); ++i) {
			const auto x = i % 256, y = i / 256;
			if (c->cells[i] == CaveKind::Floor && (x < 2 || y < 2 || x > 253 || y > 253)) border = false;
		}
		// Width along the axis: perpendicular runs through tunnel axes cover 2..5 cells somewhere.
		std::array<bool, 6> found {};
		for (const auto& axis : l.axes) for (size_t k = 2; k + 2 < axis.size(); ++k) {
			const int dx = int(axis[k + 2] % 256) - int(axis[k - 2] % 256), dy = int(axis[k + 2] / 256) - int(axis[k - 2] / 256);
			if (dx != 0 && dy != 0) continue;
			const int px = dy != 0, py = dx != 0;
			int run = 1;
			for (int m = 1; m < 8 && c->cells[(axis[k] / 256 + py * m) * 256 + axis[k] % 256 + px * m] == CaveKind::Floor; ++m) ++run;
			for (int m = 1; m < 8 && c->cells[(axis[k] / 256 - py * m) * 256 + axis[k] % 256 - px * m] == CaveKind::Floor; ++m) ++run;
			if (run >= 2 && run <= 5) found[size_t(run)] = true;
		}
		widths &= found[2] || found[3];
	}
	check(shares.size() >= 20, "shape: enough generated caves");
	std::ranges::sort(shares);
	const double median = shares.empty() ? 0 : shares[shares.size() / 2];
	check(components == shares.size(), "shape: generated entrances in one component");
	check(longest <= 20, std::format("shape: longest straight axis {} <= 20", longest));
	check(median >= 0.25, std::format("shape: cavern share median {:.3f} >= 0.25", median));
	check(branches > 0, "shape: some dead-end branches");
	check(border, "shape: the border stays Solid");
	check(widths, "shape: narrow tunnel parts exist");
	// The straightness measure itself: a 30-cell row is straight; a bend every 10 cells is not.
	std::vector<uint32_t> row, zigzag;
	for (uint32_t x = 10; x < 40; ++x) row.push_back(50 * 100 + x);
	for (uint32_t x = 10; x < 40; ++x) zigzag.push_back((50 + ((x / 10) % 2 ? x % 10 : 10 - x % 10) / 2) * 100 + x);
	check(longestStraightAxis(row, 100) == 30 && longestStraightAxis(zigzag, 100) <= 20, "shape: straightness measure");
	std::cout << std::format("CAVE_SHAPE_PASS seeds {} longestStraight {} cavernShareMedian {:.3f} branches {} tunnels {}\n",
		shares.size(), longest, median, branches, tunnels);
}

// 40 x 20 grass with a lake x 20..22 (a full-height wall of water); Rock entrances A (10,10) on the left and B (30,10) on the right.
//   The start is (19,9) (the middle is (19.5, 9.5): (19,9) and (19,10) tie, the lower index wins), so A is reached and B is not.
//   Two separate discs: 21 + 21 = 42 cave cells, 2 components, A's component reached (21), B's a pocket.
//   A row of 15 cells (x 13..27, y 10) between the discs joins them: 21 + 15 + 21 = 57 cells, 1 component, all reached.
void testValidator() {
	SurfaceGrid surface = grid(40, 20, SurfaceKind::Grass);
	fillRect(surface, 20, 0, 22, 19, SurfaceKind::Lake);
	surface.cells[10 * 40 + 10] = SurfaceKind::Rock;
	surface.cells[10 * 40 + 30] = SurfaceKind::Rock;
	surface.entrances = {10 * 40 + 10, 10 * 40 + 30};
	CaveGrid cave = emptyCave(surface);
	disc(cave, 10, 10);
	disc(cave, 30, 10);
	auto r = undergroundReachabilityOf(surface, cave, false);
	check(r.has_value() && r->entrances == 2 && r->entrancesReached == 1, "validator: only the entrance on the start's side is reached");
	check(r.has_value() && r->caveCells == 42 && r->caveComponents == 2 && r->caveReachedCells == 21 && r->pockets == 1, "validator: two discs, one reached, one pocket");
	for (uint32_t x = 13; x <= 27; ++x) cave.cells[10 * 40 + x] = CaveKind::Floor;
	r = undergroundReachabilityOf(surface, cave, false);
	check(r.has_value() && r->caveCells == 57 && r->caveComponents == 1 && r->caveReachedCells == 57 && r->pockets == 0, "validator: a tunnel joins the discs, everything is reached");
	CaveGrid wrong = emptyCave(surface);
	wrong.region.width = 39;
	check(undergroundReachabilityOf(surface, wrong, false).error_or(UndergroundError::Cancelled) == UndergroundError::InvalidGrid, "validator: mismatched sizes are invalid");
}

// Independent check: flood fill both floors from the start with the same rules, on random small grids.
void testCrossCheck() {
	uint64_t state = 0x2545f4914f6cdd1dULL;
	auto next = [&] { state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; };
	const SurfaceKind kinds[] = {SurfaceKind::Grass, SurfaceKind::Grass, SurfaceKind::Dirt, SurfaceKind::Rock, SurfaceKind::Rock, SurfaceKind::Shore, SurfaceKind::Lake, SurfaceKind::River, SurfaceKind::Path};
	int matched = 0;
	for (int round = 0; round < 80; ++round) {
		const uint32_t w = 10 + uint32_t(next() % 8), h = 10 + uint32_t(next() % 8);
		SurfaceGrid g = grid(w, h, SurfaceKind::Grass);
		for (auto& c : g.cells) c = kinds[next() % std::size(kinds)];
		for (size_t i = 0; i < g.cells.size(); ++i) if (g.cells[i] == SurfaceKind::Rock && next() % 3 == 0) g.entrances.push_back(uint32_t(i));
		CaveGrid cave = emptyCave(g);
		for (auto& c : cave.cells) c = next() % 5 < 2 ? CaveKind::Floor : CaveKind::Solid;
		const size_t n = g.cells.size();
		const auto walkable = [&](size_t i) { const auto k = g.cells[i]; return k != SurfaceKind::Lake && k != SurfaceKind::River && k != SurfaceKind::Preserve; };
		long long bestD = -1; size_t start = n;
		for (size_t i = 0; i < n; ++i) {
			if (!walkable(i)) continue;
			const long long dx = 2LL * (long long)(i % w) - (long long)(w - 1), dy = 2LL * (long long)(i / w) - (long long)(h - 1);
			if (bestD < 0 || dx * dx + dy * dy < bestD) { bestD = dx * dx + dy * dy; start = i; }
		}
		std::vector<char> seen(n, 0);
		if (start != n) {
			std::vector<size_t> queue {start};
			seen[start] = 1;
			for (size_t head = 0; head < queue.size(); ++head) {
				const size_t i = queue[head];
				const size_t x = i % w, y = i / w;
				const size_t around[4] = {x > 0 ? i - 1 : n, x + 1 < w ? i + 1 : n, y > 0 ? i - w : n, y + 1 < h ? i + w : n};
				for (const size_t m : around) if (m != n && walkable(m) && !seen[m]) { seen[m] = 1; queue.push_back(m); }
			}
		}
		std::vector<int> label(n, 0);
		std::vector<uint64_t> sizes {0};
		for (size_t first = 0; first < n; ++first) {
			if (cave.cells[first] != CaveKind::Floor || label[first]) continue;
			const int id = int(sizes.size());
			std::vector<size_t> queue {first};
			label[first] = id;
			for (size_t head = 0; head < queue.size(); ++head) {
				const size_t i = queue[head];
				const size_t x = i % w, y = i / w;
				const size_t around[4] = {x > 0 ? i - 1 : n, x + 1 < w ? i + 1 : n, y > 0 ? i - w : n, y + 1 < h ? i + w : n};
				for (const size_t m : around) if (m != n && cave.cells[m] == CaveKind::Floor && !label[m]) { label[m] = id; queue.push_back(m); }
			}
			sizes.push_back(queue.size());
		}
		UndergroundReport want;
		std::vector<char> reached(sizes.size(), 0);
		for (const uint32_t e : g.entrances) {
			++want.entrances;
			if (start != n && seen[e]) { ++want.entrancesReached; if (label[e]) reached[size_t(label[e])] = 1; }
		}
		want.caveComponents = uint32_t(sizes.size() - 1);
		for (size_t id = 1; id < sizes.size(); ++id) { want.caveCells += sizes[id]; if (reached[id]) want.caveReachedCells += sizes[id]; else ++want.pockets; }
		const auto got = undergroundReachabilityOf(g, cave, false);
		const bool same = got.has_value() && got->entrances == want.entrances && got->entrancesReached == want.entrancesReached && got->caveCells == want.caveCells &&
			got->caveReachedCells == want.caveReachedCells && got->caveComponents == want.caveComponents && got->pockets == want.pockets;
		check(same, "underground cross-check round " + std::to_string(round));
		matched += same;
	}
	check(matched == 80, "all 80 underground comparisons matched");
}

constexpr ExitShape ramps[] = {
	{ExitDirection::North, 0, 1, true}, {ExitDirection::West, 1, 0, true},
	{ExitDirection::South, 0, -1, true}, {ExitDirection::East, -1, 0, true}};

bool samePlan(const CaveExitPlan& a, const CaveExitPlan& b) {
	return a.skipped == b.skipped && a.exits.size() == b.exits.size() &&
		std::ranges::equal(a.exits, b.exits, [](const CaveExit& x, const CaveExit& y) {
			return x.entrance == y.entrance && x.cell == y.cell && x.shape == y.shape;
		});
}

// Independent oracle: scan all cells for P = E, compare against accepted exit
// footprints directly (no occupancy arrays and no shared direction table).
CaveExitPlan bruteExits(const SurfaceGrid& g, const CaveGrid& c, std::span<const ExitShape> shapes, std::span<const uint8_t> occupied) {
	const int w = int(g.region.width), h = int(g.region.height);
	const auto shifted = [&](int p, int dx, int dy) {
		const int x = p % w + dx, y = p / w + dy;
		return x < 0 || y < 0 || x >= w || y >= h ? -1 : y * w + x;
	};
	const auto dest = [&](int p, ExitDirection d, int sign = 1) {
		switch (d) {
			case ExitDirection::North: return shifted(p, 0, -sign);
			case ExitDirection::South: return shifted(p, 0, sign);
			case ExitDirection::East: return shifted(p, sign, 0);
			case ExitDirection::West: return shifted(p, -sign, 0);
		}
		return -1;
	};
	const auto contains = [](const auto& list, int p) { return std::ranges::find(list, uint32_t(p)) != list.end(); };
	const auto surfaceOK = [&](int p) {
		if (p < 0 || contains(g.entrances, p) || contains(g.fires, p) || contains(g.doors, p) ||
			contains(g.gaps, p) || contains(g.sites, p) || (!occupied.empty() && occupied[size_t(p)])) return false;
		switch (g.cells[size_t(p)]) {
			case SurfaceKind::Grass: case SurfaceKind::Dirt: case SurfaceKind::Shore: case SurfaceKind::Path: return true;
			default: return false;
		}
	};
	CaveExitPlan plan;
	for (uint32_t e = 0; e < g.entrances.size(); ++e) {
		bool found = false;
		for (int p = 0; p < w * h && !found; ++p) {
			if (p != int(g.entrances[e])) continue;
			for (size_t si = 0; si < shapes.size(); ++si) {
				const auto& shape = shapes[si];
				const int t = shape.hasTail ? shifted(p, shape.tailDx, shape.tailDy) : p;
				const int d = dest(p, shape.direction), l = dest(p, shape.direction, -1);
				if (!surfaceOK(d) || l < 0 || t < 0 ||
					c.cells[size_t(p)] != CaveKind::Floor || c.cells[size_t(l)] != CaveKind::Floor || c.cells[size_t(t)] != CaveKind::Floor) continue;
				bool conflict = false;
				for (const auto& old : plan.exits) {
					const auto& os = shapes[old.shape];
					const int op = int(old.cell), ot = os.hasTail ? shifted(op, os.tailDx, os.tailDy) : op;
					const int ol = dest(op, os.direction, -1);
					for (const int candidate : {p, l, t}) for (const int reserved : {op, ol, ot}) conflict |= candidate == reserved;
					conflict |= d == dest(op, os.direction);
				}
				if (conflict) continue;
				plan.exits.push_back({e, uint32_t(p), uint8_t(si)});
				found = true;
				break;
			}
		}
		if (!found) plan.skipped.push_back(e);
	}
	return plan;
}

void testExits() {
	auto g = grid(9, 9, SurfaceKind::Grass);
	g.entrances = {40}; g.cells[40] = SurfaceKind::Rock;
	auto c = emptyCave(g); disc(c, 4, 4);
	std::vector<uint8_t> occupied(81, 0);
	const ExitShape north[] = {{ExitDirection::North}};
	std::expected<CaveExitPlan, UndergroundError> plan;
	for (const auto style : {CaveEntranceStyle::RockFace, CaveEntranceStyle::FloorHole}) {
		g.cells.assign(81, SurfaceKind::Grass);
		g.cells[40] = style == CaveEntranceStyle::RockFace ? SurfaceKind::Rock : SurfaceKind::Grass;
		if (style == CaveEntranceStyle::RockFace) for (const auto i : {31, 39, 41}) g.cells[i] = SurfaceKind::Rock;
		const ExitShape shapes[] = {{ExitDirection::North}, {ExitDirection::South, 1, 0, true}};
		plan = placeCaveExits(g, c, shapes);
		const auto expected = style == CaveEntranceStyle::RockFace ? 1 : 0;
		check(plan && plan->exits.size() == 1 && plan->exits[0].cell == 40 && plan->exits[0].shape == expected,
			"exit under entrance, first fitting direction for style");
		const auto again = placeCaveExits(g, c, shapes);
		check(plan && again && samePlan(*plan, *again), "exit deterministic");
		const auto destination = expected == 1 ? 49 : 31;
		const ExitShape south[] = {{ExitDirection::South}};
		const auto selected = expected == 1 ? std::span<const ExitShape>(south) : std::span<const ExitShape>(north);
		const auto skipped = [&] {
			const auto result = placeCaveExits(g, c, selected, occupied);
			check(result && result->exits.empty() && result->skipped == std::vector<uint32_t>{0}, "rejected entrance has no displaced exit");
		};
		for (const auto kind : {SurfaceKind::Rock, SurfaceKind::Lake, SurfaceKind::River, SurfaceKind::Floor, SurfaceKind::Preserve}) {
			g.cells[destination] = kind; skipped();
		}
		g.cells[destination] = SurfaceKind::Grass;
		occupied[destination] = 1; skipped(); occupied[destination] = 0;
		g.cells[destination] = SurfaceKind::Path; g.fires = {uint32_t(destination)}; skipped(); g.fires.clear();
		g.cells[destination] = SurfaceKind::Grass;
		const auto landing = expected == 1 ? 31 : 49;
		c.cells[landing] = CaveKind::Solid; skipped(); c.cells[landing] = CaveKind::Floor;
		c.cells[40] = CaveKind::Solid; skipped(); c.cells[40] = CaveKind::Floor;
		const ExitShape tailOnLanding[] = {{ExitDirection::South, 0, -1, true}};
		const auto onLanding = placeCaveExits(g, c, tailOnLanding);
		check(onLanding && onLanding->exits.size() == 1 && onLanding->skipped.empty(), "tail on the landing accepted");
		const ExitShape tailEast[] = {{ExitDirection::South, 1, 0, true}};
		c.cells[41] = CaveKind::Solid;
		const auto solidTail = placeCaveExits(g, c, tailEast);
		check(solidTail && solidTail->exits.empty(), "non-Floor tail rejected");
		c.cells[41] = CaveKind::Floor;
	}
	// Real ramp geometry: every tail lies on its landing (the ramp's lower half), and the variant fits.
	plan = placeCaveExits(g, c, ramps);
	check(plan && plan->exits.size() == 1 && plan->skipped.empty() && samePlan(*plan, bruteExits(g, c, ramps, {})), "client ramp with tail on the landing is placed");
	g.entrances = {41, 40}; g.cells[41] = g.cells[40] = SurfaceKind::Rock;
	disc(c, 5, 4);
	plan = placeCaveExits(g, c, north);
	check(plan && samePlan(*plan, bruteExits(g, c, north, {})), "close entrances reservations match independent oracle");

	// Shared destinations and lower landing/tail reservations in entrance-list order.
	for (const auto second : {uint32_t(38), uint32_t(42), uint32_t(58)}) {
		auto adjacent = grid(9, 9, SurfaceKind::Grass);
		adjacent.entrances = {40, second};
		adjacent.cells[40] = adjacent.cells[second] = SurfaceKind::Rock;
		auto lower = emptyCave(adjacent); lower.cells.assign(81, CaveKind::Floor);
		const ExitShape shapes[] = {{ExitDirection::East, 0, 1, true}, {ExitDirection::West}, {ExitDirection::North}};
		const auto got = placeCaveExits(adjacent, lower, shapes);
		check(got && samePlan(*got, bruteExits(adjacent, lower, shapes, {})), "reserved P/L/tail/destination collision");
	}
	// Metadata exclusions remain enforced even though their valid cells are Floor.
	for (int field = 0; field < 3; ++field) {
		auto blocked = grid(9, 9, SurfaceKind::Grass);
		blocked.entrances = {40}; blocked.cells[40] = SurfaceKind::Rock; blocked.cells[31] = SurfaceKind::Floor;
		if (field == 0) blocked.doors = {31};
		if (field == 1) blocked.gaps = {31};
		if (field == 2) blocked.sites = {31};
		const auto got = placeCaveExits(blocked, c, north);
		check(got && got->exits.empty() && got->skipped.size() == 1, "surface metadata destination rejected");
	}

	const auto invalid = [&](const SurfaceGrid& surface, const CaveGrid& cave, std::span<const ExitShape> shapes,
		std::span<const uint8_t> mask = {}) {
		const auto result = placeCaveExits(surface, cave, shapes, mask);
		return !result && result.error() == UndergroundError::InvalidGrid;
	};
	check(invalid(g, c, {}), "exit: empty shapes rejected");
	check(invalid(g, c, ramps, std::span(occupied).first(80)), "exit: wrong occupied size rejected");
	for (int field = 0; field < 5; ++field) {
		auto wrong = c;
		if (field == 0) ++wrong.region.x;
		if (field == 1) ++wrong.region.y;
		if (field == 2) ++wrong.region.z;
		if (field == 3) ++wrong.region.width;
		if (field == 4) ++wrong.region.height;
		check(invalid(g, wrong, ramps), "exit: mismatched region rejected");
	}
	auto wrongCave = c; wrongCave.cells[0] = static_cast<CaveKind>(2);
	check(invalid(g, wrongCave, ramps), "exit: invalid cave rejected");
	auto wrongSurface = g; wrongSurface.entrances.push_back(81);
	check(invalid(wrongSurface, c, ramps), "exit: invalid surface rejected");
	const ExitShape badTail[] = {{ExitDirection::North, 1, 1, true}};
	check(invalid(g, c, badTail), "exit: diagonal tail rejected");
	const ExitShape zeroTail[] = {{ExitDirection::North, 0, 0, true}};
	check(invalid(g, c, zeroTail), "exit: zero tail rejected");
	const ExitShape badDirection[] = {{static_cast<ExitDirection>(4)}};
	check(invalid(g, c, badDirection), "exit: invalid direction rejected");
	std::vector<ExitShape> tooMany(257);
	check(invalid(g, c, tooMany), "exit: unrepresentable shape index rejected");
	std::stop_source stopped; stopped.request_stop();
	const auto cancelled = placeCaveExits(g, c, ramps, {}, stopped.get_token());
	check(!cancelled && cancelled.error() == UndergroundError::Cancelled, "exit: cancellation");
	g.entrances.clear();
	plan = placeCaveExits(g, c, ramps);
	check(plan && plan->exits.empty() && plan->skipped.empty(), "exit: no entrances");
}

void testExitCrossCheck() {
	uint64_t state = 0xa8459b3127ULL;
	auto next = [&] { state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; };
	int matched = 0;
	for (const auto style : {CaveEntranceStyle::RockFace, CaveEntranceStyle::FloorHole}) for (int round = 0; round < 80; ++round) {
		auto g = grid(5 + uint32_t(next() % 10), 5 + uint32_t(next() % 10), SurfaceKind::Grass);
		for (auto& kind : g.cells) kind = static_cast<SurfaceKind>(next() % 9);
		for (uint32_t i = 0; i < g.cells.size(); ++i) {
			if (g.cells[i] == (style == CaveEntranceStyle::RockFace ? SurfaceKind::Rock : SurfaceKind::Grass) && next() % 2 == 0) g.entrances.push_back(i);
			if (g.cells[i] == SurfaceKind::Path && next() % 2 == 0) g.fires.push_back(i);
			if (g.cells[i] == SurfaceKind::Floor) {
				switch (next() % 3) { case 0: g.doors.push_back(i); break; case 1: g.gaps.push_back(i); break; default: g.sites.push_back(i); }
			}
		}
		std::ranges::reverse(g.entrances);
		auto c = emptyCave(g);
		for (auto& kind : c.cells) kind = next() % 4 ? CaveKind::Floor : CaveKind::Solid;
		std::vector<uint8_t> occupied(g.cells.size());
		for (auto& byte : occupied) byte = uint8_t(next() % 7 == 0);
		std::vector<ExitShape> shapes;
		for (int i = 0; i < 8; ++i) {
			auto shape = ramps[next() % 4]; shape.hasTail = next() % 2 != 0;
			if (next() % 2) { const auto dx = shape.tailDx; shape.tailDx = shape.tailDy; shape.tailDy = dx; }
			shapes.push_back(shape);
		}
		const std::span<const uint8_t> mask = round % 2 ? std::span<const uint8_t>(occupied) : std::span<const uint8_t>();
		const auto got = placeCaveExits(g, c, shapes, mask);
		const auto want = bruteExits(g, c, shapes, mask);
		const auto repeated = placeCaveExits(g, c, shapes, mask);
		const bool same = got && repeated && samePlan(*got, want) && samePlan(*got, *repeated);
		check(same, std::format("exit cross-check round {}", round));
		matched += same;
	}
	std::cout << std::format("EXIT_CROSS_CHECK {}/160\n", matched);
	check(matched == 160, "all 160 independent exit comparisons matched");
}
} // namespace

// Deliberately exhaustive oracle: no distance transform and no production component labelling.

// Independent exhaustive Chebyshev search and four-neighbour flood fill.
void checkPlateauGeometry(const SurfaceGrid& base, const PlateauGrid& p, uint32_t minimum, std::span<const ExitShape> shapes)
{
    const int w = int(base.region.width), h = int(base.region.height);
    const size_t n = base.cells.size();
    std::vector<uint8_t> candidate(n), seen(n), expected(n);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        int distance = std::min({x + 1, y + 1, w - x, h - y});
        for (int yy = 0; yy < h; ++yy) for (int xx = 0; xx < w; ++xx)
            if (base.cells[size_t(yy) * w + xx] != SurfaceKind::Rock)
                distance = std::min(distance, std::max(std::abs(xx - x), std::abs(yy - y)));
        candidate[size_t(y) * w + x] = base.cells[size_t(y) * w + x] == SurfaceKind::Rock && distance >= int(p.inset);
    }
    for (size_t start = 0; start < n; ++start) if (candidate[start] && !seen[start]) {
        std::vector<size_t> q{start}; seen[start] = 1;
        for (size_t k = 0; k < q.size(); ++k) {
            int x = int(q[k] % w), y = int(q[k] / w);
            for (auto [xx, yy] : {std::pair{x-1,y}, {x+1,y}, {x,y-1}, {x,y+1}}) {
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                auto j = size_t(yy) * w + xx;
                if (candidate[j] && !seen[j]) { seen[j] = 1; q.push_back(j); }
            }
        }
        bool retained = false;
        for (auto i : q) retained |= p.cells[i] == PlateauKind::Top;
        if (retained) {
            check(q.size() >= minimum, "oracle minimum component");
            for (auto i : q) expected[i] = 1;
            check(std::ranges::any_of(p.ramps, [&](const auto& r) { return std::ranges::find(q, r.destination) != q.end(); }), "oracle component ramp");
        }
    }
    for (size_t i = 0; i < n; ++i) check((p.cells[i] == PlateauKind::Top) == bool(expected[i]), "whole oracle component");
    for (const auto& r : p.ramps) {
        const auto shape = shapes[r.shape];
        constexpr int dx[] = {0,0,1,-1}, dy[] = {-1,1,0,0};
        const auto d = size_t(shape.direction);
        check(r.destination == uint32_t(int(r.cell) + dx[d] + w * dy[d]), "ramp direct arrival");
        check(p.cells[r.destination] == PlateauKind::Top && p.cells[r.cell] == PlateauKind::None, "arrival cap head outside cliff");
        check(p.landings[r.cell] && p.landings[r.destination], "opening and arrival reserved");
        auto tail = size_t(int(r.cell) + shape.tailDx + w * shape.tailDy);
        check(p.cells[tail] == PlateauKind::None, "tail outside cliff");
    }
}

void testPlateaus()
{
    constexpr ExitShape ramps[] = {{ExitDirection::North,0,1,true}, {ExitDirection::South,0,-1,true},
        {ExitDirection::East,-1,0,true}, {ExitDirection::West,1,0,true}};
    auto g = grid(24,24,SurfaceKind::Grass);
    fillRect(g,4,4,15,15,SurfaceKind::Rock);
    auto p = generatePlateau(g,1729,64,ramps);
    check(p && p->components == 1 && std::ranges::count(p->cells,PlateauKind::Top) == 144, "whole first cap no inset");
    auto small = generatePlateau(g,1729,145,ramps);
    check(small && !small->components, "small component skipped");
    auto limited = generatePlateau(g,1729,64,ramps,{}, {},15);
    check(limited && !limited->components, "oversized whole component skipped");
    auto noRamp = generatePlateau(g,1729,64,{});
    check(noRamp && !noRamp->components, "inaccessible component removed");
    const ExitShape invalidDirection[] = {{static_cast<ExitDirection>(4)}};
    auto invalidRamp = generatePlateau(g,1729,64,invalidDirection);
    check(!invalidRamp && invalidRamp.error() == SurfaceError::InvalidParameters, "invalid ramp direction retains error path");
    auto noRock = grid(24,24,SurfaceKind::Grass);
    check(bool(generatePlateau(noRock,1729,64,invalidDirection)), "unused invalid direction retains empty result");
    auto off = generatePlateau(g,1729,0,ramps);
    check(off && !off->components, "mountains disabled");
    std::vector<uint8_t> occupied(g.cells.size(),1);
    auto blocked = generatePlateau(g,1729,64,ramps,occupied);
    check(blocked && !blocked->components, "occupied component refused");
    occupied.assign(g.cells.size(), 0);
    occupied[8 * 24 + 8] = 1;
    auto interiorOccupied = generatePlateau(g,1729,64,ramps,occupied);
    check(interiorOccupied && std::ranges::count(interiorOccupied->cells, PlateauKind::Top) == 144,
        "occupied lower interior does not truncate or discard the whole cap");
    std::stop_source stop; stop.request_stop();
    check(!generatePlateau(g,1729,64,ramps,{},stop.get_token()), "cancelled");
    check(!generatePlateau(g,1729,64,ramps,{}, {},101), "invalid percent");
    auto broad = grid(80,80,SurfaceKind::Grass);
    fillRect(broad,4,4,74,74,SurfaceKind::Rock);
    auto first = generatePlateau(broad,1729,64,ramps);
    auto second = generateNextPlateau(*first,1729,ramps);
    check(second && second->components && second->inset == 3, "higher cap minimum inset three");
    auto third = generateNextPlateau(*second,1730,ramps);
    check(third && third->components && third->inset == 3, "third cap minimum inset three");
    for (uint64_t seed = 1; seed <= 60; ++seed) {
        auto base = grid(48,48,SurfaceKind::Grass);
        fillRect(base,4,4,42,42,SurfaceKind::Rock);
        StableRandom random(seed,912);
        for (int hole = 0; hole < 15; ++hole) base.cells[size_t(random.bounded(base.cells.size()))] = SurfaceKind::Grass;
        auto lower = generatePlateau(base,seed,16,ramps);
        auto repeat = generatePlateau(base,seed,16,ramps);
        check(lower && repeat && lower->cells == repeat->cells && lower->ramps == repeat->ramps, "first determinism");
        checkPlateauGeometry(base,*lower,16,ramps);
        for (uint32_t level = 2; level <= 3; ++level) {
            auto upper = generateNextPlateau(*lower,seed ^ (uint64_t(level)<<48),ramps,{},30);
            auto again = generateNextPlateau(*lower,seed ^ (uint64_t(level)<<48),ramps,{},30);
            check(upper && again && upper->cells == again->cells && upper->ramps == again->ramps, "higher determinism");
            SurfaceGrid support; support.region = lower->region; support.cells.assign(base.cells.size(),SurfaceKind::Grass);
            for (size_t i = 0; i < support.cells.size(); ++i) if (lower->cells[i] == PlateauKind::Top) support.cells[i] = SurfaceKind::Rock;
            checkPlateauGeometry(support,*upper,30,ramps);
            const auto count = [](const PlateauGrid& g) { return std::ranges::count(g.cells,PlateauKind::Top); };
            check(count(*upper)*100 <= count(*lower)*30, "higher percent");
            auto decor = generatePlateauDecoration(*lower,seed,{});
            check(bool(decor), "cap decor");
            for (size_t i = 0; i < upper->cells.size(); ++i) {
                if (upper->cells[i] == PlateauKind::Top) check(lower->cells[i] == PlateauKind::Top, "same xy support");
                if (decor->cells[i] != DecorKind::None) check(lower->cells[i] == PlateauKind::Top && !lower->landings[i], "vegetation grass only");
            }
            for (const auto& r : upper->ramps) {
                check(lower->cells[r.cell] == PlateauKind::Top && !lower->landings[r.cell], "terrace head");
                auto tail = size_t(int(r.cell)+ramps[r.shape].tailDx+48*ramps[r.shape].tailDy);
                check(lower->cells[tail] == PlateauKind::Top && !lower->landings[tail], "terrace tail");
            }
            lower = std::move(upper);
        }
    }
    std::cout << "PLATEAU_LEVELS_LIMIT_CROSS_CHECK 60/60\nPLATEAU_CROSS_CHECK 60/60\n";
    PlateauGrid large {{0,0,6,1000,1000},std::vector<PlateauKind>(1000000,PlateauKind::Top),std::vector<uint8_t>(1000000)};
    std::stop_source during;
    std::jthread canceller([&] {std::this_thread::sleep_for(std::chrono::milliseconds(10));during.request_stop();});
    auto cancelled = generateNextPlateau(large,1729,ramps,during.get_token(),1);
    canceller.join();
    check(!cancelled && cancelled.error() == SurfaceError::Cancelled, "cancel while searching a strict higher-cap inset");
    std::cout << "PLATEAU_COOPERATIVE_CANCEL_PASS\n";
}

void testFarExitsAndDoors() {
    constexpr ExitShape shapes[] = {{ExitDirection::North, 0, 1, true}, {ExitDirection::South, 0, -1, true},
        {ExitDirection::East, -1, 0, true}, {ExitDirection::West, 1, 0, true}};
    const auto distances = [](const CaveGrid& c, std::span<const uint32_t> sources) {
        const auto w = c.region.width;
        std::vector<int> d(c.cells.size(), -1);
        std::vector<uint32_t> q;
        for (auto i : sources) { d[i] = 0; q.push_back(i); }
        for (size_t head = 0; head < q.size(); ++head) {
            const auto i = q[head];
            for (auto j : {i - w, i - 1, i + 1, i + w})
                if (j < d.size() && (j / w == i / w || j % w == i % w) && d[j] < 0 && c.cells[j] == CaveKind::Floor) {
                    d[j] = d[i] + 1; q.push_back(j);
                }
        }
        return d;
    };
    for (uint64_t seed = 1; seed <= 120; ++seed) {
        auto s = grid(96, 96, SurfaceKind::Grass);
        s.entrances = {uint32_t(20 * 96 + 20), uint32_t(70 * 96 + 70)};
        auto cave = generateCave(s, seed);
        auto exits = placeCaveExits(s, *cave, shapes);
        auto original = *cave;
        auto originalExits = *exits;
        check(bool(placeFarCaveExits(s, *cave, *exits, shapes, seed)), "distant exits generated");
        auto again = original; auto repeated = originalExits;
        check(bool(placeFarCaveExits(s, again, repeated, shapes, seed)) && again.cells == cave->cells &&
            repeated.exits.size() == exits->exits.size(), "distant determinism");
        check(std::ranges::count_if(exits->exits, [](const auto& e) { return e.distant; }) == 1, "one distant exit per entered component");
        auto d = distances(*cave, s.entrances);
        for (const auto& e : exits->exits) if (e.distant) check(d[e.cell] >= 24, "independent BFS distant distance");
        check(bool(placeCaveDoorCorridors(*cave, *exits, shapes)), "corridors generated");
        check(!cave->corridors.empty(), "connection between rooms gets door corridor");
        check(bool(placeCaveDoorCorridors(again, repeated, shapes)) && cave->corridors == again.corridors && cave->cells == again.cells,
            "door determinism");
        auto after = distances(*cave, s.entrances);
        for (size_t i = 0; i < d.size(); ++i) if (cave->cells[i] == CaveKind::Floor && d[i] >= 0) check(after[i] >= 0, "narrowing preserves connectivity");
        for (const auto& c : cave->corridors) {
            check(c.length >= 3 && c.length <= 8, "corridor length");
            const auto step = c.horizontal ? 1u : 96u, side = c.horizontal ? 96u : 1u;
            check(cave->cells[c.door - step] == CaveKind::Floor && cave->cells[c.door + step] == CaveKind::Floor &&
                cave->cells[c.door - side] == CaveKind::Solid && cave->cells[c.door + side] == CaveKind::Solid,
                "exactly two opposite door neighbours");
        }
        auto plan = levelPlanOf(s, true, false, &*cave, *exits, shapes, {});
        check(plan && plan->edges.size() == originalExits.exits.size() * 2 + 1, "distant exit has only one Z edge");
        auto reach = levelReachabilityOf(*plan, {});
        check(reach && reach->allReached() && reach->edges == reach->walkableEdges, "distant exits and doors reachable");
    }
    // A compact cave cannot satisfy distance 24 without extending its tunnel.
    auto s = grid(96, 96, SurfaceKind::Grass); s.entrances = {40 * 96 + 40};
    auto c = emptyCave(s);
    for (uint32_t y = 38; y <= 42; ++y) for (uint32_t x = 38; x <= 42; ++x) c.cells[y * 96 + x] = CaveKind::Floor;
    auto e = placeCaveExits(s, c, shapes);
    const auto before = std::ranges::count(c.cells, CaveKind::Floor);
    check(bool(placeFarCaveExits(s, c, *e, shapes, 1)), "short cave extension");
    check(e->exits.size() == 2 && std::ranges::count(c.cells, CaveKind::Floor) > before, "extension adds distant ramp and floor");
    auto d = distances(c, s.entrances);
    check(d[e->exits.back().cell] >= 24, "extended ramp BFS distance");
    std::vector<uint8_t> occupied(c.cells.size(), 1);
    auto blocked = placeCaveExits(s, c, shapes);
    check(bool(placeFarCaveExits(s, c, *blocked, shapes, 1, occupied)) && blocked->exits.size() == 1, "caps forbid all distant exits");
    std::stop_source cancel; cancel.request_stop();
    auto stopped = placeFarCaveExits(s, c, *e, shapes, 1, {}, cancel.get_token());
    check(!stopped && stopped.error() == UndergroundError::Cancelled, "distant exits cancellation");
    auto separateSurface = grid(96,96,SurfaceKind::Grass);
    separateSurface.entrances = {20*96+20,70*96+20};
    auto separate = emptyCave(separateSurface);
    for (uint32_t y : {20u,45u,70u}) for (uint32_t yy=y-1; yy<=y+1; ++yy)
        for (uint32_t x=18; x<=75; ++x) separate.cells[yy*96+x] = CaveKind::Floor;
    auto separateExits = placeCaveExits(separateSurface,separate,shapes);
    check(bool(placeFarCaveExits(separateSurface,separate,*separateExits,shapes,1)), "separate components placement");
    check(std::ranges::count_if(separateExits->exits,[](const auto& e){return e.distant;}) == 2,
        "one distant exit per entered component; none in unentered component");
    check(std::ranges::all_of(separateSurface.cells,[](auto k){return k==SurfaceKind::Grass;}), "no surface hole created by distant ramps");
    auto compact = grid(32,32,SurfaceKind::Grass);
    compact.entrances = {3*32+3,3*32+28,28*32+28};
    auto compactCave = generateCave(compact, 1);
    auto compactExits = placeCaveExits(compact,*compactCave,shapes);
    check(compactCave->rooms.size() >= 2 && bool(placeCaveDoorCorridors(*compactCave,*compactExits,shapes)) &&
        !compactCave->corridors.empty(), "unchanged approach may meet room or transition reservation");
    auto impossible = grid(6,6,SurfaceKind::Grass);
    auto impossibleCave = emptyCave(impossible);
    impossibleCave.cells.assign(36,CaveKind::Floor); impossibleCave.rooms = {14,21};
    auto refusal = placeCaveDoorCorridors(impossibleCave,{},{});
    check(!refusal && refusal.error() == UndergroundError::InvalidGrid, "never accept two rooms silently without a corridor");
    auto mergingSurface = grid(96,96,SurfaceKind::Grass);
    mergingSurface.entrances = {40*96+40,26*96+38};
    auto merging = emptyCave(mergingSurface);
    for (auto entry : mergingSurface.entrances) {
        const auto x=entry%96, y=entry/96;
        for (auto yy=y-2; yy<=y+2; ++yy) for (auto xx=x-2; xx<=x+2; ++xx) merging.cells[yy*96+xx]=CaveKind::Floor;
    }
    auto mergingExits = placeCaveExits(mergingSurface,merging,shapes);
    check(bool(placeFarCaveExits(mergingSurface,merging,*mergingExits,shapes,1)), "extensions beside another entered component");
    check(std::ranges::any_of(mergingExits->exits,[](const auto& exit){return exit.distant;}), "nearby components still get a distant exit");
    const auto mergingDistances = distances(merging,mergingSurface.entrances);
    for (const auto& exit : mergingExits->exits) if (exit.distant)
        check(mergingDistances[exit.cell]>=24, "extension must respect entrances in newly joined components");
    std::cout << "CAVE_FAR_EXIT_DOOR_CROSS_CHECK 120/120\n";
}

void testMountainPasses() {
    auto surface=grid(80,80,SurfaceKind::Grass);
    for (uint32_t y=8;y<72;++y) for (uint32_t x=30;x<50;++x) surface.cells[y*80+x]=SurfaceKind::Rock;
    constexpr ExitShape shapes[]={{ExitDirection::North,0,1,true},{ExitDirection::South,0,-1,true},
        {ExitDirection::East,-1,0,true},{ExitDirection::West,1,0,true}};
    SurfaceTuning tuning; tuning.plateauMinTop=64; tuning.plateauLevels=3;
    auto old=buildLevelWorld(surface,{},1729,tuning,{}, {},shapes,false);
    check(bool(old),"mountain pass baseline");
    if (!old) return;
    const auto oldRamps=old->caps.front().ramps;
    auto disabled=*old;
    check(bool(placeMountainPasses(disabled,1729,tuning,shapes)) && disabled.surface.fingerprint()==old->surface.fingerprint()
        && disabled.caps.front().cells==old->caps.front().cells && disabled.caps.front().ramps==oldRamps,"passes=0 untouched");
    tuning.passes=4;
    auto result=buildLevelWorld(surface,{},1729,tuning,{}, {},shapes,false);
    auto again=buildLevelWorld(surface,{},1729,tuning,{}, {},shapes,false);
    check(result && again && !result->passes.empty(),"manual massif gets a pass");
    if (!result || !again || result->passes.empty()) return;
    check(result->caps.front().components >= 2, "split cap retains both large sides");
    check(result->surface.fingerprint()==again->surface.fingerprint() && result->passes.size()==again->passes.size(),"mountain pass determinism");
    std::vector<uint8_t> band(surface.cells.size());
    for (const auto& pass:result->passes) for (auto i:pass.cells) {
        band[i]=1;
        check(result->surface.cells[i]==SurfaceKind::Path,"pass walkable ground");
        for (const auto& cap:result->caps) check(cap.cells[i]==PlateauKind::None,"pass has no cap");
    }
    for (const auto& cap:result->caps) {
        for (const auto& r:cap.ramps) check(!band[r.cell] && !band[r.destination],"reselected ramp outside pass");
        std::vector<uint8_t> seen(band.size());
        for (uint32_t i=0;i<band.size();++i) if (!seen[i] && cap.cells[i]==PlateauKind::Top) {
            std::vector<uint32_t> q{i};seen[i]=1;
            for (size_t j=0;j<q.size();++j) {
                const auto p=q[j];
                for (auto k:{p%80?p-1:6400u,p%80<79?p+1:6400u,p>=80?p-80:6400u,p+80<6400?p+80:6400u})
                    if (k<6400 && !seen[k] && cap.cells[k]==PlateauKind::Top) {seen[k]=1;q.push_back(k);}
            }
            check(q.size()>=(cap.region.z==6?64u:30u),"split cap minimum");
        }
    }
    // Force an old ramp into the selected valley, then run the same selector.
    auto onRamp=*old; const auto& p=result->passes.front();
    onRamp.caps.front().ramps={{p.cells[p.cells.size()/2],p.cells[p.cells.size()/2]+1,0,0}};
    check(bool(placeMountainPasses(onRamp,1729,tuning,shapes)),"ramp on pass reselected");
    for (const auto& cap:onRamp.caps) for (const auto& r:cap.ramps)
        check(onRamp.surface.cells[r.cell]!=SurfaceKind::Path,"old ramp on pass removed");
    tuning.passes=9;
    check(!placeMountainPasses(onRamp,1729,tuning,shapes),"invalid pass count");
    tuning.passes=4;
    std::stop_source stop; stop.request_stop();
    auto cancelled=placeMountainPasses(onRamp,1729,tuning,shapes,stop.get_token());
    check(!cancelled && cancelled.error()==ReachabilityError::Cancelled,"mountain pass cancellation");
    std::cout << "MOUNTAIN_PASS_CORE_PASS count=" << result->passes.size() << '\n';
}

// Independent check of one storey: footprint, stairs rules and a 3D walk door -> stairs -> every upper cell.
bool storeyValid(const LevelWorld& world, const BuildingStorey& s, std::span<const ExitShape> shapes) {
    const auto& g = world.surface;
    const auto w = g.region.width;
    const auto at = [&](uint32_t x, uint32_t y) { return y * w + x; };
    const auto inside = [&](int64_t x, int64_t y) { return x > s.x0 && x < s.x1 && y > s.y0 && y < s.y1; };
    uint32_t doors = 0, door = 0;
    for (uint32_t y = s.y0; y <= s.y1; ++y) for (uint32_t x = s.x0; x <= s.x1; ++x) {
        if (g.cells[at(x, y)] != SurfaceKind::Floor) return false;
        if (std::ranges::find(g.doors, at(x, y)) != g.doors.end()) { ++doors; door = at(x, y); }
    }
    if (doors != 1 || (s.x1 - s.x0 - 1) * (s.y1 - s.y0 - 1) < 16 || s.shape >= shapes.size()) return false;
    const auto& shape = shapes[s.shape];
    constexpr int sx[] = {0, 0, 1, -1}, sy[] = {-1, 1, 0, 0};
    const int64_t x = s.stairs % w, y = s.stairs / w, d = int64_t(shape.direction);
    const int64_t tx = x + shape.tailDx, ty = y + shape.tailDy, ex = tx - sx[d], ey = ty - sy[d];
    if (s.arrival != at(uint32_t(x + sx[d]), uint32_t(y + sy[d])) || !inside(x + sx[d], y + sy[d]) || !inside(tx, ty) || !inside(ex, ey)) return false;
    if (sx[d] == 0 ? x != s.x0 + 1 && x != s.x1 - 1 : y != s.y0 + 1 && y != s.y1 - 1) return false;
    // Ground floor without the stairs and tail: one component reached from the door.
    std::vector<uint8_t> seen(g.cells.size());
    const auto blockedGround = [&](uint32_t i) { return i == s.stairs || i == at(uint32_t(tx), uint32_t(ty)); };
    std::vector<uint32_t> q{door}; seen[door] = 1;
    for (size_t h = 0; h < q.size(); ++h) for (const auto j : {q[h] - 1, q[h] + 1, q[h] - w, q[h] + w})
        if (!seen[j] && inside(j % w, j / w) && !blockedGround(j)) { seen[j] = 1; q.push_back(j); }
    bool stairsReached = false;
    for (const auto j : {s.stairs - 1, s.stairs + 1, s.stairs - w, s.stairs + w}) stairsReached |= seen[j] && inside(j % w, j / w);
    // q holds the door (outline) and every interior cell except the stairs and its tail.
    if (q.size() - 1 + (shape.hasTail ? 2 : 1) != (s.x1 - s.x0 - 1) * (s.y1 - s.y0 - 1) || !stairsReached) return false;
    std::vector<uint8_t> upper(g.cells.size());
    q.assign(1, s.arrival); upper[s.arrival] = 1;
    for (size_t h = 0; h < q.size(); ++h) for (const auto j : {q[h] - 1, q[h] + 1, q[h] - w, q[h] + w})
        if (!upper[j] && inside(j % w, j / w)) { upper[j] = 1; q.push_back(j); }
    return q.size() == (s.x1 - s.x0 - 1) * (s.y1 - s.y0 - 1);
}

void testBuildingStoreys() {
    static constexpr ExitShape shapes[]={{ExitDirection::North,0,1,true},{ExitDirection::West,1,0,true},
        {ExitDirection::South,0,-1,true},{ExitDirection::East,-1,0,true}};
    auto surface = grid(48, 40, SurfaceKind::Grass);
    fillRect(surface, 5, 5, 13, 11, SurfaceKind::Floor);   // interior 7 x 5: qualifies
    fillRect(surface, 20, 5, 26, 9, SurfaceKind::Floor);   // interior 5 x 3 = 15: too small
    fillRect(surface, 5, 20, 12, 25, SurfaceKind::Floor);  // interior 6 x 4 = 24: qualifies
    surface.doors = {11 * 48 + 9, 9 * 48 + 23, 20 * 48 + 8};
    SurfaceTuning tuning; tuning.buildingStoreys = true; tuning.upperFloorPercent = 100;
    const auto make = [&](SurfaceTuning t, const SurfaceGrid& s) { return buildLevelWorld(s, {}, 1729, t, {}, {}, shapes, false, {}, shapes); };
    auto off = make(SurfaceTuning{}, surface);
    check(off && off->storeys.empty() && off->decor.size() == 1, "storeys off: nothing placed");
    auto world = make(tuning, surface), again = make(tuning, surface);
    check(world && again && world->storeys.size() == 2 && world->storeyReport.qualifying == 2 &&
        world->storeys == again->storeys && world->decor.size() == 2, "two qualifying buildings get storeys, deterministically");
    if (!world || world->storeys.size() != 2) return;
    for (const auto& s : world->storeys) check(storeyValid(*world, s, shapes), "hand storey rules");
    auto plan = levelPlanOf(world->surface, true, false, nullptr, {}, {}, {}, world->decor, {}, world->storeys);
    check(plan && plan->floors.size() == 2 && plan->floors[1].z == 6 && plan->edges.size() == 2, "storey floor and stairs edges");
    if (!plan) return;
    auto reach = levelReachabilityOf(*plan, {});
    check(reach && reach->allReached() && reach->categories[size_t(LevelPoiKind::Storey)].total == 35 + 24 &&
        reach->walkableEdges == 2, "every storey cell reached through door and stairs");
    // Outline on z - 1 is a wall: not walkable.
    check(!plan->floors[1].walkable[5 * 48 + 5] && plan->floors[1].walkable[6 * 48 + 6], "storey walls closed, interior open");
    tuning.upperFloorPercent = 0;
    auto none = make(tuning, surface);
    check(none && none->storeys.empty() && none->storeyReport.chosen == 0 && none->decor.size() == 1, "percent 0 places none");
    // A cap within one cell of the footprint on z - 1 keeps the building single-floor.
    tuning.upperFloorPercent = 100;
    auto capped = *off;
    PlateauGrid cap; cap.region = {0, 0, 6, 48, 40}; cap.cells.assign(48 * 40, PlateauKind::None); cap.landings.assign(48 * 40, 0);
    for (uint32_t y = 2; y <= 12; ++y) for (uint32_t x = 15; x <= 18; ++x) cap.cells[y * 48 + x] = PlateauKind::Top;
    for (uint32_t y = 4; y <= 6; ++y) cap.cells[y * 48 + 14] = PlateauKind::Top;
    cap.components = 1;
    capped.caps.push_back(cap); capped.decor.push_back({});
    check(bool(placeBuildingStoreys(capped, 1729, tuning, shapes)) && capped.storeyReport.collisions == 1 &&
        capped.storeys.size() == 1 && capped.decor.size() == 2, "cap beside a building blocks its storey");
    auto mixed = levelPlanOf(capped.surface, true, false, nullptr, {}, {}, capped.caps, capped.decor, {}, capped.storeys);
    check(mixed && mixed->floors.size() == 2 && std::ranges::count_if(mixed->pois, [](const auto& p) { return p.kind == LevelPoiKind::Storey; }) == 24 &&
        std::ranges::count_if(mixed->pois, [](const auto& p) { return p.kind == LevelPoiKind::Cap; }) == 4 * 11 + 3, "storey shares the first cap floor");
    // Repair: an unreachable storey (arrival permanently blocked) is removed, the building stays.
    auto broken = *world;
    std::vector<uint8_t> mask(48 * 40 * 2);
    for (const auto& s : broken.storeys) for (uint32_t y = s.y0 + 1; y < s.y1; ++y) for (uint32_t x = s.x0 + 1; x < s.x1; ++x)
        if (s.x0 == 5 && s.y0 == 5) mask[48 * 40 + y * 48 + x] = 2;
    auto repaired = repairLevelReach(broken, true, false, {}, {}, 0, false, mask);
    check(repaired && repaired->removedStoreys == 1 && repaired->removedPois == 0 && !repaired->unrepaired &&
        broken.storeys.size() == 1 && broken.decor.size() == 2, "repair drops the unreachable storey only");
    // A ruin gap or a missing door disqualifies; invalid parameters and cancellation.
    auto bad = *off; bad.surface.doors.clear();
    check(bool(placeBuildingStoreys(bad, 1729, tuning, shapes)) && bad.storeys.empty() && bad.storeyReport.qualifying == 0, "doorless buildings never qualify");
    tuning.upperFloorPercent = 101;
    check(!placeBuildingStoreys(bad, 1729, tuning, shapes), "invalid percentage");
    tuning.upperFloorPercent = 30;
    std::stop_source stop; stop.request_stop();
    auto cancelled = placeBuildingStoreys(bad, 1729, tuning, shapes, stop.get_token());
    check(!cancelled && cancelled.error() == ReachabilityError::Cancelled, "storey cancellation");
    auto invalid = levelPlanOf(world->surface, true, false, nullptr, {}, {}, {}, world->decor, {},
        std::vector<BuildingStorey>{{5, 5, 13, 11, 6 * 48 + 6, 8 * 48 + 6, 0}});
    check(!invalid, "stairs and arrival must be neighbours");

    // Generated settlements: independent rules, 3D reachability and determinism on 60 seeds.
    uint64_t placed = 0, qualifying = 0, checkedSeeds = 0;
    for (uint64_t seed = 4300; seed < 4360; ++seed) {
        SurfaceConfig config{{100, 100, 7, 96, 96}, seed, {}};
        config.tuning.buildingDensity = 60; config.tuning.pathLength = 40; config.tuning.ruinPercent = 20;
        config.tuning.buildingStoreys = true; config.tuning.upperFloorPercent = 50;
        if (seed % 3 == 0) { config.tuning.plateauMinTop = 32; config.tuning.plateauLevels = 2; config.tuning.rockPermille = 300; }
        auto generated = generateSurface(config, {65001, 65001, 16}, {9216, 9216 * 8, 9216 * 96 + (1u << 20)});
        if (!generated) { check(false, "storey surface"); continue; }
        auto a = buildLevelWorld(*generated, {}, seed, config.tuning, {}, {}, shapes, false, {}, shapes);
        auto b = buildLevelWorld(*generated, {}, seed, config.tuning, {}, {}, shapes, false, {}, shapes);
        check(a && b && a->storeys == b->storeys, "generated storeys deterministic");
        if (!a) continue;
        ++checkedSeeds;
        qualifying += a->storeyReport.qualifying; placed += a->storeys.size();
        for (const auto& s : a->storeys) {
            check(storeyValid(*a, s, shapes), std::format("generated storey rules seed {}", seed));
            if (!a->caps.empty()) for (uint32_t y = s.y0 - 1; y <= s.y1 + 1; ++y) for (uint32_t x = s.x0 - 1; x <= s.x1 + 1; ++x)
                check(a->caps.front().cells[y * 96 + x] == PlateauKind::None && !a->caps.front().landings[y * 96 + x], "storey clear of caps");
        }
        auto p = levelPlanOf(a->surface, true, false, nullptr, {}, {}, a->caps, a->decor, {}, a->storeys);
        auto r = p ? levelReachabilityOf(*p, {}) : std::unexpected(ReachabilityError::InvalidGrid);
        check(bool(r), "generated storey plan");
        if (!r) continue;
        // Storey cells are reached exactly when the building's door is.
        for (const auto& s : a->storeys) {
            const auto doorIt = std::ranges::find_if(a->surface.doors, [&](uint32_t d) {
                return d % 96 >= s.x0 && d % 96 <= s.x1 && d / 96 >= s.y0 && d / 96 <= s.y1; });
            const bool doorReached = doorIt != a->surface.doors.end() && r->reached[*doorIt];
            // Without a cave the storeys share floor 1 with the first cap, or have it to themselves.
            for (uint32_t y = s.y0 + 1; y < s.y1; ++y) for (uint32_t x = s.x0 + 1; x < s.x1; ++x)
                check(bool(r->reached[9216 + y * 96 + x]) == doorReached, "storey reached with its door");
        }
    }
    check(checkedSeeds == 60 && placed > 0 && placed < qualifying, "generated storeys exist and are a share");
    std::cout << std::format("BUILDING_STOREY_CORE_PASS seeds={} qualifying={} placed={}\n", checkedSeeds, qualifying, placed);
}

// Independent check of one dungeon (iteration 45): sizes, corridors and doors, stairs and landing in room 0, one
// walkable component, the gap to cave Floor and the ground floor of the building without the opening (and storey stairs).
bool dungeonValid(const LevelWorld& world, const BuildingDungeon& d, std::span<const ExitShape> shapes,
    std::span<const ExitShape> storeyShapes, std::string& why) {
    const auto& g = world.surface;
    const auto w = g.region.width, h = g.region.height;
    const auto n = size_t(w) * h;
    constexpr int sx[] = {0, 0, 1, -1}, sy[] = {-1, 1, 0, 0};
    const auto fail = [&](std::string reason) { why = std::move(reason); return false; };
    if (d.rooms.size() < 2 || d.rooms.size() > 6 || d.corridors.size() + 1 != d.rooms.size()) return fail("room count");
    std::vector<uint8_t> walk(n);
    const auto paint = [&](const DungeonRect& r) {
        for (uint32_t y = r.y0; y <= r.y1; ++y) for (uint32_t x = r.x0; x <= r.x1; ++x) walk[size_t(y) * w + x] = 1;
    };
    for (const auto& r : d.rooms) {
        if (r.x1 - r.x0 + 1 < 4 || r.x1 - r.x0 + 1 > 9 || r.y1 - r.y0 + 1 < 4 || r.y1 - r.y0 + 1 > 9) return fail("room size");
        paint(r);
    }
    for (const auto& c : d.corridors) {
        const auto cw = c.cells.x1 - c.cells.x0 + 1, ch = c.cells.y1 - c.cells.y0 + 1;
        const bool horizontal = c.doorA / w == c.doorB / w;
        const auto along = horizontal ? cw : ch, across = horizontal ? ch : cw;
        if (along < 2 || along > 6 || across < 1 || across > 2) return fail("corridor size");
        paint(c.cells);
        walk[c.doorA] = walk[c.doorB] = 1;
    }
    if (size_t(std::ranges::count(walk, uint8_t(1))) != [&] {
        size_t total = 2 * d.corridors.size();
        for (const auto& r : d.rooms) total += size_t(r.x1 - r.x0 + 1) * (r.y1 - r.y0 + 1);
        for (const auto& c : d.corridors) total += size_t(c.cells.x1 - c.cells.x0 + 1) * (c.cells.y1 - c.cells.y0 + 1);
        return total; }()) return fail("overlapping parts");
    for (const auto& c : d.corridors) for (const auto door : {c.doorA, c.doorB}) {
        const bool west = walk[door - 1], east = walk[door + 1], north = walk[door - w], south = walk[door + w];
        if (!((west && east && !north && !south) || (north && south && !west && !east))) return fail("door neighbours");
    }
    // Gap to cave Floor: two empty cells between the wall rings.
    for (size_t i = 0; i < n; ++i) if (walk[i] && world.cave)
        for (int dy = -4; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx) {
            const int64_t x = int64_t(i % w) + dx, y = int64_t(i / w) + dy;
            if (x >= 0 && y >= 0 && x < w && y < h && world.cave->cells[size_t(y) * w + size_t(x)] == CaveKind::Floor) return fail("cave gap");
        }
    for (size_t i = 0; i < n; ++i) if (walk[i] && (i % w < 2 || i / w < 2 || i % w + 2 >= w || i / w + 2 >= h)) return fail("edge");
    std::vector<uint32_t> q{d.descent};
    std::vector<uint8_t> seen(n);
    seen[d.descent] = 1;
    for (size_t head = 0; head < q.size(); ++head) for (const auto j : {q[head] - 1, q[head] + 1, q[head] - w, q[head] + w})
        if (walk[j] && !seen[j]) { seen[j] = 1; q.push_back(j); }
    if (q.size() != size_t(std::ranges::count(walk, uint8_t(1)))) return fail("dungeon split");
    // Stairs: opening against the wall, arrival inside; stairs, tail, landing and the cell beyond the tail in room 0.
    const auto& b = d.building;
    const auto inside = [&](int64_t x, int64_t y) { return x > b.x0 && x < b.x1 && y > b.y0 && y < b.y1; };
    const auto& r0 = d.rooms.front();
    const auto inRoom0 = [&](int64_t x, int64_t y) { return x >= r0.x0 && x <= r0.x1 && y >= r0.y0 && y <= r0.y1; };
    if (d.shape >= shapes.size() || shapes[d.shape].direction != ExitDirection::North) return fail("shape (north only)");
    const auto& shape = shapes[d.shape];
    const auto dir = size_t(shape.direction);
    const int64_t x = d.descent % w, y = d.descent / w, tx = x + shape.tailDx, ty = y + shape.tailDy;
    if (!inside(x, y) || inside(x - sx[dir], y - sy[dir]) || !inside(x + sx[dir], y + sy[dir]) ||
        int64_t(d.arrival) != (y + sy[dir]) * w + x + sx[dir] || g.cells[d.descent] != SurfaceKind::Floor) return fail("surface stairs");
    if (!inRoom0(x, y) || !inRoom0(tx, ty) || !inRoom0(x - sx[dir], y - sy[dir]) || !inRoom0(tx - sx[dir], ty - sy[dir])) return fail("room 0 stairs");
    // Ground floor: the door reaches every interior cell except the opening and storey stairs and tail.
    std::vector<uint8_t> blocked(n);
    blocked[d.descent] = 1;
    for (const auto& s : world.storeys) if (s.x0 == b.x0 && s.y0 == b.y0) {
        const auto& ss = storeyShapes[s.shape];
        const auto storeyTail = uint32_t(int64_t(s.stairs) + ss.tailDx + int64_t(ss.tailDy) * w);
        const auto sd = size_t(ss.direction);
        const auto entry = uint32_t(int64_t(storeyTail) - sx[sd] - int64_t(sy[sd]) * w);
        for (const auto c : {s.stairs, storeyTail, entry}) if (c == d.descent || c == d.arrival) return fail("storey stairs collision");
        blocked[s.stairs] = blocked[storeyTail] = 1;
    }
    const auto door = std::ranges::find_if(g.doors, [&](uint32_t c) { return c % w >= b.x0 && c % w <= b.x1 && c / w >= b.y0 && c / w <= b.y1; });
    if (door == g.doors.end()) return fail("building door");
    std::ranges::fill(seen, uint8_t(0));
    q.assign(1, *door); seen[*door] = 1;
    size_t interior = 0, open = 0;
    for (int64_t yy = b.y0 + 1; yy < b.y1; ++yy) for (int64_t xx = b.x0 + 1; xx < b.x1; ++xx) { ++interior; open += !blocked[size_t(yy) * w + size_t(xx)]; }
    for (size_t head = 0; head < q.size(); ++head) for (const auto j : {q[head] - 1, q[head] + 1, q[head] - w, q[head] + w})
        if (!seen[j] && inside(j % w, j / w) && !blocked[j]) { seen[j] = 1; q.push_back(j); }
    if (q.size() - 1 != open || interior < 16) return fail("ground floor split");
    return true;
}

void testBuildingDungeons() {
    static constexpr ExitShape shapes[]={{ExitDirection::North,0,1,true},{ExitDirection::West,1,0,true},
        {ExitDirection::South,0,-1,true},{ExitDirection::East,-1,0,true}};
    constexpr uint32_t w = 72, h = 64, n = w * h;
    auto surface = grid(w, h, SurfaceKind::Grass);
    fillRect(surface, 10, 10, 18, 16, SurfaceKind::Floor);   // interior 7 x 5
    fillRect(surface, 44, 44, 51, 49, SurfaceKind::Floor);   // interior 6 x 4
    fillRect(surface, 44, 10, 48, 13, SurfaceKind::Floor);   // interior 3 x 2: too small
    surface.doors = {16 * w + 14, 44 * w + 47, 13 * w + 46};
    LevelWorld base;
    base.surface = surface;
    base.cave = emptyCave(surface);
    base.decor = {DecorGrid{surface.region, std::vector<DecorKind>(n)}, DecorGrid{}};
    SurfaceTuning tuning; tuning.buildingDungeons = true; tuning.dungeonPercent = 100;
    auto off = base;
    check(bool(placeBuildingDungeons(off, 1729, SurfaceTuning{}, shapes)) && off.dungeons.empty(), "dungeons off: nothing placed");
    auto world = base, again = base;
    check(bool(placeBuildingDungeons(world, 1729, tuning, shapes)) && bool(placeBuildingDungeons(again, 1729, tuning, shapes)) &&
        world.dungeons.size() == 2 && world.dungeonReport.qualifying == 2 && world.dungeons == again.dungeons, "two qualifying buildings get dungeons, deterministically");
    std::string why;
    for (const auto& d : world.dungeons) check(dungeonValid(world, d, shapes, shapes, why), "hand dungeon rules: " + why);
    auto plan = levelPlanOf(world.surface, true, false, &*world.cave, world.exits, {}, {}, world.decor, {}, {}, world.dungeons);
    check(plan && plan->floors.size() == 2 && plan->floors[1].z == 8 && plan->edges.size() == 4, "dungeon floor and stairs edges");
    if (!plan) return;
    auto reach = levelReachabilityOf(*plan, {});
    uint64_t cells = 0;
    for (const auto& d : world.dungeons) {
        for (const auto& r : d.rooms) cells += uint64_t(r.x1 - r.x0 + 1) * (r.y1 - r.y0 + 1);
        for (const auto& c : d.corridors) cells += uint64_t(c.cells.x1 - c.cells.x0 + 1) * (c.cells.y1 - c.cells.y0 + 1);
    }
    check(reach && reach->allReached() && reach->categories[size_t(LevelPoiKind::Dungeon)].total == cells &&
        reach->walkableEdges == 4, "every dungeon room reached through door, stairs and corridors");
    tuning.dungeonPercent = 0;
    auto none = base;
    check(bool(placeBuildingDungeons(none, 1729, tuning, shapes)) && none.dungeons.empty() && none.dungeonReport.chosen == 0, "percent 0 places none");
    tuning.dungeonPercent = 100;
    // A cave beside the second building leaves no room for its dungeon; the first keeps its own.
    auto caved = base;
    for (uint32_t y = 40; y <= 58; ++y) for (uint32_t x = 38; x <= 60; ++x) caved.cave->cells[y * w + x] = CaveKind::Floor;
    check(bool(placeBuildingDungeons(caved, 1729, tuning, shapes)) && caved.dungeons.size() == 1 && caved.dungeonReport.noSpace == 1 &&
        caved.dungeons.front().building == DungeonRect{10, 10, 18, 16}, "cave below a building blocks its dungeon");
    for (const auto& d : caved.dungeons) check(dungeonValid(caved, d, shapes, shapes, why), "dungeon keeps the cave gap: " + why);
    // Storey stairs of the same building are avoided and the ground floor stays connected.
    auto storeyed = base;
    SurfaceTuning both = tuning; both.buildingStoreys = true; both.upperFloorPercent = 100;
    check(bool(placeBuildingStoreys(storeyed, 1729, both, shapes)) && storeyed.storeys.size() == 2, "storeys before dungeons");
    check(bool(placeBuildingDungeons(storeyed, 1729, both, shapes, shapes)) && storeyed.dungeons.size() == 2, "dungeons beside storeys");
    for (const auto& d : storeyed.dungeons) check(dungeonValid(storeyed, d, shapes, shapes, why), "dungeon beside storey stairs: " + why);
    // Repair: a dungeon whose rooms are permanently blocked is removed, the building stays.
    auto blocked = world;
    std::vector<uint8_t> mask(size_t(n) * 2);
    const auto& first = blocked.dungeons.front();
    std::vector<uint8_t> walk(n);
    check(markDungeonCells(first, w, h, walk), "mark dungeon cells");
    for (size_t i = 0; i < n; ++i) if (walk[i] && i != first.descent) mask[n + i] = 2;
    auto repaired = repairLevelReach(blocked, true, false, {}, {}, 0, false, mask);
    check(repaired && repaired->removedDungeons == 1 && repaired->removedPois == 0 && !repaired->unrepaired &&
        blocked.dungeons.size() == 1 && blocked.surface.doors.size() == 3, "repair drops the unreachable dungeon only");
    // Repair: a building cut off by water loses its dungeon with it.
    auto island = base;
    fillRect(island.surface, 38, 38, 57, 55, SurfaceKind::Lake);
    fillRect(island.surface, 41, 41, 54, 52, SurfaceKind::Grass);
    fillRect(island.surface, 44, 44, 51, 49, SurfaceKind::Floor);
    check(bool(placeBuildingDungeons(island, 1729, tuning, shapes)) && island.dungeons.size() == 2, "island dungeon placed");
    auto cut = repairLevelReach(island, true, false, {}, {}, 0, false);
    check(cut && cut->removedDungeons == 1 && cut->removedPois >= 1 && !cut->unrepaired && island.dungeons.size() == 1 &&
        island.dungeons.front().building == DungeonRect{10, 10, 18, 16}, "removed building takes its dungeon");
    // Invalid input and cancellation.
    tuning.dungeonPercent = 101;
    auto bad = base;
    check(!placeBuildingDungeons(bad, 1729, tuning, shapes), "invalid percentage");
    tuning.dungeonPercent = 20;
    std::stop_source stop; stop.request_stop();
    auto cancelled = placeBuildingDungeons(bad, 1729, tuning, shapes, {}, stop.get_token());
    check(!cancelled && cancelled.error() == ReachabilityError::Cancelled, "dungeon cancellation");
    check(!levelPlanOf(world.surface, true, false, nullptr, {}, {}, {}, {}, {}, {}, world.dungeons), "dungeons need the cave floor");
    auto moved = world.dungeons;
    moved.front().arrival = moved.front().descent + 2;
    check(!levelPlanOf(world.surface, true, false, &*world.cave, {}, {}, {}, {}, {}, {}, moved), "arrival must neighbour the descent");
    auto overCave = base;
    overCave.cave->cells[world.dungeons.front().rooms.front().y0 * w + world.dungeons.front().rooms.front().x0] = CaveKind::Floor;
    check(!levelPlanOf(overCave.surface, true, false, &*overCave.cave, {}, {}, {}, {}, {}, {}, world.dungeons), "dungeon over cave Floor refused");

    // Generated settlements with caves: independent rules, 3D reachability and determinism on 48 seeds.
    uint64_t placed = 0, qualifying = 0, checkedSeeds = 0, rooms = 0;
    for (uint64_t seed = 4500; seed < 4548; ++seed) {
        SurfaceConfig config{{100, 100, 7, 128, 128}, seed, {}};
        config.tuning.buildingDensity = 60; config.tuning.pathLength = 40; config.tuning.ruinPercent = 20;
        config.tuning.caveEntrances = 8; config.tuning.rockPermille = 200;
        config.tuning.buildingDungeons = true; config.tuning.dungeonPercent = 40;
        if (seed % 2 == 0) { config.tuning.buildingStoreys = true; config.tuning.upperFloorPercent = 50; }
        if (seed % 3 == 0) { config.tuning.plateauMinTop = 32; config.tuning.plateauLevels = 2; }
        auto generated = generateSurface(config, {65001, 65001, 16}, {16384, 16384 * 8, 16384 * 96 + (1u << 20)});
        if (!generated) { check(false, "dungeon surface"); continue; }
        auto a = buildLevelWorld(*generated, {}, seed, config.tuning, {}, shapes, shapes, false, {}, shapes, shapes);
        auto b = buildLevelWorld(*generated, {}, seed, config.tuning, {}, shapes, shapes, false, {}, shapes, shapes);
        check(a && b && a->dungeons == b->dungeons, "generated dungeons deterministic");
        if (!a || !a->cave) continue;
        ++checkedSeeds;
        qualifying += a->dungeonReport.qualifying; placed += a->dungeons.size();
        for (const auto& d : a->dungeons) {
            rooms += d.rooms.size();
            check(dungeonValid(*a, d, shapes, shapes, why), std::format("generated dungeon rules seed {}: {}", seed, why));
        }
        for (size_t i = 0; i < a->dungeons.size(); ++i) for (size_t j = i + 1; j < a->dungeons.size(); ++j)
            for (const auto& p : a->dungeons[i].rooms) for (const auto& q : a->dungeons[j].rooms) {
                const auto dx = std::max<int64_t>({0, int64_t(q.x0) - p.x1, int64_t(p.x0) - q.x1});
                const auto dy = std::max<int64_t>({0, int64_t(q.y0) - p.y1, int64_t(p.y0) - q.y1});
                check(std::max(dx, dy) >= 5, "dungeons keep apart");
            }
        auto p = levelPlanOf(a->surface, true, false, &*a->cave, a->exits, shapes, a->caps, a->decor, {}, a->storeys, a->dungeons);
        auto r = p ? levelReachabilityOf(*p, {}) : std::unexpected(ReachabilityError::InvalidGrid);
        check(bool(r), "generated dungeon plan");
        if (!r) continue;
        // Dungeon cells are reached exactly when the building's door is.
        for (const auto& d : a->dungeons) {
            const auto doorIt = std::ranges::find_if(a->surface.doors, [&](uint32_t c) {
                return c % 128 >= d.building.x0 && c % 128 <= d.building.x1 && c / 128 >= d.building.y0 && c / 128 <= d.building.y1; });
            const bool doorReached = doorIt != a->surface.doors.end() && r->reached[*doorIt];
            std::vector<uint8_t> walk(16384);
            check(markDungeonCells(d, 128, 128, walk), "generated dungeon cells");
            for (uint32_t i = 0; i < 16384; ++i) if (walk[i]) check(bool(r->reached[16384 + i]) == doorReached, "dungeon reached with its door");
        }
    }
    check(checkedSeeds == 48 && placed > 0 && placed < qualifying && rooms >= 3 * placed / 2, "generated dungeons exist and are a share");
    std::cout << std::format("BUILDING_DUNGEON_CORE_PASS seeds={} qualifying={} placed={} rooms={}\n", checkedSeeds, qualifying, placed, rooms);
}

int main() {
    testBuildingDungeons();
    testBuildingStoreys();
    testMountainPasses();
    testFarExitsAndDoors();
    {
        CaveGrid cave {{0, 0, 8, 7, 3}, std::vector<CaveKind>(21, CaveKind::Solid)};
        cave.cells[8] = cave.cells[9] = cave.cells[11] = cave.cells[12] = CaveKind::Floor;
        CaveExitPlan exits; exits.exits.push_back({0, 8, 0});
        auto removed = removeUnenteredCaveComponents(cave, exits);
        check(removed && *removed == 1 && cave.cells[8] == CaveKind::Floor && cave.cells[9] == CaveKind::Floor &&
            cave.cells[11] == CaveKind::Solid && cave.cells[12] == CaveKind::Solid, "only entered cave component retained");
        check(removeUnenteredCaveComponents(cave, {}) == 1 && std::ranges::count(cave.cells, CaveKind::Floor) == 0,
            "no entrance means no generated cave floor");
        std::cout << "CAVE_WITH_ENTRANCE_ONLY_PASS\n";
    }
    testPlateaus();
    testExits();
	testExitCrossCheck();
	testEntrances();
	testEntranceStyles();
	testCave();
	testCaveShape();
	testValidator();
	testCrossCheck();
	if (failures) { std::cerr << failures << " failures\n"; return 1; }
	std::cout << "UNDERGROUND_PASS\n";
	return 0;
}
