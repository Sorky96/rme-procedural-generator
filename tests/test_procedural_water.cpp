// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/water_shape.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

constexpr uint32_t W = 200, H = 100;

struct Mask {
	std::vector<uint8_t> cells = std::vector<uint8_t>(W * H, 0);
	void set(int x, int y) { if (x >= 0 && y >= 0 && x < int(W) && y < int(H)) cells[size_t(y) * W + size_t(x)] = 1; }
	void stamp(int cx, int cy, int half) { for (int dy = -half; dy <= half; ++dy) for (int dx = -half; dx <= half; ++dx) set(cx + dx, cy + dy); }
};

WaterShapes shapesOf(const Mask& m) {
	WaterShapes s;
	check(addWaterShapes(m.cells, W, H, s, {}).has_value(), "addWaterShapes succeeds");
	return s;
}

void testRivers() {
	Mask straight;
	for (int x = 10; x < 70; ++x) straight.stamp(x, 40, 1);
	const WaterShapes a = shapesOf(straight);
	check(a.rivers.size() == 1 && a.lakeAreas.empty() && a.seaTiles == 0, "a 3-wide, 60-long strip is one river");
	if (a.rivers.size() == 1) {
		const RiverComponent& r = a.rivers[0];
		check(r.area == 3 * 62 && r.side == 62, "area and side of the strip");
		check(r.skeleton >= 55 && r.skeleton <= 62, "skeleton of the strip is about its length");
		const RiverSummary s = summarizeRivers(a.rivers);
		check(s.tortuosityPermille >= 880 && s.tortuosityPermille <= 1000, "a straight river has tortuosity at most 1000");
		check(s.straightPermille >= 900, "a straight river is almost all straight runs");
		check(s.widthMilli >= 2900 && s.widthMilli <= 3500, "width about 3 tiles");
	}

	Mask diagonal;
	for (int i = 0; i < 60; ++i) diagonal.stamp(10 + i, 20 + i / 1 % 70, 1);
	const WaterShapes d = shapesOf(diagonal);
	check(d.rivers.size() == 1, "a diagonal band is a river");
	if (d.rivers.size() == 1) check(summarizeRivers(d.rivers).straightPermille < 300, "a diagonal river has few straight runs");

	Mask wavy;
	for (int x = 5; x < 105; ++x) wavy.stamp(x, 45 + int(std::lround(18.0 * std::sin(x / 7.0))), 1);
	const WaterShapes w = shapesOf(wavy);
	check(w.rivers.size() == 1, "a sine river is one river");
	if (w.rivers.size() == 1) {
		const RiverSummary s = summarizeRivers(w.rivers);
		check(s.tortuosityPermille > 1250, "a meandering river is much longer than its box");
		check(s.straightPermille < 400, "a meandering river has few long straight runs");
		const RiverSummary straightSummary = summarizeRivers(a.rivers);
		check(s.tortuosityPermille > straightSummary.tortuosityPermille && s.straightPermille < straightSummary.straightPermille, "ordering: wavy is more tortuous and less straight than the strip");
	}
}

