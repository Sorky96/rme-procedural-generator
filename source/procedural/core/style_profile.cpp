// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_profile.h"

#include "procedural/core/doodad_class.h"

#include <algorithm>
#include <charconv>
#include <format>
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
	for (const auto& name : p.palette) if (bad(name)) return std::unexpected(ProfileError::InvalidKey);
	for (const auto& name : p.decorPalette) if (bad(name)) return std::unexpected(ProfileError::InvalidKey);
	for (const auto* group : {&p.grassMix, &p.treeMix, &p.bushMix}) {
		uint32_t share = 0;
		for (const StyleMix& mix : *group) {
			if (bad(mix.brush)) return std::unexpected(ProfileError::InvalidKey);
			if (mix.brush.empty() != (mix.permille == 0) || mix.permille > 1000) return std::unexpected(ProfileError::InvalidRange);
			share += mix.permille;
		}
		if (share > 1000) return std::unexpected(ProfileError::InvalidRange);
	}
	// The brush of the trees standing in the water is set exactly when they are on.
	if (bad(p.shoreTree) || p.shoreTree.empty() != (!p.decorTuning || !p.decorTuning->shoreTreePermille)) return std::unexpected(ProfileError::InvalidRange);
	if (const auto& b = p.buildings; bad(b.floor) || bad(b.wall) || b.density > 100 || b.ruinPercent > 100 || !b.floorPercent || b.floorPercent > 100 || b.compoundPercent > 100 ||
		b.floor.empty() != (b.density == 0) || (!b.density && (b.ruinPercent || !b.wall.empty()))) return std::unexpected(ProfileError::InvalidRange);
	for (const StyleOverlay& o : p.interiors) {
		if (bad(o.name)) return std::unexpected(ProfileError::InvalidKey);
		if (o.onTreePpm || o.onGroundPpm > 1000000 || o.name.empty() != (o.onGroundPpm == 0)) return std::unexpected(ProfileError::InvalidRange);
	}
	if (p.rockOutlinePermille > 1000) return std::unexpected(ProfileError::InvalidRange);
	if (bad(p.carpet.name) || p.carpet.onTreePpm || p.carpet.onGroundPpm > 1000000 || p.carpet.name.empty() != (p.carpet.onGroundPpm == 0) ||
		(!p.carpet.name.empty() && !p.buildings.density)) return std::unexpected(ProfileError::InvalidRange);
	for (const StyleOverlay& o : p.wallDecor) {
		if (bad(o.name)) return std::unexpected(ProfileError::InvalidKey);
		if (o.onTreePpm > 1000000 || o.onGroundPpm > 1000000 || o.name.empty() != (o.onTreePpm + o.onGroundPpm == 0) || (!o.name.empty() && !p.buildings.density))
			return std::unexpected(ProfileError::InvalidRange);
	}
	for (const StyleOverlay& o : p.overlays) {
		if (bad(o.name)) return std::unexpected(ProfileError::InvalidKey);
		if (o.onTreePpm > 1000000 || o.onGroundPpm > 1000000 || o.name.empty() != (o.onTreePpm + o.onGroundPpm == 0))
			return std::unexpected(ProfileError::InvalidRange);
	}
	for (size_t m = 0; m < maxGrassMix; ++m) {
		const auto& d = p.grassMixDecor[m];
		if (p.grassMix[m].brush.empty() && d != StyleMixDecor {}) return std::unexpected(ProfileError::InvalidRange);
		if (p.grassPatchStep > 64) return std::unexpected(ProfileError::InvalidRange);
		for (size_t k = 0; k < 3; ++k) {
			if (d.keepPermille[k] > 1000) return std::unexpected(ProfileError::InvalidRange);
			if (bad(d.brushes[k])) return std::unexpected(ProfileError::InvalidKey);
		}
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
	for (size_t k = 0; k < naturalKindCount; ++k) {
		if (!p.palette[k].empty()) out += std::string("palette\t") + kindNames[k] + '\t' + p.palette[k] + '\n';
	}
	static constexpr const char* decorNames[3] = {"Tree", "Bush", "Stone"};
	for (size_t k = 0; k < 3; ++k) {
		if (!p.decorPalette[k].empty()) out += std::string("doodad\t") + decorNames[k] + '\t' + p.decorPalette[k] + '\n';
	}
	for (const auto& mix : p.grassMix) if (!mix.brush.empty()) out += std::format("mix\tGrass\t{}\t{}\n", mix.permille, mix.brush);
	if (p.grassPatchStep) out += std::format("mixpatch\t{}\n", p.grassPatchStep);
	for (size_t m = 0; m < maxGrassMix; ++m) {
		const auto& d = p.grassMixDecor[m];
		if (d == StyleMixDecor {}) continue;
		const auto name = [](const std::string& b) { return b.empty() ? std::string("-") : b; };
		out += std::format("mixdecor\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n", m, d.keepPermille[0], d.keepPermille[1], d.keepPermille[2],
			name(d.brushes[0]), name(d.brushes[1]), name(d.brushes[2]));
	}
	for (const auto& mix : p.treeMix) if (!mix.brush.empty()) out += std::format("mix\tTree\t{}\t{}\n", mix.permille, mix.brush);
	for (const auto& mix : p.bushMix) if (!mix.brush.empty()) out += std::format("mix\tBush\t{}\t{}\n", mix.permille, mix.brush);
	if (const auto& b = p.buildings; b.density)
		out += std::format("buildings\t{}\t{}\t{}\t{}\t{}\t{}\n", b.density, b.ruinPercent, b.floorPercent, b.compoundPercent, b.floor,
			b.wall.empty() ? std::string("-") : b.wall);
	for (const auto& o : p.interiors) if (!o.name.empty()) out += std::format("interior\t{}\t{}\n", o.onGroundPpm, o.name);
	for (const auto& o : p.wallDecor) if (!o.name.empty()) out += std::format("walldecor\t{}\t{}\t{}\n", o.onTreePpm, o.onGroundPpm, o.name);
	if (!p.carpet.name.empty()) out += std::format("carpet\t{}\t{}\n", p.carpet.onGroundPpm, p.carpet.name);
	if (p.rockOutlinePermille) out += std::format("rockoutline\t{}\n", p.rockOutlinePermille);
	for (const auto& o : p.overlays) if (!o.name.empty()) out += std::format("overlay\t{}\t{}\t{}\n", o.onTreePpm, o.onGroundPpm, o.name);
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
	if (p.decorTarget) {
		for (size_t i = 0; i < p.decorTarget->cell.size(); ++i) {
			const DecorCell& d = p.decorTarget->cell[i];
			out += "decor\tcell\t" + std::to_string(i) + '\t' + std::to_string(d.tiles) + '\t' + std::to_string(d.trees) + '\t' +
				std::to_string(d.bushes) + '\t' + std::to_string(d.stones) + '\n';
		}
	}
	if (p.decorTuning) {
		const DecorTuning& d = *p.decorTuning;
		out += "decor\ttuning";
		for (const uint32_t v : {d.treeScalePermille, d.waterTreePermille, d.bushPermille, d.waterBushPermille, d.dirtStonePermille, d.rockStonePermille}) {
			out += '\t' + std::to_string(v);
		}
		out += '\n';
		// Its own line, after the tuning and only when set, so profiles without clumps keep their bytes.
		if (d.clumpPermille) out += std::format("decor\tclump\t{}\n", d.clumpPermille);
		if (d.shoreTreePermille) out += std::format("decor\tshoretree\t{}\t{}\n", d.shoreTreePermille, p.shoreTree);
	}
	if (p.tuning) {
		const SurfaceTuning& t = *p.tuning;
		out += "tuning";
		for (const uint32_t v : {t.featureSize, t.lakePermille, t.minLakeCells, t.riverCount, t.minRiverLength, t.shoreWidth,
			t.rockPermille, t.minRockCells, t.dirtPermille, t.transitionWidth, t.meander}) out += '\t' + std::to_string(v);
		out += '\n';
		// Its own line, only when set, so profiles without a sea keep their bytes.
		if (t.islandPermille) out += std::format("tuning\tisland\t{}\n", t.islandPermille);
		if (t.dirtSize) out += std::format("tuning\tdirtsize\t{}\n", t.dirtSize);
		if (t.cleanEdges) out += "tuning\tcleanedges\t1\n";
	}
	return out;
}

