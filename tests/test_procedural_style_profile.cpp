// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_profile.h"

#include <iostream>
#include <string>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

StyleKind kindOf(const char* name) {
	const std::vector<std::string> keys {name};
	return proposeKeyMap(keys, defaultKeyRules())[0].kind;
}

void testRules() {
	check(kindOf("") == StyleKind::NoGround, "empty key is NoGround");
	check(kindOf("grass") == StyleKind::Grass && kindOf("jungle grass") == StyleKind::Grass && kindOf("lawn") == StyleKind::Grass, "grass names");
	check(kindOf("GRASS") == StyleKind::Grass, "case does not matter");
	check(kindOf("grass closed stone pile") == StyleKind::Grass, "grass variant stays grass");
	check(kindOf("grassland") == StyleKind::Unmapped, "whole words only: grassland is not grass");
	check(kindOf("earth (stone border)") == StyleKind::Dirt && kindOf("dry earth") == StyleKind::Dirt && kindOf("frozen mud") == StyleKind::Dirt, "dirt names");
	check(kindOf("dirt mountain ground") == StyleKind::Rock, "rock is tried before dirt");
	check(kindOf("mountain top ground") == StyleKind::Rock && kindOf("rocky ground") == StyleKind::Rock, "rock names");
	check(kindOf("sea") == StyleKind::Water && kindOf("walkable sea") == StyleKind::Water && kindOf("clear water") == StyleKind::Water, "water names");
	check(kindOf("sand") == StyleKind::Shore && kindOf("sand (thick border)") == StyleKind::Shore, "sand is shore");
	check(kindOf("sandstone") == StyleKind::Other && kindOf("dark tiled sandstone") == StyleKind::Other, "sandstone is not sand");
	check(kindOf("brick roof, down") == StyleKind::Other && kindOf("wooden floor (with border)") == StyleKind::Other, "man-made grounds");
	check(kindOf("grass pitfall") == StyleKind::Other && kindOf("cave hole") == StyleKind::Other, "qualifiers win over the base ground");
	check(kindOf("lava") == StyleKind::Other && kindOf("tar pit") == StyleKind::Other && kindOf("swamp") == StyleKind::Other, "hazards and biome grounds");
	check(kindOf("item earth") == StyleKind::Dirt && kindOf("item mountain") == StyleKind::Rock && kindOf("item flowing water") == StyleKind::Water, "definition-named grounds use the same words");
	check(kindOf("item dirt wall") == StyleKind::Other && kindOf("item stone wall") == StyleKind::Other, "wall-like grounds are not terrain, even when named after dirt");
	check(kindOf("snow") == StyleKind::Unmapped && kindOf("white gravel") == StyleKind::Unmapped, "no rule: Unmapped, not guessed");
	const std::vector<std::string> keys {"", "grass", "snow"};
	const auto map = proposeKeyMap(keys, defaultKeyRules());
	check(map.size() == 3 && map[1].rule == "grass" && map[2].rule.empty() && map[0].key.empty(), "order kept, deciding rule recorded");
}

StyleProfile sample() {
	StyleProfile p;
	p.client = "10.98";
	p.otbSha256 = "aa11";
	p.datSha256 = "bb22";
	p.zMin = 0;
	p.zMax = 7;
	p.sources = {{"M05", 400}, {"M06", 600}};
	const std::vector<std::string> keys {"", "grass", "brick roof, down", "snow"};
	p.keymap = proposeKeyMap(keys, defaultKeyRules());
	p.stats.kindTiles[0] = 12;
	p.stats.kindTiles[7] = 3;
	p.stats.pairs[0][0] = 9;
	p.stats.pairs[0][4] = 1;
	StyleTarget t;
	t.sharesPpm = {600000, 200000, 100000, 50000, 50000};
	t.adjacencyPpm[0][0] = 500000;
	t.adjacencyPpm[0][4] = t.adjacencyPpm[4][0] = 30000;
	p.target = t;
	p.tuning = SurfaceTuning {20, 70, 12, 2, 20, 3, 90, 8, 250, 5000, 4};
	DecorContext d;
	for (size_t i = 0; i < d.cell.size(); ++i) d.cell[i] = {1000000, i + 1, i + 2, i + 3};
	p.decorTarget = d;
	p.decorTuning = DecorTuning {1500, 20, 150, 90, 40, 110};
	p.palette[static_cast<size_t>(StyleKind::Rock)] = "snowy mountain";
	p.palette[static_cast<size_t>(StyleKind::Water)] = "sea";
	p.decorPalette[0] = "snowy trees";
	p.decorPalette[2] = "small snowy rocks";
	return p;
}