void testOthers() {
	Mask lake;
	for (int y = -12; y <= 12; ++y) for (int x = -12; x <= 12; ++x) if (x * x + y * y <= 144) lake.set(60 + x, 50 + y);
	const WaterShapes l = shapesOf(lake);
	check(l.rivers.empty() && l.lakeAreas.size() == 1 && l.lakeAreas[0] > 400, "a disc is a lake, not a river");

	Mask thin;
	for (int x = 10; x < 20; ++x) thin.stamp(x, 10, 1);
	check(shapesOf(thin).rivers.empty() && shapesOf(thin).lakeAreas.size() == 1, "a short strip is not a river");
	Mask tiny;
	for (int x = 10; x < 30; ++x) tiny.set(x, 10);
	const WaterShapes t = shapesOf(tiny);
	check(t.rivers.empty() && t.lakeAreas.size() == 1 && t.lakeAreas[0] == 20, "a 1-wide strip of 20 tiles is under the area minimum: a lake");

	Mask all;
	std::fill(all.cells.begin(), all.cells.end(), uint8_t {1});
	const WaterShapes sea = shapesOf(all);
	check(sea.seaTiles == W * H && sea.rivers.empty() && sea.lakeAreas.empty(), "20000 connected tiles are the sea and are skipped");

	Mask corner;
	for (int y = 10; y < 20; ++y) for (int x = 10; x < 20; ++x) corner.set(x, y);
	for (int y = 20; y < 30; ++y) for (int x = 20; x < 30; ++x) corner.set(x, y);
	check(shapesOf(corner).lakeAreas.size() == 2, "bodies touching only at a corner are separate (4-connected)");

	Mask empty;
	const WaterShapes e = shapesOf(empty);
	check(e.rivers.empty() && e.lakeAreas.empty() && e.seaTiles == 0 && summarizeRivers(e.rivers) == RiverSummary {}, "no water, no river");

	WaterShapes bad;
	check(addWaterShapes(std::vector<uint8_t>(10, 0), 3, 3, bad, {}).error() == StatsError::InvalidRange, "size mismatch refused");
	std::stop_source source;
	source.request_stop();
	Mask one;
	one.set(5, 5);
	check(addWaterShapes(one.cells, W, H, bad, source.get_token()).error() == StatsError::Cancelled, "cancelled");
}

void testSummary() {
	const std::vector<RiverComponent> rivers {{300, 100, 100, 0}, {300, 150, 100, 150}, {300, 200, 100, 100}};
	const RiverSummary s = summarizeRivers(rivers);
	check(s.count == 3 && s.tortuosityPermille == 1500 && s.straightPermille == 500 && s.widthMilli == 2000, "medians of tortuosity 1000/1500/2000, straight 0/1000/500, width 3000/2000/1500");
	const std::vector<RiverComponent> two {{100, 50, 50, 0}, {100, 100, 50, 100}};
	check(summarizeRivers(two).tortuosityPermille == 1500, "even count: mean of the middle two");
}

void testText() {
	WaterShapes s;
	s.seaTiles = 123456;
	s.rivers = {{300, 100, 90, 40}, {500, 200, 150, 0}};
	s.lakeAreas = {41, 5000, 12};
	const std::string text = serializeWaterShapes(s);
	check(text.starts_with("g4-water/1\n"), "schema line");
	const auto back = parseWaterShapes(text);
	check(back && back->seaTiles == s.seaTiles && back->rivers == s.rivers && back->lakeAreas == s.lakeAreas && serializeWaterShapes(*back) == text, "round trip");
	check(parseWaterShapes("g4-water/2\n").error() == ProfileError::UnsupportedSchema, "other version refused");
	check(parseWaterShapes("g4-water/1\n").error() == ProfileError::BadFormat, "missing sea line refused");
	check(parseWaterShapes("g4-water/1\nsea\t1\nsea\t2\n").error() == ProfileError::BadFormat, "duplicate sea line refused");
	check(parseWaterShapes("g4-water/1\nsea\t1\nriver\t1\t2\t3\n").error() == ProfileError::BadFormat, "river with three fields refused");
	check(parseWaterShapes("g4-water/1\nsea\t1\nlake\tx\n").error() == ProfileError::BadFormat, "non-numeric field refused");
	check(parseWaterShapes("g4-water/1\nsea\t1\nriver\t1\t2\t3\t99999999999\n").error() == ProfileError::BadFormat, "value beyond 32 bits refused");
}