std::expected<StyleProfile, ProfileError> parseProfile(std::string_view text) {
	StyleProfile p;
	bool header = false, haveClient = false, haveLevel = false, haveFloors = false, haveOtb = false, haveDat = false, haveTiles = false;
	std::array<bool, styleKindCount> havePairs {};
	std::array<bool, naturalKindCount> haveAdjacency {};
	std::array<bool, 8> haveDecorCell {};
	bool haveShares = false;
	std::array<bool, maxGrassMix> haveMixDecor {};
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
        // Legacy edge overrides are ignored: grass reaches the cap edge.
        if (type == "plateauEdge" && f.size() == 2) {
            continue;
        }
        if (type == "client" && f.size() == 2 && !haveClient) {
            p.client = f[1];
            haveClient = true;
        } else if (type == "level" && f.size() == 2 && !haveLevel) {
            if (f[1] == "surface") {
                p.level = StyleLevel::Surface;
            } else if (f[1] == "underground")
                p.level = StyleLevel::Underground;
            else return std::unexpected(ProfileError::BadFormat);
			haveLevel = true;
        } else if (type == "floors" && f.size() == 3 && !haveFloors) {
            if (!parseInt(f[1], p.zMin) || !parseInt(f[2], p.zMax)) {
                return std::unexpected(ProfileError::BadFormat);
            }
            haveFloors = true;
        } else if (type == "assets" && f.size() == 3 && f[1] == "otb_sha256" && !haveOtb) {
            p.otbSha256 = f[2];
            haveOtb = true;
        } else if (type == "assets" && f.size() == 3 && f[1] == "dat_sha256" && !haveDat) {
            p.datSha256 = f[2];
            haveDat = true;
        } else if (type == "source" && f.size() == 3) {
            uint64_t w = 0;
            if (!parseUnsigned(f[2], w) || w > 1000) return std::unexpected(ProfileError::BadFormat);
			p.sources.push_back({std::string(f[1]), static_cast<uint32_t>(w)});
        } else if (type == "key" && f.size() == 4) {
            KeyMapEntry e;
            if (!parseStyleKind(f[1], e.kind)) return std::unexpected(ProfileError::UnknownKind);
			e.rule = f[2] == "-" ? std::string {} : std::string(f[2]);
			e.key = f[3];
			p.keymap.push_back(std::move(e));
        } else if (type == "stat" && f.size() == 2 + styleKindCount && f[1] == "kind_tiles" && !haveTiles) {
            for (size_t i = 0; i < styleKindCount; ++i) {
                if (!parseUnsigned(f[2 + i], p.stats.kindTiles[i])) {
                    return std::unexpected(ProfileError::BadFormat);
                }
            }
            haveTiles = true;
        } else if (type == "stat" && f.size() == 3 + styleKindCount && f[1] == "pairs") {
            StyleKind row;
            if (!parseStyleKind(f[2], row)) return std::unexpected(ProfileError::UnknownKind);
			const size_t a = static_cast<size_t>(row);
			if (havePairs[a]) return std::unexpected(ProfileError::BadFormat);
			for (size_t i = 0; i < styleKindCount; ++i) if (!parseUnsigned(f[3 + i], p.stats.pairs[a][i])) return std::unexpected(ProfileError::BadFormat);
			havePairs[a] = true;
        } else if (type == "palette" && f.size() == 3) {
            StyleKind kind;
            if (!parseStyleKind(f[1], kind) || static_cast<size_t>(kind) >= naturalKindCount) return std::unexpected(ProfileError::UnknownKind);
			std::string& slot = p.palette[static_cast<size_t>(kind)];
			if (!slot.empty() || f[2].empty()) return std::unexpected(ProfileError::BadFormat);
			slot = f[2];
        } else if (type == "doodad" && f.size() == 3) {
            static constexpr std::string_view names[3] = {"Tree", "Bush", "Stone"};
            size_t k = 3;
			for (size_t i = 0; i < 3; ++i) if (f[1] == names[i]) k = i;
			if (k == 3) return std::unexpected(ProfileError::UnknownKind);
			if (!p.decorPalette[k].empty() || f[2].empty()) return std::unexpected(ProfileError::BadFormat);
			p.decorPalette[k] = f[2];
        } else if (type == "mix" && f.size() == 4 && (f[1] == "Grass" || f[1] == "Tree" || f[1] == "Bush")) {
            StyleMix mix;
            if (!parseU32(f[2], mix.permille) || f[3].empty()) return std::unexpected(ProfileError::BadFormat);
            mix.brush = f[3];
            auto& slots = f[1] == "Grass" ? p.grassMix : f[1] == "Tree" ? p.treeMix : p.bushMix;
            const auto slot = std::ranges::find_if(slots, [](const StyleMix& m) { return m.brush.empty(); });
            if (slot == slots.end()) return std::unexpected(ProfileError::BadFormat);
            *slot = std::move(mix);
        } else if (type == "buildings" && f.size() == 7 && !p.buildings.density) {
            if (!parseU32(f[1], p.buildings.density) || !parseU32(f[2], p.buildings.ruinPercent) || !parseU32(f[3], p.buildings.floorPercent) ||
                !parseU32(f[4], p.buildings.compoundPercent) || f[5].empty() || f[6].empty())
                return std::unexpected(ProfileError::BadFormat);
            p.buildings.floor = f[5];
            p.buildings.wall = f[6] == "-" ? std::string() : std::string(f[6]);
            if (!p.buildings.density) return std::unexpected(ProfileError::BadFormat);
        } else if (type == "interior" && f.size() == 3) {
            StyleOverlay interior;
            if (!parseU32(f[1], interior.onGroundPpm) || f[2].empty()) return std::unexpected(ProfileError::BadFormat);
            interior.name = f[2];
            const auto slot = std::ranges::find_if(p.interiors, [](const StyleOverlay& o) { return o.name.empty(); });
            if (slot == p.interiors.end()) return std::unexpected(ProfileError::BadFormat);
            *slot = std::move(interior);
        } else if (type == "rockoutline" && f.size() == 2) {
            if (p.rockOutlinePermille || !parseU32(f[1], p.rockOutlinePermille) || !p.rockOutlinePermille) return std::unexpected(ProfileError::BadFormat);
        } else if (type == "carpet" && f.size() == 3) {
            if (!p.carpet.name.empty() || !parseU32(f[1], p.carpet.onGroundPpm) || f[2].empty()) return std::unexpected(ProfileError::BadFormat);
            p.carpet.name = f[2];
        } else if (type == "walldecor" && f.size() == 4) {
            StyleOverlay decor;
            if (!parseU32(f[1], decor.onTreePpm) || !parseU32(f[2], decor.onGroundPpm) || f[3].empty()) return std::unexpected(ProfileError::BadFormat);
            decor.name = f[3];
            const auto slot = std::ranges::find_if(p.wallDecor, [](const StyleOverlay& o) { return o.name.empty(); });
            if (slot == p.wallDecor.end()) return std::unexpected(ProfileError::BadFormat);
            *slot = std::move(decor);
        } else if (type == "overlay" && f.size() == 4) {
            StyleOverlay overlay;
            if (!parseU32(f[1], overlay.onTreePpm) || !parseU32(f[2], overlay.onGroundPpm) || f[3].empty())
                return std::unexpected(ProfileError::BadFormat);
            overlay.name = f[3];
            const auto slot = std::ranges::find_if(p.overlays, [](const StyleOverlay& o) { return o.name.empty(); });
            if (slot == p.overlays.end()) return std::unexpected(ProfileError::BadFormat);
            *slot = std::move(overlay);
        } else if (type == "mixpatch" && f.size() == 2 && !p.grassPatchStep) {
            if (!parseU32(f[1], p.grassPatchStep) || p.grassPatchStep < 1 || p.grassPatchStep > 64) return std::unexpected(ProfileError::BadFormat);
        } else if (type == "mixdecor" && f.size() == 8) {
            uint32_t slot = 0;
            if (!parseU32(f[1], slot) || slot >= maxGrassMix || haveMixDecor[slot]) return std::unexpected(ProfileError::BadFormat);
            auto& d = p.grassMixDecor[slot];
            for (size_t k = 0; k < 3; ++k) {
                if (!parseU32(f[2 + k], d.keepPermille[k]) || f[5 + k].empty()) return std::unexpected(ProfileError::BadFormat);
                d.brushes[k] = f[5 + k] == "-" ? std::string() : std::string(f[5 + k]);
            }
            haveMixDecor[slot] = true;
        } else if (type == "target" && f.size() == 2 + naturalKindCount && f[1] == "shares" && !haveShares) {
            if (!p.target) {
                p.target = StyleTarget {};
            }
            for (size_t i = 0; i < naturalKindCount; ++i) if (!parseU32(f[2 + i], p.target->sharesPpm[i])) return std::unexpected(ProfileError::BadFormat);
			haveShares = true;
        } else if (type == "target" && f.size() == 3 + naturalKindCount && f[1] == "adjacency") {
            StyleKind row;
            if (!parseStyleKind(f[2], row) || static_cast<size_t>(row) >= naturalKindCount) return std::unexpected(ProfileError::UnknownKind);
			const size_t a = static_cast<size_t>(row);
			if (haveAdjacency[a]) return std::unexpected(ProfileError::BadFormat);
			if (!p.target) p.target = StyleTarget {};
			for (size_t i = 0; i < naturalKindCount; ++i) if (!parseU32(f[3 + i], p.target->adjacencyPpm[a][i])) return std::unexpected(ProfileError::BadFormat);
			haveAdjacency[a] = true;
        } else if (type == "decor" && f.size() == 7 && f[1] == "cell") {
            uint64_t index = 0;
            if (!parseUnsigned(f[2], index) || index >= 8 || haveDecorCell[index]) return std::unexpected(ProfileError::BadFormat);
			if (!p.decorTarget) p.decorTarget = DecorContext {};
			DecorCell& d = p.decorTarget->cell[index];
			if (!parseUnsigned(f[3], d.tiles) || !parseUnsigned(f[4], d.trees) || !parseUnsigned(f[5], d.bushes) || !parseUnsigned(f[6], d.stones)) {
				return std::unexpected(ProfileError::BadFormat);
			}
			haveDecorCell[index] = true;
        } else if (type == "decor" && f.size() == 8 && f[1] == "tuning" && !p.decorTuning) {
            uint32_t v[6];
            for (size_t i = 0; i < 6; ++i) if (!parseU32(f[2 + i], v[i])) return std::unexpected(ProfileError::BadFormat);
			p.decorTuning = DecorTuning {v[0], v[1], v[2], v[3], v[4], v[5]};
        } else if (type == "decor" && f.size() == 4 && f[1] == "shoretree" && p.decorTuning && !p.decorTuning->shoreTreePermille) {
            if (!parseU32(f[2], p.decorTuning->shoreTreePermille) || !p.decorTuning->shoreTreePermille || p.decorTuning->shoreTreePermille > 1000 || f[3].empty())
                return std::unexpected(ProfileError::BadFormat);
            p.shoreTree = f[3];
        } else if (type == "decor" && f.size() == 3 && f[1] == "clump" && p.decorTuning && !p.decorTuning->clumpPermille) {
            if (!parseU32(f[2], p.decorTuning->clumpPermille) || !p.decorTuning->clumpPermille || p.decorTuning->clumpPermille > 1000)
                return std::unexpected(ProfileError::BadFormat);
        } else if (type == "tuning" && f.size() == 3 && f[1] == "cleanedges" && p.tuning && !p.tuning->cleanEdges) {
            if (f[2] != "1") return std::unexpected(ProfileError::BadFormat);
            p.tuning->cleanEdges = 1;
        } else if (type == "tuning" && f.size() == 3 && f[1] == "dirtsize" && p.tuning && !p.tuning->dirtSize) {
            if (!parseU32(f[2], p.tuning->dirtSize) || !p.tuning->dirtSize || p.tuning->dirtSize > 4096) return std::unexpected(ProfileError::BadFormat);
        } else if (type == "tuning" && f.size() == 3 && f[1] == "island" && p.tuning && !p.tuning->islandPermille) {
            if (!parseU32(f[2], p.tuning->islandPermille) || !p.tuning->islandPermille || p.tuning->islandPermille > 900)
                return std::unexpected(ProfileError::BadFormat);
        } else if (type == "tuning" && f.size() == 12 && !p.tuning) {
            uint32_t v[11];
            for (size_t i = 0; i < 11; ++i) if (!parseU32(f[1 + i], v[i])) return std::unexpected(ProfileError::BadFormat);
			SurfaceTuning tuning;
			tuning.featureSize = v[0]; tuning.lakePermille = v[1]; tuning.minLakeCells = v[2]; tuning.riverCount = v[3];
			tuning.minRiverLength = v[4]; tuning.shoreWidth = v[5]; tuning.rockPermille = v[6]; tuning.minRockCells = v[7];
			tuning.dirtPermille = v[8]; tuning.transitionWidth = v[9]; tuning.meander = v[10];
			p.tuning = tuning;
        } else {
            return std::unexpected(ProfileError::BadFormat);
        }
    }
    bool allAdjacency = true;
	for (const bool b : haveAdjacency) allAdjacency = allAdjacency && b;
	if (p.target && (!haveShares || !allAdjacency)) return std::unexpected(ProfileError::BadFormat);
	if (p.decorTarget && !std::all_of(haveDecorCell.begin(), haveDecorCell.end(), [](bool b) { return b; })) return std::unexpected(ProfileError::BadFormat);
	bool allPairs = true;
	for (const bool b : havePairs) allPairs = allPairs && b;
	if (!header || !haveClient || !haveLevel || !haveFloors || !haveOtb || !haveDat || !haveTiles || !allPairs) return std::unexpected(ProfileError::BadFormat);
	if (auto v = validateProfile(p); !v) return std::unexpected(v.error());
	return p;
}