void testSerialization() {
	const StyleProfile p = sample();
	const auto text = serializeProfile(p);
	check(text.has_value(), "serializes");
	if (!text) return;
	const auto back = parseProfile(*text);
    const auto legacy = parseProfile(*text + "plateauEdge\tmountain\n");
    check(legacy && *legacy == p, "legacy plateauEdge record is ignored and omitted on save");
    check(back && *back == p, "round trip is lossless, including the empty key and a key with a comma");
    check(back && serializeProfile(*back).value() == *text, "serialization is stable");
    check(text->starts_with("g4-profile/6\n"), "schema line first");

	check(parseProfile("g4-profile/7\n").error() == ProfileError::UnsupportedSchema, "other schema version refused");
	check(parseProfile("nonsense\n").error() == ProfileError::BadFormat, "no schema line refused");
	check(parseProfile(text->substr(0, text->find("stat\t"))).error() == ProfileError::BadFormat, "missing statistics refused");
	std::string unknownKind = *text;
	unknownKind.replace(unknownKind.find("Grass"), 5, "Moss");
	check(parseProfile(unknownKind).error() == ProfileError::UnknownKind, "unknown kind refused");
	check(parseProfile(*text + "extra\tline\n").error() == ProfileError::BadFormat, "unknown line refused");
	check(parseProfile(*text + "client\tother\n").error() == ProfileError::BadFormat, "duplicate client line refused");
	std::string twoLevels = *text;
	twoLevels.replace(twoLevels.find("level\tsurface"), 13, "level\tdeep");
	check(parseProfile(twoLevels).error() == ProfileError::BadFormat, "unknown level refused");
}

void testKeyMapText() {
	const std::vector<std::string> keys {"", "grass", "brick roof, down", "snow"};
	const auto map = proposeKeyMap(keys, defaultKeyRules());
	const auto text = serializeKeyMap(map);
	check(text.has_value() && text->starts_with("g4-keymap/1\n"), "key map serializes");
	const auto back = parseKeyMap(text.value_or(""));
	check(back && *back == map, "key map round trip, hand-set rule kept empty");
	std::string edited = text.value_or("");
	edited.replace(edited.find("Unmapped"), 8, "Shore");
	const auto changed = parseKeyMap(edited);
	check(changed && (*changed)[3].kind == StyleKind::Shore, "a hand edit is read back");
	check(parseKeyMap("g4-keymap/2\n").error() == ProfileError::UnsupportedSchema, "other version refused");
	check(parseKeyMap("key\tGrass\t-\tgrass\n").error() == ProfileError::BadFormat, "missing header refused");
	check(parseKeyMap("g4-keymap/1\nkey\tMoss\t-\tx\n").error() == ProfileError::UnknownKind, "unknown kind refused");
	check(parseKeyMap("g4-keymap/1\nkey\tGrass\t-\tx\nkey\tDirt\t-\tx\n").error() == ProfileError::DuplicateKey, "duplicate key refused");
	check(parseKeyMap("g4-keymap/1\n# note\nkey\tGrass\t-\tx\n").has_value(), "comments skipped");
}

