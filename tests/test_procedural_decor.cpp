// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/decor_context.h"
#include "procedural/core/doodad_class.h"

#include <algorithm>
#include <iostream>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

void testClassification() {
	struct Case { const char* name; DecorKind kind; };
	const Case cases[] = {
		{"trees", DecorKind::Tree}, {"dead mangrove tree (on water)", DecorKind::Tree}, {"broken palm trees", DecorKind::Tree}, {"tree stubs", DecorKind::Tree},
		{"large trunk", DecorKind::Tree}, {"snowy trees", DecorKind::Tree},
		{"bushes", DecorKind::Bush}, {"swamp plants", DecorKind::Bush}, {"grass tufts", DecorKind::Bush}, {"yellow flowers", DecorKind::Bush},
		{"giant mushrooms", DecorKind::Bush}, {"lilypads", DecorKind::Bush}, {"Tall Jungle Grass", DecorKind::Bush},
		{"kamienie", DecorKind::Stone}, {"small rocks", DecorKind::Stone}, {"stone cave rocks with mud", DecorKind::Stone},
		{"weird shaped stones", DecorKind::Stone}, {"staglamites", DecorKind::Stone},
		{"debris", DecorKind::None}, {"normal bed", DecorKind::None}, {"fire field (decaying)", DecorKind::None}, {"head stone cobra", DecorKind::None},
		{"statue1", DecorKind::None}, {"", DecorKind::None}, {"grassland", DecorKind::None}, {"treehouse", DecorKind::None},
	};
	for (const Case& c : cases) check(classifyDoodadBrush(c.name) == c.kind, c.name);
	check(classifyDoodadBrush("rocky tree") == DecorKind::Tree, "first rule wins: tree before rock");
	const DoodadRule only[] {{"debris", DecorKind::Stone}};
	check(classifyDoodadBrush("debris", only) == DecorKind::Stone && classifyDoodadBrush("trees", only) == DecorKind::None, "custom rules");
	check(decorTraitBit(DecorKind::Tree) == bit(Trait::TreeDoodad) && decorTraitBit(DecorKind::None) == 0, "trait bits");
	check(decorTraitBit(DecorKind::Tree) != decorTraitBit(DecorKind::Bush) && decorTraitBit(DecorKind::Bush) != decorTraitBit(DecorKind::Stone), "distinct bits");
	check((bit(Trait::TreeDoodad) | bit(Trait::BushDoodad) | bit(Trait::StoneDoodad)) >> traitCount >> 1 != 0 &&
	      (bit(Trait::TreeDoodad) & ((1u << traitCount) - 1)) == 0, "the new bits lie beyond the reported traits");
}

SurfaceGrid builtGrid(uint32_t side, uint64_t seed) {
	SurfaceConfig config {{100, 100, 7, side, side}, seed, {}};
	const SurfaceBudget budget {uint64_t(side) * side, uint64_t(side) * side * 8, uint64_t(side) * side * 64 + (1u << 20)};
	return generateSurface(config, MapExtent {65001, 65001, 16}, budget).value();
}

