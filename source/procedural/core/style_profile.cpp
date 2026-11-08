// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_profile.h"

#include <algorithm>
#include <charconv>
#include <map>
#include <new>
#include <set>
#include <unordered_map>

namespace rme::procedural {
namespace {

constexpr std::array<const char*, styleKindCount> kindNames {"Grass", "Dirt", "Rock", "Shore", "Water", "Other", "Unmapped", "NoGround"};

// Order is priority. Qualifiers that turn a ground into something else come first, then the base grounds.
// Water is checked before Rock and Rock before Dirt so that e.g. "dirt mountain ground" is Rock.
constexpr KeyRule defaultRules[] = {
	{"roof", StyleKind::Other}, {"floor", StyleKind::Other}, {"tiles", StyleKind::Other}, {"tiled", StyleKind::Other},
	{"stairs", StyleKind::Other}, {"ladder", StyleKind::Other}, {"trapdoor", StyleKind::Other}, {"pitfall", StyleKind::Other},
	{"hole", StyleKind::Other}, {"pickhole", StyleKind::Other}, {"ropespot", StyleKind::Other}, {"drawbridge", StyleKind::Other},
	{"bridge", StyleKind::Other}, {"marquee", StyleKind::Other}, {"pavement", StyleKind::Other}, {"cobblestone", StyleKind::Other},
	{"marble", StyleKind::Other}, {"brick", StyleKind::Other}, {"bamboo", StyleKind::Other}, {"void", StyleKind::Other},
	{"lava", StyleKind::Other}, {"tar", StyleKind::Other}, {"swamp", StyleKind::Other}, {"cave", StyleKind::Other},
	{"sandstone", StyleKind::Other}, {"wall", StyleKind::Other},
	{"sea", StyleKind::Water}, {"water", StyleKind::Water},
	{"mountain", StyleKind::Rock}, {"rocky", StyleKind::Rock},
	{"dirt", StyleKind::Dirt}, {"earth", StyleKind::Dirt}, {"mud", StyleKind::Dirt},
	{"grass", StyleKind::Grass}, {"lawn", StyleKind::Grass},
	{"sand", StyleKind::Shore},
};

void appendStatLines(std::string& out, const StyleStats& stats) {
	out += "stat\tkind_tiles";
	for (const uint64_t v : stats.kindTiles) out += '\t' + std::to_string(v);
	out += '\n';
	for (size_t a = 0; a < styleKindCount; ++a) {
		out += std::string("stat\tpairs\t") + kindNames[a];
		for (const uint64_t v : stats.pairs[a]) out += '\t' + std::to_string(v);
		out += '\n';
	}
}

std::vector<std::string_view> splitTabs(std::string_view line, size_t maxFields) {
	std::vector<std::string_view> fields;
	while (fields.size() + 1 < maxFields) {
		const size_t tab = line.find('\t');
		if (tab == std::string_view::npos) break;
		fields.push_back(line.substr(0, tab));
		line.remove_prefix(tab + 1);
	}
	fields.push_back(line);
	return fields;
}

bool parseUnsigned(std::string_view text, uint64_t& out) {
	const auto r = std::from_chars(text.data(), text.data() + text.size(), out);
	return r.ec == std::errc {} && r.ptr == text.data() + text.size();
}

bool parseU32(std::string_view text, uint32_t& out) {
	uint64_t v = 0;
	if (!parseUnsigned(text, v) || v > UINT32_MAX) return false;
	out = static_cast<uint32_t>(v);
	return true;
}

bool parseInt(std::string_view text, int& out) {
	const auto r = std::from_chars(text.data(), text.data() + text.size(), out);
	return r.ec == std::errc {} && r.ptr == text.data() + text.size();
}

bool hasTokens(std::string_view name, std::string_view token) {
	size_t i = 0;
	while (i < name.size()) {
		auto isWord = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };
		while (i < name.size() && !isWord(name[i])) ++i;
		size_t j = i;
		while (j < name.size() && isWord(name[j])) ++j;
		if (j - i == token.size()) {
			bool same = true;
			for (size_t k = 0; k < token.size(); ++k) {
				char c = name[i + k];
				if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
				if (c != token[k]) { same = false; break; }
			}
			if (same) return true;
		}
		i = j;
	}
	return false;
}

} // namespace

