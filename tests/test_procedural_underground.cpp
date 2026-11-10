// SPDX-License-Identifier: GPL-3.0-or-later
// Z entrances, the cave below and the reachability through Z (G5 iteration 27). Hand counted layouts plus a cross-check.
#include "procedural/core/reachability.h"
#include "procedural/core/underground.h"

#include <algorithm>
#include <cstdlib>
#include <format>
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
void checkPlateauGeometry(const SurfaceGrid& base, const PlateauGrid& p, uint32_t minTop, std::span<const ExitShape> shapes)
{
    const int w = int(base.region.width), h = int(base.region.height);
    const auto n = base.cells.size();
    std::vector<uint8_t> candidate(n), expected(n), seen(n);
    for (int y = 3; y < h - 3; ++y) {
        for (int x = 3; x < w - 3; ++x) {
            int distance = std::min({x + 1, y + 1, w - x, h - y});
            for (int yy = 0; yy < h; ++yy) {
                for (int xx = 0; xx < w; ++xx) {
                    if (base.cells[size_t(yy) * w + xx] != SurfaceKind::Rock) {
                        distance = std::min(distance, std::max(std::abs(x - xx), std::abs(y - yy)));
                    }
                }
            }
            candidate[size_t(y) * w + x] = uint32_t(distance) >= p.inset;
        }
    }
    std::vector<uint32_t> destinations;
    const int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
    for (const auto& ramp : p.ramps) {
        auto shape = shapes[ramp.shape];
        auto d = size_t(shape.direction);
        check(ramp.destination == uint32_t(int(ramp.cell) + dx[d] + w * dy[d]), "oracle ramp destination");
        bool hit = false;
        for (uint32_t step = 0; step < p.inset; ++step) {
            int x = int(ramp.destination % w) + int(step) * dx[d], y = int(ramp.destination / w) + int(step) * dy[d];
            check(x >= 0 && y >= 0 && x < w && y < h, "oracle corridor bounds");
            if (x < 0 || y < 0 || x >= w || y >= h) {
                break;
            }
            auto i = size_t(y) * w + x;
            check(base.cells[i] == SurfaceKind::Rock, "oracle corridor stays on base");
            if (candidate[i]) {
                destinations.push_back(uint32_t(i));
                hit = true;
                break;
            }
            expected[i] = 3;
            check(p.landings[i], "oracle corridor no decor marker");
        }
        check(hit, "oracle corridor reaches Top within inset");
    }
    for (size_t start = 0; start < n; ++start) {
        if (candidate[start] && !seen[start]) {
            std::vector<size_t> q {start};
            seen[start] = 1;
            for (size_t k = 0; k < q.size(); ++k) {
                int x = int(q[k] % w), y = int(q[k] / w);
                for (int d = 0; d < 4; ++d) {
                    int xx = x + dx[d], yy = y + dy[d];
                    if (xx >= 0 && yy >= 0 && xx < w && yy < h) {
                        auto j = size_t(yy) * w + xx;
                        if (candidate[j] && !seen[j]) {
                            seen[j] = 1;
                            q.push_back(j);
                        }
                    }
                }
            }
            bool reached = false;
            for (auto i : q) {
                reached |= std::ranges::find(destinations, i) != destinations.end();
            }
            if (reached) {
                check(q.size() >= minTop, "oracle minimum component");
                for (auto i : q) {
                    expected[i] = 2;
                }
                for (auto i : q) {
                    int x = int(i % w), y = int(i / w);
                    for (int yy = y - 1; yy <= y + 1; ++yy) {
                        for (int xx = x - 1; xx <= x + 1; ++xx) {
                            auto j = size_t(yy) * w + xx;
                            if (base.cells[j] == SurfaceKind::Rock && !expected[j]) {
                                expected[j] = 1;
                            }
                        }
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < n; ++i) {
        check((p.cells[i] == PlateauKind::Top) == (expected[i] == 2), "oracle Top exact");
        check((p.cells[i] != PlateauKind::None) == bool(expected[i]), "oracle footprint exact");
    }
}

void testPlateaus()
{
    constexpr ExitShape ramps[]
        = {{ExitDirection::North, 0, 1, true},
           {ExitDirection::South, 0, -1, true},
           {ExitDirection::East, -1, 0, true},
           {ExitDirection::West, 1, 0, true}};
    auto g = grid(24, 24, SurfaceKind::Grass);
    fillRect(g, 4, 4, 15, 15, SurfaceKind::Rock);
    auto p = generatePlateau(g, 1729, 64, ramps);
    check(p && p->components == 1 && p->ramps.size() == 1, "hand plateau: one ramp");
    check(
        p && std::ranges::count(p->cells, PlateauKind::Top) == 100 && std::ranges::count(p->cells, PlateauKind::Edge) == 44,
        "hand plateau: Top 100 Edge 44"
    );
    auto small = generatePlateau(g, 1729, 101, ramps);
    check(small && small->components == 0, "small Top removed");
    auto noRamp = generatePlateau(g, 1729, 64, {});
    check(noRamp && std::ranges::count(noRamp->cells, PlateauKind::None) == 576, "inaccessible plateau removed");
    std::vector<uint8_t> blocked(g.cells.size(), 1);
    auto occupied = generatePlateau(g, 1729, 64, ramps, blocked);
    check(occupied && occupied->components == 0, "occupied ramps removed");
    auto off = generatePlateau(g, 1729, 0, ramps);
    check(off && off->components == 0, "plateau off");
    std::stop_source source;
    source.request_stop();
    check(!generatePlateau(g, 1729, 64, ramps, {}, source.get_token()), "plateau cancellation");
    for (uint64_t seed = 1; seed <= 60; ++seed) {
        // Independent brute erosion on randomized rectangular rock patches and holes.
        auto s = grid(48, 48, SurfaceKind::Grass);
        fillRect(s, 4, 4, 42, 42, SurfaceKind::Rock);
        StableRandom r(seed, 123);
        for (int i = 0; i < 30; ++i) {
            s.cells[size_t(r.bounded(s.cells.size()))] = SurfaceKind::Grass;
        }
        auto a = generatePlateau(s, seed, 16, ramps);
        auto b = generatePlateau(s, seed, 16, ramps);
        check(a && b && a->cells == b->cells && a->ramps == b->ramps, "plateau deterministic");
        if (!a) {
            continue;
        }
        std::vector<uint8_t> eligible(s.cells.size());
        for (int y = 3; y < 45; ++y) {
            for (int x = 3; x < 45; ++x) {
                bool ok = true;
                for (int yy = 0; yy < 48; ++yy) {
                    for (int xx = 0; xx < 48; ++xx) {
                        if (s.cells[size_t(yy) * 48 + xx] != SurfaceKind::Rock && std::max(std::abs(xx - x), std::abs(yy - y)) < 2) {
                            ok = false;
                        }
                    }
                }
                eligible[size_t(y) * 48 + x] = ok;
            }
        }
        std::vector<uint8_t> seen(eligible.size());
        for (size_t start = 0; start < eligible.size(); ++start) {
            if (eligible[start] && !seen[start]) {
                std::vector<size_t> q {start};
                seen[start] = 1;
                for (size_t k = 0; k < q.size(); ++k) {
                    int x = int(q[k] % 48), y = int(q[k] / 48);
                    for (auto [xx, yy] : {std::pair {x - 1, y}, std::pair {x + 1, y}, std::pair {x, y - 1}, std::pair {x, y + 1}}) {
                        if (xx >= 0 && yy >= 0 && xx < 48 && yy < 48) {
                            size_t j = size_t(yy) * 48 + xx;
                            if (eligible[j] && !seen[j]) {
                                seen[j] = 1;
                                q.push_back(j);
                            }
                        }
                    }
                }
                if (q.size() < 16) {
                    for (auto i : q) {
                        eligible[i] = 0;
                    }
                }
            }
        }
        for (size_t i = 0; i < eligible.size(); ++i) {
            check((a->cells[i] == PlateauKind::Top) == bool(eligible[i]), "brute Top erosion matches");
        }
        auto d = generatePlateauDecoration(*a, seed, {});
        check(bool(d), "plateau vegetation generated");
        for (size_t i = 0; d && i < eligible.size(); ++i) {
            check(d->cells[i] == DecorKind::None || (a->cells[i] == PlateauKind::Top && !a->landings[i]), "vegetation only Top");
        }
        for (size_t i = 0; i < a->ramps.size(); ++i) {
            auto p = a->ramps[i].cell;
            check(s.cells[p] == SurfaceKind::Grass, "ramp on open surface");
            for (size_t j = 0; j < i; ++j) {
                check(
                    std::max(std::abs(int(p % 48) - int(a->ramps[j].cell % 48)), std::abs(int(p / 48) - int(a->ramps[j].cell / 48))) >= 16,
                    "ramp spacing"
                );
            }
        }
    }

    auto limited = generatePlateau(g, 1729, 16, ramps, {}, {}, 15);
    check(limited && limited->components, "limited rectangle retains plateau");
    check(
        limited && std::ranges::count_if(limited->cells, [](auto k) { return k != PlateauKind::None; }) * 100 <= 576 * 15,
        "limited rectangle includes band and corridor in percent"
    );
    check(limited && std::ranges::count(limited->landings, uint8_t(1)) > 1, "inset creates landing corridor");
    check(!generatePlateau(g, 1729, 16, ramps, {}, {}, 101), "invalid percent refused");
    // Independent exhaustive distances to non-rock, then independent flood fill of the result.
    // The oracle checks each possible global inset, not the production distance transform.
    for (uint64_t seed = 1; seed <= 60; ++seed) {
        auto s = grid(48, 48, SurfaceKind::Grass);
        fillRect(s, 4, 4, 42, 42, SurfaceKind::Rock);
        StableRandom random(seed, 912);
        for (int hole = 0; hole < 15; ++hole) {
            s.cells[size_t(random.bounded(s.cells.size()))] = SurfaceKind::Grass;
        }
        auto p = generatePlateau(s, seed, 16, ramps, {}, {}, 30);
        auto repeat = generatePlateau(s, seed, 16, ramps, {}, {}, 30);
        check(
            p && repeat && p->cells == repeat->cells && p->ramps == repeat->ramps && p->landings == repeat->landings, "limited determinism"
        );
        if (!p) {
            continue;
        }
        check(
            std::ranges::count_if(p->cells, [](auto k) { return k != PlateauKind::None; }) * 100 <= 48 * 48 * 30, "limited randomized area"
        );
        std::vector<int> distance(48 * 48, 48);
        for (int y = 0; y < 48; ++y) {
            for (int x = 0; x < 48; ++x) {
                auto& d = distance[size_t(y) * 48 + x];
                d = std::min({x + 1, y + 1, 48 - x, 48 - y});
                for (int yy = 0; yy < 48; ++yy) {
                    for (int xx = 0; xx < 48; ++xx) {
                        if (s.cells[size_t(yy) * 48 + xx] != SurfaceKind::Rock) {
                            d = std::min(d, std::max(std::abs(xx - x), std::abs(yy - y)));
                        }
                    }
                }
            }
        }
        bool matched = false;
        for (int inset = 2; inset < 48 && !matched; ++inset) {
            std::vector<uint8_t> candidate(48 * 48), seen(48 * 48);
            for (int y = 3; y < 45; ++y) {
                for (int x = 3; x < 45; ++x) {
                    candidate[size_t(y) * 48 + x] = distance[size_t(y) * 48 + x] >= inset;
                }
            }
            for (size_t start = 0; start < candidate.size(); ++start) {
                if (candidate[start] && !seen[start]) {
                    std::vector<size_t> q {start};
                    seen[start] = 1;
                    for (size_t k = 0; k < q.size(); ++k) {
                        int x = int(q[k] % 48), y = int(q[k] / 48);
                        for (auto [xx, yy] : {std::pair {x - 1, y}, std::pair {x + 1, y}, std::pair {x, y - 1}, std::pair {x, y + 1}}) {
                            if (xx >= 0 && yy >= 0 && xx < 48 && yy < 48) {
                                auto j = size_t(yy) * 48 + xx;
                                if (candidate[j] && !seen[j]) {
                                    seen[j] = 1;
                                    q.push_back(j);
                                }
                            }
                        }
                    }
                    bool retained = false;
                    for (auto i : q) {
                        retained |= p->cells[i] == PlateauKind::Top;
                    }
                    if (q.size() < 16 || !retained) {
                        for (auto i : q) {
                            candidate[i] = 0;
                        }
                    }
                }
            }
            matched = true;
            for (size_t i = 0; i < candidate.size(); ++i) {
                if (bool(candidate[i]) != (p->cells[i] == PlateauKind::Top)) {
                    matched = false;
                }
            }
        }
        check(matched, "limited independent erosion and component search");
        checkPlateauGeometry(s, *p, 16, ramps);
        auto lower = *p;
        for (int level = 2; level <= 3; ++level) {
            auto upper = generateNextPlateau(lower, seed ^ (uint64_t(level) << 48), ramps, {}, 30);
            auto again = generateNextPlateau(lower, seed ^ (uint64_t(level) << 48), ramps, {}, 30);
            check(upper && again && upper->cells == again->cells && upper->ramps == again->ramps, "upper determinism");
            if (!upper) {
                break;
            }
            SurfaceGrid base;
            base.region = lower.region;
            base.cells.assign(lower.cells.size(), SurfaceKind::Grass);
            for (size_t i = 0; i < base.cells.size(); ++i) {
                bool destination = false;
                for (const auto& ramp : lower.ramps) {
                    destination
                        |= ramp.destination == i || ramp.cell == i || int64_t(i) == 2 * int64_t(ramp.cell) - int64_t(ramp.destination);
                }
                if (lower.cells[i] != PlateauKind::None && !lower.landings[i] && !destination) {
                    base.cells[i] = SurfaceKind::Rock;
                }
            }
            checkPlateauGeometry(base, *upper, 30, ramps);
            const auto count
                = [](const PlateauGrid& g) { return std::ranges::count_if(g.cells, [](auto k) { return k != PlateauKind::None; }); };
            check(uint64_t(count(*upper)) * 100 <= uint64_t(count(lower)) * 30, "upper percent of level below");
            auto decor = generatePlateauDecoration(*upper, seed, {});
            check(bool(decor), "upper vegetation");
            for (size_t i = 0; i < upper->cells.size(); ++i) {
                if (upper->cells[i] == PlateauKind::Top) {
                    int x = int(i % 48), y = int(i / 48);
                    for (int yy = y - 1; yy <= y + 1; ++yy) {
                        for (int xx = x - 1; xx <= x + 1; ++xx) {
                            auto j = size_t(yy) * 48 + xx;
                            check(lower.cells[j] != PlateauKind::None && !lower.landings[j], "upper independent base erosion");
                        }
                    }
                }
                if (decor && decor->cells[i] != DecorKind::None) {
                    check(upper->cells[i] == PlateauKind::Top && !upper->landings[i], "upper decor only Top");
                }
            }
            for (const auto& r : upper->ramps) {
                check(lower.cells[r.cell] == PlateauKind::Top && !lower.landings[r.cell], "upper P lower Top");
                check(upper->cells[r.cell] == PlateauKind::None, "upper P outside projection");
                auto shape = ramps[r.shape];
                const int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
                auto d = size_t(shape.direction);
                check(r.destination == uint32_t(int(r.cell) + dx[d] + 48 * dy[d]), "upper D forward");
                auto tail = size_t(int(r.cell) + shape.tailDx + 48 * shape.tailDy);
                check(lower.cells[tail] != PlateauKind::None && !lower.landings[tail], "upper tail open");
            }
            lower = std::move(*upper);
        }
    }
    auto broad = grid(80, 80, SurfaceKind::Grass);
    fillRect(broad, 4, 4, 74, 74, SurfaceKind::Rock);
    auto first = generatePlateau(broad, 1729, 64, ramps);
    check(first && first->components, "hand large lower plateau");
    auto second = generateNextPlateau(*first, 1729, ramps);
    check(second && second->components && second->inset == 4, "hand upper minimum inset supports ramp and tail");
    auto third = generateNextPlateau(*second, 1730, ramps);
    check(third && third->components && third->inset == 4, "hand third level");
    PlateauGrid narrow {g.region, std::vector<PlateauKind>(g.cells.size(),PlateauKind::None), std::vector<uint8_t>(g.cells.size())};
    for(int y=6;y<14;++y) for(int x=6;x<14;++x) narrow.cells[size_t(y)*24+x]=PlateauKind::Top;
    auto tooSmall=generateNextPlateau(narrow,1729,ramps);
    check(tooSmall && !tooSmall->components && std::ranges::count(tooSmall->cells,PlateauKind::Top)==0,
          "hand upper component of four cells is below minimum 30");
    auto without = generateNextPlateau(*first, 1729, {});
    check(without && !without->components, "upper without ramp removed");
    auto above = generateNextPlateau(*without, 1729, ramps);
    check(above && !above->components, "levels above inaccessible level removed");
    auto capped = generateNextPlateau(*first, 1729, ramps, {}, 10);
    const auto covered = [](const PlateauGrid& g) { return std::ranges::count_if(g.cells, [](auto k) { return k != PlateauKind::None; }); };
    check(capped && uint64_t(covered(*capped)) * 100 <= uint64_t(covered(*first)) * 10, "hand upper relative area cap");
    std::cout << "PLATEAU_LEVELS_LIMIT_CROSS_CHECK 60/60\n";
    std::cout << "PLATEAU_CROSS_CHECK 60/60\n";
}

int main() {
    testPlateaus();
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