void testTargetAndTuning() {
	StyleProfile p = sample();
	const auto text = serializeProfile(p);
	check(text && text->find("target\tshares\t600000") != std::string::npos && text->find("tuning\t20\t70") != std::string::npos && text->find("\t5000\t4\n") != std::string::npos, "target and tuning written, meander last");
	const auto back = parseProfile(text.value_or(""));
	check(back && back->target == p.target && back->tuning == p.tuning, "target and tuning round trip");
	check(text && text->find("decor\ttuning\t1500\t20\t150\t90\t40\t110") != std::string::npos && text->find("decor\tcell\t7\t1000000") != std::string::npos, "decor sections written");
	check(back && back->decorTarget == p.decorTarget && back->decorTuning == p.decorTuning, "decor target and tuning round trip");
	{
		std::string cutDecor = *text;
		cutDecor.erase(cutDecor.find("decor\tcell\t3\t"), cutDecor.find("decor\tcell\t4\t") - cutDecor.find("decor\tcell\t3\t"));
		check(parseProfile(cutDecor).error() == ProfileError::BadFormat, "decor target with a missing cell refused");
		check(parseProfile(*text + "decor\tcell\t0\t1\t1\t1\t1\n").error() == ProfileError::BadFormat, "duplicate decor cell refused");
		check(parseProfile(*text + "decor\ttuning\t1\t2\t3\t4\t5\t6\n").error() == ProfileError::BadFormat, "second decor tuning refused");
	}
	p.target.reset();
	p.tuning.reset();
	p.decorTarget.reset();
	p.decorTuning.reset();
	const auto plain = serializeProfile(p);
	const auto plainBack = parseProfile(plain.value_or(""));
	check(plainBack && !plainBack->target && !plainBack->tuning && *plainBack == p, "a profile without them is valid");
	std::string cut = *text;
	cut.erase(cut.find("target\tadjacency\tDirt"), cut.find("target\tadjacency\tRock") - cut.find("target\tadjacency\tDirt"));
	check(parseProfile(cut).error() == ProfileError::BadFormat, "target with a missing adjacency row refused");
	check(parseProfile(*text + "tuning\t1\t2\t3\t4\t5\t6\t7\t8\t9\t10\t11\n").error() == ProfileError::BadFormat, "second tuning line refused");
	check(parseProfile(text->substr(0, text->find("tuning\t20")) + "tuning\t20\t70\t12\t2\t20\t3\t90\t8\t250\t5000\n").error() == ProfileError::BadFormat, "a tuning line without meander (the old 10 values) refused");
	std::string overflow = *text;
	overflow.replace(overflow.find("600000"), 6, "4294967296");
	check(parseProfile(overflow).error() == ProfileError::BadFormat, "share beyond 32 bits refused");
}

MapStats mapStats(const char* alias, const char* family, uint64_t grass, uint64_t water, uint64_t grassGrass, uint64_t grassWater) {
	MapStats m {alias, family, {}};
	m.stats.kindTiles[static_cast<size_t>(StyleKind::Grass)] = grass;
	m.stats.kindTiles[static_cast<size_t>(StyleKind::Water)] = water;
	m.stats.kindTiles[static_cast<size_t>(StyleKind::Other)] = 999;
	m.stats.pairs[static_cast<size_t>(StyleKind::Grass)][static_cast<size_t>(StyleKind::Grass)] = grassGrass;
	m.stats.pairs[static_cast<size_t>(StyleKind::Grass)][static_cast<size_t>(StyleKind::Water)] = grassWater;
	return m;
}