const char* styleKindName(StyleKind kind) noexcept {
	const size_t i = static_cast<size_t>(kind);
	return i < kindNames.size() ? kindNames[i] : "?";
}

bool parseStyleKind(std::string_view name, StyleKind& out) noexcept {
	for (size_t i = 0; i < kindNames.size(); ++i) {
		if (name == kindNames[i]) { out = static_cast<StyleKind>(i); return true; }
	}
	return false;
}

std::span<const KeyRule> defaultKeyRules() noexcept { return defaultRules; }

std::vector<KeyMapEntry> proposeKeyMap(std::span<const std::string> keys, std::span<const KeyRule> rules) {
	std::vector<KeyMapEntry> result;
	result.reserve(keys.size());
	for (const std::string& key : keys) {
		KeyMapEntry entry {key, key.empty() ? StyleKind::NoGround : StyleKind::Unmapped, {}};
		if (!key.empty()) {
			for (const KeyRule& rule : rules) {
				if (hasTokens(key, rule.token)) { entry.kind = rule.kind; entry.rule = std::string(rule.token); break; }
			}
		}
		result.push_back(std::move(entry));
	}
	return result;
}

std::expected<std::string, ProfileError> serializeKeyMap(std::span<const KeyMapEntry> keymap) {
	std::string out(keyMapSchema);
	out += '\n';
	std::set<std::string_view> seen;
	for (const auto& e : keymap) {
		if (e.key.find_first_of("\t\r\n") != std::string::npos || e.rule.find_first_of("\t\r\n") != std::string::npos) return std::unexpected(ProfileError::InvalidKey);
		if (!seen.insert(e.key).second) return std::unexpected(ProfileError::DuplicateKey);
		out += std::string("key\t") + styleKindName(e.kind) + '\t' + (e.rule.empty() ? "-" : e.rule) + '\t' + e.key + '\n';
	}
	return out;
}

std::expected<std::vector<KeyMapEntry>, ProfileError> parseKeyMap(std::string_view text) {
	std::vector<KeyMapEntry> keymap;
	std::set<std::string> seen;
	bool header = false;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		if (line.empty() || line.front() == '#') continue;
		if (!header) {
			if (line == keyMapSchema) { header = true; continue; }
			return std::unexpected(line.starts_with("g4-keymap/") ? ProfileError::UnsupportedSchema : ProfileError::BadFormat);
		}
		const auto f = splitTabs(line, 4);
		if (f.size() != 4 || f[0] != "key") return std::unexpected(ProfileError::BadFormat);
		KeyMapEntry e;
		if (!parseStyleKind(f[1], e.kind)) return std::unexpected(ProfileError::UnknownKind);
		e.rule = f[2] == "-" ? std::string {} : std::string(f[2]);
		e.key = f[3];
		if (!seen.insert(e.key).second) return std::unexpected(ProfileError::DuplicateKey);
		keymap.push_back(std::move(e));
	}
	if (!header) return std::unexpected(ProfileError::BadFormat);
	return keymap;
}

std::expected<void, ProfileError> validateProfile(const StyleProfile& p) {
	auto bad = [](std::string_view s) { return s.find_first_of("\t\r\n") != std::string_view::npos; };
	if (p.client.empty() || p.otbSha256.empty() || p.datSha256.empty()) return std::unexpected(ProfileError::EmptyField);
	if (bad(p.client) || bad(p.otbSha256) || bad(p.datSha256)) return std::unexpected(ProfileError::InvalidKey);
	if (p.zMin < 0 || p.zMax > 15 || p.zMin > p.zMax) return std::unexpected(ProfileError::InvalidRange);
	if (p.level == StyleLevel::Surface ? p.zMax > 7 : p.zMin < 8) return std::unexpected(ProfileError::InvalidRange);
	for (const auto& s : p.sources) {
		if (s.alias.empty()) return std::unexpected(ProfileError::EmptyField);
		if (bad(s.alias)) return std::unexpected(ProfileError::InvalidKey);
	}
	std::set<std::string_view> seen;
	for (const auto& e : p.keymap) {
		if (bad(e.key) || bad(e.rule)) return std::unexpected(ProfileError::InvalidKey);
		if (!seen.insert(e.key).second) return std::unexpected(ProfileError::DuplicateKey);
	}
	return {};
}

