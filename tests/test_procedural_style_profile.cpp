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
	p.tuning = SurfaceTuning {20, 70, 12, 2, 20, 3, 90, 8, 250, 5000};
	return p;
}

void testSerialization() {
	const StyleProfile p = sample();
	const auto text = serializeProfile(p);
	check(text.has_value(), "serializes");
	if (!text) return;
	const auto back = parseProfile(*text);
	check(back && *back == p, "round trip is lossless, including the empty key and a key with a comma");
	check(back && serializeProfile(*back).value() == *text, "serialization is stable");
	check(text->starts_with("g4-profile/2\n"), "schema line first");

	check(parseProfile("g4-profile/3\n").error() == ProfileError::UnsupportedSchema, "other schema version refused");
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
	check(text && text->find("target\tshares\t600000") != std::string::npos && text->find("tuning\t20\t70") != std::string::npos, "target and tuning written");
	const auto back = parseProfile(text.value_or(""));
	check(back && back->target == p.target && back->tuning == p.tuning, "target and tuning round trip");
	p.target.reset();
	p.tuning.reset();
	const auto plain = serializeProfile(p);
	const auto plainBack = parseProfile(plain.value_or(""));
	check(plainBack && !plainBack->target && !plainBack->tuning && *plainBack == p, "a profile without them is valid");
	std::string cut = *text;
	cut.erase(cut.find("target\tadjacency\tDirt"), cut.find("target\tadjacency\tRock") - cut.find("target\tadjacency\tDirt"));
	check(parseProfile(cut).error() == ProfileError::BadFormat, "target with a missing adjacency row refused");
	check(parseProfile(*text + "tuning\t1\t2\t3\t4\t5\t6\t7\t8\t9\t10\n").error() == ProfileError::BadFormat, "second tuning line refused");
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
	testTargetBuild();
	testStatsText();
	testValidation();
	testCompatibility();
	testStats();
	if (failures == 0) std::cout << "OK\n";
	return failures == 0 ? 0 : 1;
}