StylePalette choosePalette(std::span<const KeyMapEntry> keymap, std::span<const KeyTiles> tiles, uint64_t minTiles, uint32_t minSharePercent,
	const std::set<std::string>* usable) {
	std::unordered_map<std::string_view, StyleKind> kindOf;
	for (const auto& e : keymap) kindOf.emplace(e.key, e.kind);
	std::array<uint64_t, naturalKindCount> total {};
	std::array<const KeyTiles*, naturalKindCount> best {};
	for (const KeyTiles& t : tiles) {
		const auto it = kindOf.find(t.key);
		if (it == kindOf.end() || static_cast<size_t>(it->second) >= naturalKindCount) continue;
		const size_t k = static_cast<size_t>(it->second);
		total[k] += t.tiles;
		if (t.key.starts_with("item ") || (usable && !usable->contains(t.key))) continue;
		// Most tiles wins; the smaller name on a tie, so the choice does not depend on the order of `tiles`.
		if (!best[k] || t.tiles > best[k]->tiles || (t.tiles == best[k]->tiles && t.key < best[k]->key)) best[k] = &t;
	}
	StylePalette palette;
	for (size_t k = 0; k < naturalKindCount; ++k) {
		if (best[k] && best[k]->tiles >= minTiles && best[k]->tiles * 100 >= total[k] * minSharePercent) palette[k] = best[k]->key;
	}
	return palette;
}

StyleDecorPalette chooseDecorPalette(std::span<const KeyTiles> items, uint64_t minItems, uint32_t minSharePercent, const std::set<std::string>* usable) {
	std::array<uint64_t, 3> total {};
	std::array<const KeyTiles*, 3> best {};
	for (const KeyTiles& t : items) {
		const DecorKind kind = classifyDoodadBrush(t.key);
		if (kind == DecorKind::None) continue;
		const size_t k = static_cast<size_t>(kind) - 1;
		total[k] += t.tiles;
		if (usable && !usable->contains(t.key)) continue;
		if (!best[k] || t.tiles > best[k]->tiles || (t.tiles == best[k]->tiles && t.key < best[k]->key)) best[k] = &t;
	}
	StyleDecorPalette palette;
	for (size_t k = 0; k < 3; ++k) {
		if (best[k] && best[k]->tiles >= minItems && best[k]->tiles * 100 >= total[k] * minSharePercent) palette[k] = best[k]->key;
	}
	return palette;
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