std::expected<std::string, ProfileError> serializeProfile(const StyleProfile& p) {
	if (auto v = validateProfile(p); !v) return std::unexpected(v.error());
	std::string out(profileSchema);
	out += '\n';
	out += "client\t" + p.client + '\n';
	out += p.level == StyleLevel::Surface ? "level\tsurface\n" : "level\tunderground\n";
	out += "floors\t" + std::to_string(p.zMin) + '\t' + std::to_string(p.zMax) + '\n';
	out += "assets\totb_sha256\t" + p.otbSha256 + '\n';
	out += "assets\tdat_sha256\t" + p.datSha256 + '\n';
	for (const auto& s : p.sources) out += "source\t" + s.alias + '\t' + std::to_string(s.weightPermille) + '\n';
	for (const auto& e : p.keymap) out += std::string("key\t") + styleKindName(e.kind) + '\t' + (e.rule.empty() ? "-" : e.rule) + '\t' + e.key + '\n';
	appendStatLines(out, p.stats);
	if (p.target) {
		out += "target\tshares";
		for (const uint32_t v : p.target->sharesPpm) out += '\t' + std::to_string(v);
		out += '\n';
		for (size_t a = 0; a < naturalKindCount; ++a) {
			out += std::string("target\tadjacency\t") + kindNames[a];
			for (const uint32_t v : p.target->adjacencyPpm[a]) out += '\t' + std::to_string(v);
			out += '\n';
		}
	}
	if (p.tuning) {
		const SurfaceTuning& t = *p.tuning;
		out += "tuning";
		for (const uint32_t v : {t.featureSize, t.lakePermille, t.minLakeCells, t.riverCount, t.minRiverLength, t.shoreWidth,
			t.rockPermille, t.minRockCells, t.dirtPermille, t.transitionWidth}) out += '\t' + std::to_string(v);
		out += '\n';
	}
	return out;
}

