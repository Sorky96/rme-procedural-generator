// SPDX-License-Identifier: GPL-3.0-or-later
// Z entrances, the cave below and the reachability through Z (G5 iteration 27). Hand counted layouts plus a cross-check.
#include "procedural/core/reachability.h"
#include "procedural/core/underground.h"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <tuple>
#include <iostream>
#include <iterator>
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
		if (style == CaveEntranceStyle::RockFace) check(!caveEntranceAllowed(corner.cells, 11, 11, e, style), "rock corner rejected");
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
					for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) if (at(dx,dy) == SurfaceKind::Lake || at(dx,dy) == SurfaceKind::River || at(dx,dy) == SurfaceKind::Floor) expected = false;
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

// Independent oracle: enumerate every grid cell, sort candidate tuples and compare
// each prospective placement directly against every accepted exit (no occupancy masks).
CaveExitPlan bruteExits(const SurfaceGrid& g, const CaveGrid& c, std::span<const ExitShape> shapes, std::span<const uint8_t> occupied) {
	const int w = int(g.region.width), h = int(g.region.height);
	const auto shifted = [&](int p, int dx, int dy) {
		const int x = p % w + dx, y = p / w + dy;
		return x < 0 || y < 0 || x >= w || y >= h ? -1 : y * w + x;
	};
	const auto dest = [&](int p, ExitDirection d) {
		switch (d) {
			case ExitDirection::North: return shifted(p, 0, -1);
			case ExitDirection::South: return shifted(p, 0, 1);
			case ExitDirection::East: return shifted(p, 1, 0);
			case ExitDirection::West: return shifted(p, -1, 0);
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
		const int entrance = int(g.entrances[e]);
		std::vector<std::tuple<int, int, int>> candidates;
		for (int p = 0; p < w * h; ++p) {
			const int dx = p % w - entrance % w, dy = p / w - entrance / w;
			const int distance = dx * dx + dy * dy;
			if (distance > 0 && distance <= 5) candidates.emplace_back(distance, p / w, p % w);
		}
		std::ranges::sort(candidates);
		bool found = false;
		for (const auto& [distance, y, x] : candidates) {
			const int p = y * w + x;
			for (size_t si = 0; si < shapes.size(); ++si) {
				const auto& shape = shapes[si];
				const int t = shape.hasTail ? shifted(p, shape.tailDx, shape.tailDy) : p;
				const int d = dest(p, shape.direction);
				if (!surfaceOK(p) || !surfaceOK(d) || d == p || t < 0 || contains(g.entrances, t) ||
					c.cells[size_t(p)] != CaveKind::Floor || c.cells[size_t(t)] != CaveKind::Floor) continue;
				bool conflict = false;
				for (const auto& old : plan.exits) {
					const auto& os = shapes[old.shape];
					const int op = int(old.cell), ot = os.hasTail ? shifted(op, os.tailDx, os.tailDy) : op;
					const int od = dest(op, os.direction);
					conflict |= p == op || p == ot || t == op || t == ot || p == od || d == op || d == od;
				}
				if (conflict) continue;
				plan.exits.push_back({e, uint32_t(p), uint8_t(si)});
				found = true;
				break;
			}
			if (found) break;
		}
		if (!found) plan.skipped.push_back(e);
	}
	return plan;
}

void testExits() {
	auto g = grid(9, 9, SurfaceKind::Grass);
	g.entrances = {40}; g.cells[40] = SurfaceKind::Rock;
	auto c = emptyCave(g); disc(c, 4, 4);
	const ExitShape north[] = {{ExitDirection::North}};
	auto plan = placeCaveExits(g, c, north);
	check(plan && plan->exits.size() == 1 && plan->exits[0].cell == 31 && plan->exits[0].shape == 0 &&
		plan->exits[0].entrance == 0 && plan->skipped.empty(), "exit: closest north cell (4,3), destination (4,2), not entrance");
	const auto again = placeCaveExits(g, c, north);
	check(plan && again && samePlan(*plan, *again), "exit: deterministic");

	for (const auto rock : {31u, 22u}) {
		g.cells[rock] = SurfaceKind::Rock;
		const auto blocked = placeCaveExits(g, c, north);
		check(blocked && blocked->exits.size() == 1 && blocked->exits[0].cell == 39, "exit: Rock P or destination rejected");
		g.cells[rock] = SurfaceKind::Grass;
	}
	std::vector<uint8_t> occupied(81, 0);
	occupied[31] = 1;
	plan = placeCaveExits(g, c, north, occupied);
	check(plan && plan->exits.size() == 1 && plan->exits[0].cell == 39, "exit: occupied P excludes first candidate");
	occupied[31] = 0; occupied[22] = 1;
	plan = placeCaveExits(g, c, north, occupied);
	check(plan && plan->exits.size() == 1 && plan->exits[0].cell == 39, "exit: occupied destination excludes first candidate");
	g.fires = {31}; g.cells[31] = SurfaceKind::Path;
	plan = placeCaveExits(g, c, north);
	check(plan && plan->exits[0].cell == 39, "exit: fire excluded");
	g.fires.clear(); g.cells[31] = SurfaceKind::Grass;
	plan = placeCaveExits(g, c, ramps);
	check(plan && plan->exits.size() == 1 && plan->exits[0].cell == 31 && plan->exits[0].shape == 1,
		"exit: north ramp tail hits entrance, next shape West wins");

	g.cells.assign(81, SurfaceKind::Lake); g.cells[40] = SurfaceKind::Rock;
	plan = placeCaveExits(g, c, ramps);
	check(plan && plan->exits.empty() && plan->skipped == std::vector<uint32_t>{0}, "exit: water landing skipped");
	g.cells.assign(81, SurfaceKind::Grass);
	// Reverse spatial order checks that the entrance list, not cell order, controls placement.
	g.entrances = {41, 40}; g.cells[40] = g.cells[41] = SurfaceKind::Rock;
	disc(c, 5, 4);
	plan = placeCaveExits(g, c, ramps);
	check(plan && plan->exits.size() == 2 && plan->exits[0].cell == 32 && plan->exits[1].cell == 39 &&
		plan->exits[0].entrance == 0 && plan->exits[1].entrance == 1, "exit: close entrances have distinct P/tails/destinations in list order");
	check(plan && samePlan(*plan, bruteExits(g, c, ramps, {})), "exit: close layout matches independent checker");

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

int main() {
	testExits();
	testExitCrossCheck();
	testEntrances();
	testEntranceStyles();
	testCave();
	testValidator();
	testCrossCheck();
	if (failures) { std::cerr << failures << " failures\n"; return 1; }
	std::cout << "UNDERGROUND_PASS\n";
	return 0;
}