void testTuning() {
	for (const uint64_t seed : {1u, 2u, 3u}) {
		const SurfaceGrid grid = builtGrid(96, seed);
		const auto a = generateDecoration(grid, seed), b = generateDecoration(grid, seed, DecorTuning {});
		check(a && b && a->cells == b->cells, "a default DecorTuning changes nothing");
	}
	const SurfaceGrid grid = builtGrid(128, 5);
	auto count = [](const DecorGrid& d, DecorKind k) { return std::count(d.cells.begin(), d.cells.end(), k); };
	const auto base = generateDecoration(grid, 5);
	DecorTuning bushy;
	bushy.bushPermille = 1000;
	bushy.waterBushPermille = 1000;
	const auto more = generateDecoration(grid, 5, bushy);
	check(base && more && count(*more, DecorKind::Bush) > count(*base, DecorKind::Bush), "higher bush chance places more bushes");
	DecorTuning none;
	none.treeScalePermille = 0;
	const auto noTrees = generateDecoration(grid, 5, none);
	check(noTrees && count(*noTrees, DecorKind::Tree) == 0 && count(*base, DecorKind::Tree) > 0, "tree scale 0 places no tree away from water");
	DecorTuning wet;
	wet.waterTreePermille = 1000;
	const auto trees = generateDecoration(grid, 5, wet);
	check(trees && count(*trees, DecorKind::Tree) > count(*base, DecorKind::Tree), "trees may be allowed near water");
	DecorTuning bad;
	bad.treeScalePermille = maxTreeScalePermille + 1;
	check(generateDecoration(grid, 5, bad).error() == SurfaceError::InvalidParameters, "tree scale beyond the limit refused");
	bad = {};
	bad.bushPermille = 1001;
	check(generateDecoration(grid, 5, bad).error() == SurfaceError::InvalidParameters, "chance above 1000 refused");
	bad = {};
	bad.rockStonePermille = 1001;
	check(generateDecoration(grid, 5, bad).error() == SurfaceError::InvalidParameters, "stone chance above 1000 refused");
	DecorTuning limit;
	limit.treeScalePermille = maxTreeScalePermille;
	check(generateDecoration(grid, 5, limit).has_value(), "the limit itself is valid");
}

// R G G G L   (Rock, Grass x3, Lake); reach 2: grass[1] near rock only, grass[2] near both, grass[3] near water only.
SurfaceGrid rowGrid() {
	SurfaceGrid g;
	g.region = {0, 0, 7, 5, 1};
	g.cells = {SurfaceKind::Rock, SurfaceKind::Grass, SurfaceKind::Grass, SurfaceKind::Grass, SurfaceKind::Lake};
	return g;
}

void testGeneratedContext() {
	DecorGrid d;
	d.region = {0, 0, 7, 5, 1};
	d.cells = {DecorKind::None, DecorKind::Bush, DecorKind::Tree, DecorKind::Stone, DecorKind::None};
	const DecorContext c = decorContextOf(rowGrid(), d);
	const auto& rockOnly = c.cell[DecorContext::index(false, false, true)];
	const auto& both = c.cell[DecorContext::index(false, true, true)];
	const auto& waterOnly = c.cell[DecorContext::index(false, true, false)];
	check(rockOnly.tiles == 1 && rockOnly.bushes == 1 && rockOnly.trees == 0, "near rock only");
	check(both.tiles == 1 && both.trees == 1, "near both");
	check(waterOnly.tiles == 1 && waterOnly.stones == 1, "near water only");
	uint64_t tiles = 0;
	for (const auto& cell : c.cell) tiles += cell.tiles;
	check(tiles == 3, "only grass and dirt cells are counted");
}