std::expected<StyleProfile, ProfileError> parseProfile(std::string_view text) {
	StyleProfile p;
	bool header = false, haveClient = false, haveLevel = false, haveFloors = false, haveOtb = false, haveDat = false, haveTiles = false;
	std::array<bool, styleKindCount> havePairs {};
	std::array<bool, naturalKindCount> haveAdjacency {};
	bool haveShares = false;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		if (line.empty()) continue;
		if (!header) {
			if (line == profileSchema) { header = true; continue; }
			return std::unexpected(line.starts_with("g4-profile/") ? ProfileError::UnsupportedSchema : ProfileError::BadFormat);
		}
		const auto f = splitTabs(line, line.starts_with("key\t") ? 4 : 12);
		const std::string_view type = f[0];
		if (type == "client" && f.size() == 2 && !haveClient) { p.client = f[1]; haveClient = true; }
		else if (type == "level" && f.size() == 2 && !haveLevel) {
			if (f[1] == "surface") p.level = StyleLevel::Surface;
			else if (f[1] == "underground") p.level = StyleLevel::Underground;
			else return std::unexpected(ProfileError::BadFormat);
			haveLevel = true;
		}
		else if (type == "floors" && f.size() == 3 && !haveFloors) {
			if (!parseInt(f[1], p.zMin) || !parseInt(f[2], p.zMax)) return std::unexpected(ProfileError::BadFormat);
			haveFloors = true;
		}
		else if (type == "assets" && f.size() == 3 && f[1] == "otb_sha256" && !haveOtb) { p.otbSha256 = f[2]; haveOtb = true; }
		else if (type == "assets" && f.size() == 3 && f[1] == "dat_sha256" && !haveDat) { p.datSha256 = f[2]; haveDat = true; }
		else if (type == "source" && f.size() == 3) {
			uint64_t w = 0;
			if (!parseUnsigned(f[2], w) || w > 1000) return std::unexpected(ProfileError::BadFormat);
			p.sources.push_back({std::string(f[1]), static_cast<uint32_t>(w)});
		}
		else if (type == "key" && f.size() == 4) {
			KeyMapEntry e;
			if (!parseStyleKind(f[1], e.kind)) return std::unexpected(ProfileError::UnknownKind);
			e.rule = f[2] == "-" ? std::string {} : std::string(f[2]);
			e.key = f[3];
			p.keymap.push_back(std::move(e));
		}
		else if (type == "stat" && f.size() == 2 + styleKindCount && f[1] == "kind_tiles" && !haveTiles) {
			for (size_t i = 0; i < styleKindCount; ++i) if (!parseUnsigned(f[2 + i], p.stats.kindTiles[i])) return std::unexpected(ProfileError::BadFormat);
			haveTiles = true;
		}
		else if (type == "stat" && f.size() == 3 + styleKindCount && f[1] == "pairs") {
			StyleKind row;
			if (!parseStyleKind(f[2], row)) return std::unexpected(ProfileError::UnknownKind);
			const size_t a = static_cast<size_t>(row);
			if (havePairs[a]) return std::unexpected(ProfileError::BadFormat);
			for (size_t i = 0; i < styleKindCount; ++i) if (!parseUnsigned(f[3 + i], p.stats.pairs[a][i])) return std::unexpected(ProfileError::BadFormat);
			havePairs[a] = true;
		}
		else if (type == "target" && f.size() == 2 + naturalKindCount && f[1] == "shares" && !haveShares) {
			if (!p.target) p.target = StyleTarget {};
			for (size_t i = 0; i < naturalKindCount; ++i) if (!parseU32(f[2 + i], p.target->sharesPpm[i])) return std::unexpected(ProfileError::BadFormat);
			haveShares = true;
		}
		else if (type == "target" && f.size() == 3 + naturalKindCount && f[1] == "adjacency") {
			StyleKind row;
			if (!parseStyleKind(f[2], row) || static_cast<size_t>(row) >= naturalKindCount) return std::unexpected(ProfileError::UnknownKind);
			const size_t a = static_cast<size_t>(row);
			if (haveAdjacency[a]) return std::unexpected(ProfileError::BadFormat);
			if (!p.target) p.target = StyleTarget {};
			for (size_t i = 0; i < naturalKindCount; ++i) if (!parseU32(f[3 + i], p.target->adjacencyPpm[a][i])) return std::unexpected(ProfileError::BadFormat);
			haveAdjacency[a] = true;
		}
		else if (type == "tuning" && f.size() == 11 && !p.tuning) {
			uint32_t v[10];
			for (size_t i = 0; i < 10; ++i) if (!parseU32(f[1 + i], v[i])) return std::unexpected(ProfileError::BadFormat);
			p.tuning = SurfaceTuning {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]};
		}
		else return std::unexpected(ProfileError::BadFormat);
	}
	bool allAdjacency = true;
	for (const bool b : haveAdjacency) allAdjacency = allAdjacency && b;
	if (p.target && (!haveShares || !allAdjacency)) return std::unexpected(ProfileError::BadFormat);
	bool allPairs = true;
	for (const bool b : havePairs) allPairs = allPairs && b;
	if (!header || !haveClient || !haveLevel || !haveFloors || !haveOtb || !haveDat || !haveTiles || !allPairs) return std::unexpected(ProfileError::BadFormat);
	if (auto v = validateProfile(p); !v) return std::unexpected(v.error());
	return p;
}