void testPalette() {
	const std::vector<std::string> names {"", "grass", "jungle grass", "earth", "item earth", "mountain", "item mountain", "sand", "sea", "snow", "rare brush"};
	const auto keymap = proposeKeyMap(names, defaultKeyRules());
	const std::vector<KeyTiles> tiles {{"grass", 600}, {"jungle grass", 300}, {"earth", 100}, {"item earth", 9000}, {"mountain", 500},
		{"item mountain", 1000}, {"sand", 199}, {"sea", 1000}, {"snow", 5000}, {"rare brush", 900}, {"unknown key", 7000}, {"", 40000}};
	const StylePalette p = choosePalette(keymap, tiles);
	check(p[static_cast<size_t>(StyleKind::Grass)] == "grass", "the brushed key with the most tiles wins within its kind");
	check(p[static_cast<size_t>(StyleKind::Dirt)].empty(), "a kind that is mostly unbrushed items gets no brush (earth is 1 percent of the dirt tiles)");
	check(p[static_cast<size_t>(StyleKind::Rock)] == "mountain", "share exactly at the threshold (500 of 1500 is above 25 percent) is chosen");
	check(p[static_cast<size_t>(StyleKind::Shore)].empty(), "fewer than 200 tiles: no brush");
	check(p[static_cast<size_t>(StyleKind::Water)] == "sea", "single brush of its kind");
	const StylePalette strict = choosePalette(keymap, tiles, 200, 40);
	check(strict[static_cast<size_t>(StyleKind::Rock)].empty() && strict[static_cast<size_t>(StyleKind::Grass)] == "grass", "a higher share threshold drops weaker choices");
	check(choosePalette(keymap, {}, 200, 25) == StylePalette {}, "no tiles: empty palette");

	// Only brushes the generator can place: the next best usable one wins, and shares still count every tile.
	const std::set<std::string> usable {"jungle grass", "sea", "sand"};
	const StylePalette limited = choosePalette(keymap, tiles, 200, 25, &usable);
	check(limited[static_cast<size_t>(StyleKind::Grass)] == "jungle grass", "grass is not usable, jungle grass (30 percent of the grass tiles) is");
	check(limited[static_cast<size_t>(StyleKind::Rock)].empty(), "no usable rock brush: no entry");
	check(limited[static_cast<size_t>(StyleKind::Water)] == "sea" && limited[static_cast<size_t>(StyleKind::Shore)].empty(), "usable water kept; shore still below the tile minimum");
	const std::set<std::string> none;
	check(choosePalette(keymap, tiles, 200, 25, &none) == StylePalette {}, "an empty list of usable brushes allows nothing");

	// Same counts in another order, and a tie between two keys: the smaller name, whatever the order.
	std::vector<KeyTiles> shuffled(tiles.rbegin(), tiles.rend());
	check(choosePalette(keymap, shuffled) == p, "the choice does not depend on the order of the tiles");
	const std::vector<KeyTiles> tie {{"jungle grass", 400}, {"grass", 400}};
	check(choosePalette(keymap, tie)[static_cast<size_t>(StyleKind::Grass)] == "grass", "tie: the smaller name");

	StyleProfile profile = sample();
	const auto text = serializeProfile(profile);
	check(text && text->find("palette\tRock\tsnowy mountain\n") != std::string::npos && text->find("palette\tWater\tsea\n") != std::string::npos, "palette lines written");
	check(text && text->find("palette\tGrass") == std::string::npos, "empty entries are not written");
	const auto back = parseProfile(text.value_or(""));
	check(back && back->palette == profile.palette, "palette round trip");
	check(parseProfile(*text + "palette\tRock\tother\n").error() == ProfileError::BadFormat, "second palette entry for one kind refused");
	check(parseProfile(*text + "palette\tOther\tx\n").error() == ProfileError::UnknownKind, "palette for a non-natural kind refused");
	check(parseProfile(*text + "palette\tGrass\t\n").error() == ProfileError::BadFormat, "empty palette name refused");
	profile.palette[0] = "a\tb";
	check(validateProfile(profile).error() == ProfileError::InvalidKey, "tab in a palette name refused");
}

