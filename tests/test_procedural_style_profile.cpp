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
	check(text->starts_with("g4-profile/1\n"), "schema line first");

	check(parseProfile("g4-profile/2\n").error() == ProfileError::UnsupportedSchema, "other schema version refused");
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
	testValidation();
	testCompatibility();
	testStats();
	if (failures == 0) std::cout << "OK\n";
	return failures == 0 ? 0 : 1;
}