void testSources() {
	auto map = SemanticMap::create(std::vector<FloorBounds> {{7, 0, 0, 80, 20}, {9, 0, 0, 80, 20}}, {10000}).value();
	const uint16_t grass = map.internGroundKey("grass").value();
	const uint16_t sea = map.internGroundKey("sea").value();
	const uint32_t t = bit(Trait::HasGround);
	for (int x = 0; x < 80; ++x) {
		for (int y = 0; y < 20; ++y) {
			const bool river = y >= 9 && y <= 11 && x >= 5 && x < 70;
			check(map.set(x, y, 7, river ? sea : grass, t).has_value(), "set z7");
			if (x < 40 && y < 3) check(map.set(x, y, 9, sea, t).has_value(), "set z9");
		}
	}
	const std::vector<std::string> keys {"", "grass", "sea"};
	const auto keymap = proposeKeyMap(keys, defaultKeyRules());
	const auto surface = extractWaterShapes(map, keymap, 0, 7, {});
	check(surface && surface->rivers.size() == 1 && surface->lakeAreas.empty(), "surface floor: one river");
	const auto both = extractWaterShapes(map, keymap, 0, 15, {});
	check(both && both->rivers.size() + both->lakeAreas.size() == 2, "both floors: the z9 strip is a body of its own");
	check(extractWaterShapes(map, keymap, 8, 15, {}).value().rivers.size() + extractWaterShapes(map, keymap, 8, 15, {}).value().lakeAreas.size() == 1, "underground range only");
	const std::vector<KeyMapEntry> missing {{"grass", StyleKind::Grass, {}}};
	check(extractWaterShapes(map, missing, 0, 7, {}).error() == StatsError::KeyNotMapped, "unmapped key is an error");
	check(extractWaterShapes(map, keymap, 5, 4, {}).error() == StatsError::InvalidRange, "reversed range refused");

	SurfaceGrid grid;
	grid.region = {0, 0, 7, 80, 20};
	grid.cells.assign(80 * 20, SurfaceKind::Grass);
	for (int x = 5; x < 70; ++x) for (int y = 9; y <= 11; ++y) grid.cells[size_t(y) * 80 + size_t(x)] = (x < 20 ? SurfaceKind::Lake : SurfaceKind::River);
	for (int x = 0; x < 3; ++x) grid.cells[size_t(x)] = SurfaceKind::Shore; // shore is not water
	const auto generated = waterShapesOf(grid);
	check(generated && generated->rivers.size() == 1 && generated->lakeAreas.empty(), "generator side: lake and river cells together form one river-shaped body, shore is ignored");
	std::stop_source source;
	source.request_stop();
	check(waterShapesOf(grid, source.get_token()).error() == StatsError::Cancelled, "generator side cancelled");
}

// The river swing of the generator makes rivers more tortuous and breaks up the long straight runs (D6).
void testMeanderEffect() {
	auto medians = [](uint32_t meander) {
		std::vector<uint32_t> tortuosity, straight;
		for (uint64_t seed = 1; seed <= 16; ++seed) {
			SurfaceConfig config {{100, 100, 7, 160, 160}, seed, {}};
			config.tuning.meander = meander;
			const SurfaceBudget budget {160 * 160, 160 * 160 * 8, 160 * 160 * 64 + (1u << 20)};
			const auto grid = generateSurface(config, MapExtent {65001, 65001, 16}, budget);
			check(grid.has_value(), "surface generated");
			if (!grid) continue;
			const auto shapes = waterShapesOf(*grid);
			check(shapes.has_value(), "shapes extracted");
			if (!shapes) continue;
			const RiverSummary summary = summarizeRivers(shapes->rivers);
			if (!summary.count) continue;
			tortuosity.push_back(summary.tortuosityPermille);
			straight.push_back(summary.straightPermille);
		}
		check(tortuosity.size() >= 12, "most seeds have a river");
		std::sort(tortuosity.begin(), tortuosity.end());
		std::sort(straight.begin(), straight.end());
		return std::pair {tortuosity.empty() ? 0u : tortuosity[tortuosity.size() / 2], straight.empty() ? 0u : straight[straight.size() / 2]};
	};
	const auto [plainT, plainS] = medians(0);
	const auto [swungT, swungS] = medians(8);
	check(swungT > plainT + 50, "meander 8 raises the median tortuosity by more than 5 percent of the box side");
	check(swungS + 50 < plainS, "meander 8 lowers the median share of straight runs");
	check(medians(0) == std::pair {plainT, plainS}, "the measurement is repeatable");
}

} // namespace

int main() {
	testRivers();
	testOthers();
	testSummary();
	testText();
	testSources();
	testMeanderEffect();
	if (failures == 0) std::cout << "OK\n";
	return failures == 0 ? 0 : 1;
}