void testReferenceContext() {
	// The same row as a reference map, with an absent tile and a ground-less tile that must not change reach.
	auto map = SemanticMap::create(std::vector<FloorBounds> {{7, 0, 0, 5, 1}, {9, 0, 0, 1, 1}}, {1000}).value();
	const uint16_t grass = map.internGroundKey("grass").value();
	const uint16_t rock = map.internGroundKey("mountain").value();
	const uint16_t sand = map.internGroundKey("sand").value();
	const uint32_t g = bit(Trait::HasGround);
	check(map.set(0, 0, 7, rock, g) && map.set(1, 0, 7, grass, g | bit(Trait::BushDoodad)) && map.set(2, 0, 7, grass, g | bit(Trait::TreeDoodad)) &&
	      map.set(3, 0, 7, grass, g | bit(Trait::StoneDoodad) | bit(Trait::TreeDoodad)) && map.set(4, 0, 7, sand, g), "set");
	check(map.set(0, 0, 9, grass, g | bit(Trait::TreeDoodad)).has_value(), "set underground");
	const std::vector<std::string> keys {"", "grass", "mountain", "sand"};
	const auto keymap = proposeKeyMap(keys, defaultKeyRules());
	const auto c = extractDecorContext(map, keymap, 0, 7, {});
	check(c.has_value(), "extract");
	if (!c) return;
	const auto& rockOnly = c->cell[DecorContext::index(false, false, true)];
	const auto& both = c->cell[DecorContext::index(false, true, true)];
	const auto& waterOnly = c->cell[DecorContext::index(false, true, false)];
	check(rockOnly.tiles == 1 && rockOnly.bushes == 1, "shore (sand) counts as water seed; bush near rock only");
	check(both.tiles == 1 && both.trees == 1, "near both");
	check(waterOnly.tiles == 1 && waterOnly.stones == 1 && waterOnly.trees == 1, "a tile can hold a stone and a tree");
	uint64_t tiles = 0;
	for (const auto& cell : c->cell) tiles += cell.tiles;
	check(tiles == 3, "floor 9 is outside the range");
	const auto under = extractDecorContext(map, keymap, 8, 15, {});
	check(under && under->cell[DecorContext::index(false, false, false)].tiles == 1 && under->cell[DecorContext::index(false, false, false)].trees == 1, "underground range");
	const std::vector<KeyMapEntry> missing {{"grass", StyleKind::Grass, {}}};
	check(extractDecorContext(map, missing, 0, 7, {}).error() == StatsError::KeyNotMapped, "unmapped ground key is an error");
	check(extractDecorContext(map, keymap, 5, 4, {}).error() == StatsError::InvalidRange, "reversed range refused");
	std::stop_source source;
	source.request_stop();
	check(extractDecorContext(map, keymap, 0, 7, source.get_token()).error() == StatsError::Cancelled, "cancelled");
}

void testDistance() {
	DecorContext ref, got;
	ref.cell[0] = {1000, 100, 200, 50};
	got.cell[0] = {500, 50, 100, 100};   // rates 100/200/200 vs 100/200/100 per mille -> only the stone term differs
	ref.cell[5] = {100, 50, 50, 50};     // below the 200-tile minimum: skipped
	const DecorDistance d = decorDistance(ref, got);
	check(d.terms == 3 && d.sumPermille == (100 - 100) + (200 - 200) + (200 - 50) , "mean over comparable contexts only");
	check(decorDistance(DecorContext {}, got).terms == 0, "no comparable context is terms == 0");
	check(decorDistance(ref, ref).sumPermille == 0, "zero against itself");
}