std::string serializeStats(const StyleStats& stats) {
	std::string out(statsSchema);
	out += '\n';
	appendStatLines(out, stats);
	return out;
}

std::expected<StyleStats, ProfileError> parseStats(std::string_view text) {
	StyleStats stats;
	bool header = false, haveTiles = false;
	std::array<bool, styleKindCount> havePairs {};
	while (!text.empty()) {
		const size_t end = text.find('\n');
		std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		if (line.empty()) continue;
		if (!header) {
			if (line == statsSchema) { header = true; continue; }
			return std::unexpected(line.starts_with("g4-kindstats/") ? ProfileError::UnsupportedSchema : ProfileError::BadFormat);
		}
		const auto f = splitTabs(line, 12);
		if (f.size() == 2 + styleKindCount && f[0] == "stat" && f[1] == "kind_tiles" && !haveTiles) {
			for (size_t i = 0; i < styleKindCount; ++i) if (!parseUnsigned(f[2 + i], stats.kindTiles[i])) return std::unexpected(ProfileError::BadFormat);
			haveTiles = true;
		} else if (f.size() == 3 + styleKindCount && f[0] == "stat" && f[1] == "pairs") {
			StyleKind row;
			if (!parseStyleKind(f[2], row)) return std::unexpected(ProfileError::UnknownKind);
			const size_t a = static_cast<size_t>(row);
			if (havePairs[a]) return std::unexpected(ProfileError::BadFormat);
			for (size_t i = 0; i < styleKindCount; ++i) if (!parseUnsigned(f[3 + i], stats.pairs[a][i])) return std::unexpected(ProfileError::BadFormat);
			havePairs[a] = true;
		} else return std::unexpected(ProfileError::BadFormat);
	}
	bool all = header && haveTiles;
	for (const bool b : havePairs) all = all && b;
	if (!all) return std::unexpected(ProfileError::BadFormat);
	return stats;
}

std::optional<StyleTarget> targetFromStats(const StyleStats& stats) noexcept {
	uint64_t tiles = 0, pairTotal = 0;
	for (size_t a = 0; a < naturalKindCount; ++a) {
		tiles += stats.kindTiles[a];
		for (size_t b = 0; b < naturalKindCount; ++b) pairTotal += stats.pairs[a][b];
	}
	if (tiles == 0 || pairTotal == 0) return std::nullopt;
	StyleTarget t;
	for (size_t a = 0; a < naturalKindCount; ++a) {
		t.sharesPpm[a] = static_cast<uint32_t>(stats.kindTiles[a] * 1'000'000 / tiles);
		for (size_t b = 0; b < naturalKindCount; ++b) {
			// A pair is stored once as (a, b) in one direction; the symmetric matrix counts it in both cells.
			const uint64_t cell = a == b ? stats.pairs[a][a] * 2 : stats.pairs[a][b] + stats.pairs[b][a];
			t.adjacencyPpm[a][b] = static_cast<uint32_t>(cell * 1'000'000 / (pairTotal * 2));
		}
	}
	return t;
}