void testDecorPalette() {
	const std::vector<KeyTiles> items {{"trees", 300}, {"snowy trees", 900}, {"bushes", 150}, {"grass tufts", 600}, {"small rocks", 80}, {"kamienie", 400},
		{"normal bed", 5000}, {"wall lamp", 7000}, {"dead trees", 100}};
	const StyleDecorPalette p = chooseDecorPalette(items);
	check(p[0] == "snowy trees", "most items among tree brushes (900 of 1300 = 69 percent)");
	check(p[1] == "grass tufts", "bush: grass tufts beat bushes");
	check(p[2] == "kamienie", "stone: furniture and unclassified brushes do not count; small rocks is under the minimum");
	const std::set<std::string> usable {"trees", "bushes", "small rocks"};
	const StyleDecorPalette limited = chooseDecorPalette(items, 100, 25, &usable);
	check(limited[0].empty() && limited[1].empty() && limited[2].empty(), "usable brushes only: trees 23 percent and bushes 20 percent are below 25, small rocks under the minimum");
	const StyleDecorPalette lower = chooseDecorPalette(items, 100, 20, &usable);
	check(lower[0] == "trees" && lower[1] == "bushes" && lower[2].empty(), "a lower share threshold admits them");
	check(chooseDecorPalette({}) == StyleDecorPalette {}, "no items: empty palette");
	std::vector<KeyTiles> shuffled(items.rbegin(), items.rend());
	check(chooseDecorPalette(shuffled) == p, "independent of the order");
	const std::vector<KeyTiles> tie {{"palm trees", 400}, {"banana tree", 400}};
	check(chooseDecorPalette(tie)[0] == "banana tree", "tie: the smaller name");

	StyleProfile profile = sample();
	const auto text = serializeProfile(profile);
	check(text && text->find("doodad\tTree\tsnowy trees\n") != std::string::npos && text->find("doodad\tStone\tsmall snowy rocks\n") != std::string::npos && text->find("doodad\tBush") == std::string::npos, "doodad lines written, empty entries not");
	const auto back = parseProfile(text.value_or(""));
	check(back && back->decorPalette == profile.decorPalette, "doodad palette round trip");
	check(parseProfile(*text + "doodad\tTree\tother\n").error() == ProfileError::BadFormat, "second entry for one kind refused");
	check(parseProfile(*text + "doodad\tFern\tx\n").error() == ProfileError::UnknownKind, "unknown doodad kind refused");
	profile.decorPalette[1] = "a\tb";
	check(validateProfile(profile).error() == ProfileError::InvalidKey, "tab in a doodad name refused");
}