void testDecorTarget() {
	auto ctx = [](uint64_t tiles, uint64_t trees, uint64_t bushes, uint64_t stones) {
		MapDecor m;
		m.context.cell[0] = {tiles, trees, bushes, stones};
		return m;
	};
	// Family F: two maps with tree rates 100 and 300 per mille -> 200; family G: one map with 500. Equal families: (200 + 500) / 2.
	std::vector<MapDecor> maps {ctx(1000, 100, 0, 0), ctx(10000, 3000, 0, 0), ctx(400, 200, 0, 0)};
	maps[0].family = maps[1].family = "F";
	maps[2].family = "G";
	const DecorContext t = buildDecorTarget(maps);
	check(t.cell[0].tiles == 1'000'000 && t.cell[0].trees == 350'000, "family-weighted mean of the rates, in virtual units");
	check(t.cell[1].tiles == 0, "a cell no map qualifies for stays empty");
	// A map below the tile minimum in this cell does not take part.
	maps.push_back(ctx(100, 100, 0, 0));
	maps.back().family = "H";
	check(buildDecorTarget(maps).cell[0].trees == 350'000, "maps under 200 tiles are ignored");
	check(buildDecorTarget(maps, 50).cell[0].trees != 350'000, "...with the minimum lowered it takes part");
	check(buildDecorTarget({}).cell[0].tiles == 0, "no maps: empty target");
	// A large map does not dominate: same family shares the weight equally.
	std::vector<MapDecor> big {ctx(1'000'000, 0, 0, 0), ctx(300, 300, 0, 0)};
	big[0].family = big[1].family = "F";
	check(buildDecorTarget(big).cell[0].trees == 500'000, "map size does not matter, only its rate");
}

void testSerialization() {
	DecorContext c;
	for (size_t i = 0; i < c.cell.size(); ++i) c.cell[i] = {i * 100 + 1, i, i + 1, i + 2};
	const std::string text = serializeDecorContext(c);
	check(text.starts_with("g4-decorctx/1\n"), "schema line");
	const auto back = parseDecorContext(text);
	check(back && *back == c && serializeDecorContext(*back) == text, "round trip, stable");
	check(parseDecorContext("g4-decorctx/2\n").error() == ProfileError::UnsupportedSchema, "other version refused");
	check(parseDecorContext(text.substr(0, text.size() - 12)).error() == ProfileError::BadFormat, "cut text refused");
	check(parseDecorContext(text + "cell\t0\t1\t1\t1\t1\n").error() == ProfileError::BadFormat, "duplicate cell refused");
	check(parseDecorContext("g4-decorctx/1\ncell\t9\t1\t1\t1\t1\n").error() == ProfileError::BadFormat, "cell index out of range refused");
	check(parseDecorContext("g4-decorctx/1\ncell\t0\t1\t1\t1\n").error() == ProfileError::BadFormat, "missing field refused");
}

void testFit() {
	const SurfaceTuning surface;
	const EvalSpec spec {128, {1, 2, 3, 4}};
	DecorTuning known;
	known.treeScalePermille = 2500;
	known.waterTreePermille = 120;
	known.bushPermille = 150;
	known.waterBushPermille = 220;
	known.dirtStonePermille = 90;
	known.rockStonePermille = 200;
	const auto target = evaluateDecor(surface, known, spec, {});
	check(target.has_value(), "target from a known tuning");
	if (!target) return;
	const auto fitted = fitDecorTuning(*target, surface, spec, {});
	check(fitted.has_value(), "fit");
	if (!fitted) return;
	const auto builtIn = evaluateDecor(surface, DecorTuning {}, spec, {});
	const auto atFit = evaluateDecor(surface, *fitted, spec, {});
	check(builtIn && atFit, "evaluations");
	if (!(builtIn && atFit)) return;
	const DecorDistance before = decorDistance(*target, *builtIn), after = decorDistance(*target, *atFit);
	check(before.terms == after.terms && before.terms > 0, "same terms compared");
	check(after.sumPermille * 4 < before.sumPermille, "the fit gets at least four times closer than the built-in rules");
	check(fitted->waterTreePermille > 0 && fitted->treeScalePermille > 1000, "learned trees near water and a denser forest");
	check(*fitDecorTuning(*target, surface, spec, {}) == *fitted, "repeatable");

	// Too little data in every context: the built-in values stay.
	DecorContext tiny;
	tiny.cell[0] = {50, 5, 5, 5};
	const auto kept = fitDecorTuning(tiny, surface, spec, {});
	check(kept && *kept == DecorTuning {}, "contexts under 200 tiles keep the built-in chances");
	std::stop_source source;
	source.request_stop();
	check(fitDecorTuning(*target, surface, spec, source.get_token()).error() == FitError::Cancelled, "fit cancelled");
	check(evaluateDecor(surface, DecorTuning {}, {128, {}}, {}).error() == FitError::InvalidSpec, "no seeds refused");
	DecorTuning bad;
	bad.bushPermille = 1001;
	check(evaluateDecor(surface, bad, spec, {}).error() == FitError::Generator, "invalid tuning is a generator error");
}

} // namespace

int main() {
	testClassification();
	testTuning();
	testGeneratedContext();
	testReferenceContext();
	testDistance();
	testDecorTarget();
	testSerialization();
	testFit();
	if (failures == 0) std::cout << "OK\n";
	return failures == 0 ? 0 : 1;
}