std::expected<TargetBuild, TargetError> buildTarget(std::span<const MapStats> maps, uint64_t minNaturalTiles) {
	try {
		struct Usable { size_t index; StyleTarget target; };
		std::vector<Usable> usable;
		TargetBuild build;
		std::map<std::string, std::vector<size_t>> byFamily; // positions in `usable`
		for (size_t i = 0; i < maps.size(); ++i) {
			uint64_t natural = 0;
			for (size_t k = 0; k < naturalKindCount; ++k) natural += maps[i].stats.kindTiles[k];
			const auto target = natural >= minNaturalTiles ? targetFromStats(maps[i].stats) : std::nullopt;
			if (!target) { build.excluded.push_back(maps[i].alias); continue; }
			byFamily[maps[i].family].push_back(usable.size());
			usable.push_back({i, *target});
		}
		if (usable.empty()) return std::unexpected(TargetError::NothingUsable);
		build.familyCount = static_cast<uint32_t>(byFamily.size());
		std::vector<uint64_t> weight(usable.size()); // ppm, floor of 1e6 / (families * maps in family)
		for (const auto& [family, members] : byFamily) {
			for (const size_t m : members) weight[m] = 1'000'000 / (byFamily.size() * members.size());
		}
		std::array<uint64_t, naturalKindCount> shares {};
		std::array<std::array<uint64_t, naturalKindCount>, naturalKindCount> adjacency {};
		uint64_t weightSum = 0;
		for (size_t m = 0; m < usable.size(); ++m) {
			weightSum += weight[m];
			for (size_t a = 0; a < naturalKindCount; ++a) {
				shares[a] += weight[m] * usable[m].target.sharesPpm[a];
				for (size_t b = 0; b < naturalKindCount; ++b) adjacency[a][b] += weight[m] * usable[m].target.adjacencyPpm[a][b];
			}
		}
		for (size_t a = 0; a < naturalKindCount; ++a) {
			build.target.sharesPpm[a] = static_cast<uint32_t>(shares[a] / weightSum);
			for (size_t b = 0; b < naturalKindCount; ++b) build.target.adjacencyPpm[a][b] = static_cast<uint32_t>(adjacency[a][b] / weightSum);
		}
		for (size_t m = 0; m < usable.size(); ++m) {
			build.used.push_back({maps[usable[m].index].alias, static_cast<uint32_t>(weight[m] * 1000 / weightSum)});
		}
		return build;
	} catch (const std::bad_alloc&) {
		return std::unexpected(TargetError::AllocationFailed);
	}
}

ProfileCompatibility checkProfile(const StyleProfile& p, std::string_view client, std::string_view otb, std::string_view dat, int floorZ) noexcept {
	if (p.client != client) return ProfileCompatibility::WrongClient;
	if (p.otbSha256 != otb || p.datSha256 != dat) return ProfileCompatibility::WrongAssets;
	if (floorZ < p.zMin || floorZ > p.zMax) return ProfileCompatibility::WrongLevel;
	return ProfileCompatibility::Compatible;
}

std::expected<StyleStats, StatsError> accumulateKindStats(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop) {
	if (zMin > zMax) return std::unexpected(StatsError::InvalidRange);
	try {
		std::unordered_map<std::string_view, StyleKind> byKey;
		for (const auto& e : keymap) byKey.emplace(e.key, e.kind);
		const auto& names = map.groundKeys();
		std::vector<uint8_t> kindOf(names.size());
		for (size_t i = 0; i < names.size(); ++i) {
			const auto it = byKey.find(names[i]);
			if (it != byKey.end()) kindOf[i] = static_cast<uint8_t>(it->second);
			else if (i == noGroundKey) kindOf[i] = static_cast<uint8_t>(StyleKind::NoGround);
			else return std::unexpected(StatsError::KeyNotMapped);
		}
		StyleStats stats;
		for (const SemanticFloor& floor : map.floors()) {
			if (floor.bounds.z < zMin || floor.bounds.z > zMax) continue;
			const size_t w = static_cast<size_t>(floor.bounds.width), h = static_cast<size_t>(floor.bounds.height);
			auto present = [&](size_t i) { return (floor.traits[i] & bit(Trait::Present)) != 0; };
			for (size_t y = 0; y < h; ++y) {
				if (stop.stop_requested()) return std::unexpected(StatsError::Cancelled);
				for (size_t x = 0; x < w; ++x) {
					const size_t i = y * w + x;
					if (!present(i)) continue;
					const uint8_t a = kindOf[floor.ground[i]];
					++stats.kindTiles[a];
					if (x + 1 < w && present(i + 1)) ++stats.pairs[a][kindOf[floor.ground[i + 1]]];
					if (y + 1 < h && present(i + w)) ++stats.pairs[a][kindOf[floor.ground[i + w]]];
				}
			}
		}
		return stats;
	} catch (const std::bad_alloc&) {
		return std::unexpected(StatsError::AllocationFailed);
	}
}

} // namespace rme::procedural