void testTargetBuild() {
	const auto one = targetFromStats(mapStats("a", "f", 750, 250, 600, 100).stats);
	check(one && one->sharesPpm[0] == 750000 && one->sharesPpm[4] == 250000 && one->sharesPpm[1] == 0, "shares of natural tiles only (Other ignored)");
	// 700 pairs: GG 600 (counted in both cells of the diagonal), GW 100 (one cell each way).
	check(one && one->adjacencyPpm[0][0] == 600 * 2 * 1'000'000 / 1400 && one->adjacencyPpm[0][4] == one->adjacencyPpm[4][0] && one->adjacencyPpm[0][4] == 100 * 1'000'000 / 1400, "adjacency is symmetric");
	uint64_t sum = 0;
	for (const auto& row : one->adjacencyPpm) for (const uint32_t v : row) sum += v;
	check(sum >= 999'990 && sum <= 1'000'000, "adjacency sums to one million");
	StyleStats empty;
	check(!targetFromStats(empty), "no tiles: no target");
	empty.kindTiles[0] = 5;
	check(!targetFromStats(empty), "no pairs: no target");

	// Two maps of family F (all grass / all water) and one of family G (half and half): each family counts as one half.
	const std::vector<MapStats> maps {
		mapStats("a1", "F", 1000, 0, 100, 0), mapStats("a2", "F", 0, 1000, 0, 0), mapStats("b1", "G", 500, 500, 100, 100), mapStats("tiny", "H", 3, 0, 1, 0)};
	// a2 has no pairs: unusable. tiny is below the tile minimum.
	const auto build = buildTarget(maps, 100);
	check(build.has_value() && build->excluded == std::vector<std::string> {"a2", "tiny"} && build->used.size() == 2, "unusable and tiny maps are excluded and named");
	check(build && build->familyCount == 2 && build->used[0].weightPermille == 500 && build->used[1].weightPermille == 500, "families weigh the same");
	check(build && build->target.sharesPpm[0] == 750000 && build->target.sharesPpm[4] == 250000, "weighted shares: (1.0 + 0.5) / 2 grass");
	const std::vector<MapStats> many {mapStats("a", "F", 1000, 0, 100, 1), mapStats("b", "F", 1000, 0, 100, 1), mapStats("c", "F", 1000, 0, 100, 1), mapStats("d", "G", 0, 1000, 0, 0)};
	const auto weighted = buildTarget(std::vector<MapStats>(many.begin(), many.begin() + 3), 100);
	check(weighted && weighted->familyCount == 1 && weighted->used[0].weightPermille == 333, "three maps of one family share the weight");
	check(buildTarget(std::vector<MapStats> {mapStats("tiny", "H", 3, 0, 1, 0)}, 100).error() == TargetError::NothingUsable, "nothing usable is an error");
	check(buildTarget({}, 0).error() == TargetError::NothingUsable, "no maps is an error");
}

void testStatsText() {
	StyleStats stats;
	stats.kindTiles[2] = 77;
	stats.pairs[2][3] = 5;
	stats.pairs[7][7] = 9;
	const std::string text = serializeStats(stats);
	check(text.starts_with("g4-kindstats/1\n"), "stats schema line");
	const auto back = parseStats(text);
	check(back && *back == stats && serializeStats(*back) == text, "stats round trip, stable");
	check(parseStats("g4-kindstats/2\n").error() == ProfileError::UnsupportedSchema, "other stats version refused");
	check(parseStats(text.substr(0, text.size() - 30)).error() == ProfileError::BadFormat, "cut stats refused");
	check(parseStats(text + "stat\tkind_tiles\t1\t2\t3\t4\t5\t6\t7\t8\n").error() == ProfileError::BadFormat, "duplicate kind_tiles refused");
}

void testValidation() {
	StyleProfile p = sample();
	p.keymap.push_back({"grass", StyleKind::Grass, {}});
	check(serializeProfile(p).error() == ProfileError::DuplicateKey, "duplicate key refused");
	p = sample();
	p.keymap.push_back({"a\tb", StyleKind::Other, {}});
	check(validateProfile(p).error() == ProfileError::InvalidKey, "tab in key refused");
	p = sample();
	p.zMax = 8;
	check(validateProfile(p).error() == ProfileError::InvalidRange, "surface profile cannot reach floor 8");
	p = sample();
	p.level = StyleLevel::Underground;
	p.zMin = 7;
	p.zMax = 9;
	check(validateProfile(p).error() == ProfileError::InvalidRange, "underground profile cannot include floor 7");
	p.zMin = 8;
	p.zMax = 15;
	check(validateProfile(p).has_value() && parseProfile(serializeProfile(p).value())->level == StyleLevel::Underground, "underground profile valid");
	p = sample();
	p.zMin = 5;
	p.zMax = 3;
	check(validateProfile(p).error() == ProfileError::InvalidRange, "reversed span refused");
	p = sample();
	p.client.clear();
	check(validateProfile(p).error() == ProfileError::EmptyField, "empty client refused");
	p = sample();
	p.sources.push_back({"", 1});
	check(validateProfile(p).error() == ProfileError::EmptyField, "empty source alias refused");
}

void testCompatibility() {
	const StyleProfile p = sample();
	check(checkProfile(p, "10.98", "aa11", "bb22", 7) == ProfileCompatibility::Compatible, "same client, assets and floor");
	check(checkProfile(p, "1098-BlackTek", "aa11", "bb22", 7) == ProfileCompatibility::WrongClient, "other client refused");
	check(checkProfile(p, "10.98", "zz", "bb22", 7) == ProfileCompatibility::WrongAssets, "other items.otb refused");
	check(checkProfile(p, "10.98", "aa11", "zz", 7) == ProfileCompatibility::WrongAssets, "other Tibia.dat refused");
	check(checkProfile(p, "10.98", "aa11", "bb22", 8) == ProfileCompatibility::WrongLevel, "underground floor refused by a surface profile");
}

void testStats() {
	auto map = SemanticMap::create(std::vector<FloorBounds> {{7, 0, 0, 3, 2}, {8, 0, 0, 2, 1}}, {1000}).value();
	const uint16_t grass = map.internGroundKey("grass").value();
	const uint16_t water = map.internGroundKey("clear water").value();
	const uint16_t cave = map.internGroundKey("cave").value();
	const uint32_t t = bit(Trait::HasGround);
	// z=7:  G G W      z=8: C .
	//       G . W
	check(map.set(0, 0, 7, grass, t) && map.set(1, 0, 7, grass, t) && map.set(2, 0, 7, water, t) && map.set(0, 1, 7, grass, t) && map.set(2, 1, 7, water, t), "set z7");
	check(map.set(0, 0, 8, cave, t) && map.set(1, 0, 8, noGroundKey, 0), "set z8 (a tile without ground)");
	const std::vector<std::string> keys {"", "grass", "clear water", "cave"};
	const auto keymap = proposeKeyMap(keys, defaultKeyRules());

	const auto surface = accumulateKindStats(map, keymap, 0, 7, {});
	check(surface.has_value(), "surface stats");
	if (surface) {
		const auto G = static_cast<size_t>(StyleKind::Grass), W = static_cast<size_t>(StyleKind::Water);
		check(surface->kindTiles[G] == 3 && surface->kindTiles[W] == 2, "tiles per kind on the surface");
		check(surface->pairs[G][G] == 2 && surface->pairs[G][W] == 1 && surface->pairs[W][W] == 1, "pairs: east and south, absent tiles skipped");
		uint64_t total = 0;
		for (const auto& row : surface->pairs) for (const uint64_t v : row) total += v;
		check(total == 4, "no other pairs");
	}
	const auto under = accumulateKindStats(map, keymap, 8, 15, {});
	check(under && under->kindTiles[static_cast<size_t>(StyleKind::Other)] == 1 && under->kindTiles[static_cast<size_t>(StyleKind::NoGround)] == 1, "underground floor counted apart");
	check(under && under->pairs[static_cast<size_t>(StyleKind::Other)][static_cast<size_t>(StyleKind::NoGround)] == 1, "pair with a ground-less tile");
	const auto both = accumulateKindStats(map, keymap, 0, 15, {});
	check(both && both->kindTiles[static_cast<size_t>(StyleKind::Grass)] == 3 && both->kindTiles[static_cast<size_t>(StyleKind::Other)] == 1, "both floors");
	check(accumulateKindStats(map, keymap, 3, 2, {}).error() == StatsError::InvalidRange, "reversed range refused");

	const std::vector<KeyMapEntry> missing {{"grass", StyleKind::Grass, {}}};
	check(accumulateKindStats(map, missing, 0, 7, {}).error() == StatsError::KeyNotMapped, "a ground key missing from the key map is an error");
	std::stop_source source;
	source.request_stop();
	check(accumulateKindStats(map, keymap, 0, 7, source.get_token()).error() == StatsError::Cancelled, "cancelled");
}

} // namespace

int main() {
	testRules();
	testSerialization();
	testKeyMapText();
	testTargetAndTuning();
	testPalette();
	testDecorPalette();
	testTargetBuild();
	testStatsText();
	testValidation();
	testCompatibility();
	testStats();
	if (failures == 0) std::cout << "OK\n";
	return failures == 0 ? 0 : 1;
}
