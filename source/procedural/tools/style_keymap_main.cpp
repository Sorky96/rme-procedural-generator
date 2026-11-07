// SPDX-License-Identifier: GPL-3.0-or-later
// Offline tool: proposes the ground-key -> StyleKind map from the G3 reports of a reference set.
// Reads the manifest and each map's analysis/ground_keys.csv; writes keymap.tsv (g4-keymap/1) and a
// Markdown review table. Only names and tile counts are read; nothing from the maps themselves.
//
// Usage: style_keymap --manifest FILE --reports DIR --out-dir DIR
// DIR holds <alias>/run/analysis/ground_keys.csv for every manifest entry.

#include "procedural/core/reference_split.h"
#include "procedural/core/style_profile.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>

using namespace rme::procedural;
namespace fs = std::filesystem;

namespace {

bool readFile(const fs::path& path, std::string& out) {
	std::ifstream in(path, std::ios::binary);
	if (!in) return false;
	out.assign(std::istreambuf_iterator<char>(in), {});
	return true;
}

// One CSV record: fields separated by commas, optionally double-quoted with "" as an escaped quote.
std::vector<std::string> csvFields(std::string_view line) {
	std::vector<std::string> fields(1);
	bool quoted = false;
	for (size_t i = 0; i < line.size(); ++i) {
		const char c = line[i];
		if (quoted) {
			if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') { fields.back() += '"'; ++i; }
			else if (c == '"') quoted = false;
			else fields.back() += c;
		} else if (c == '"') quoted = true;
		else if (c == ',') fields.emplace_back();
		else fields.back() += c;
	}
	return fields;
}

struct KeyUse {
	uint64_t tiles = 0;
	std::set<std::string> maps;
};

} // namespace

int main(int argc, char** argv) {
	fs::path manifestPath, reportsDir, outDir;
	for (int i = 1; i + 1 < argc; i += 2) {
		const std::string key = argv[i];
		if (key == "--manifest") manifestPath = argv[i + 1];
		else if (key == "--reports") reportsDir = argv[i + 1];
		else if (key == "--out-dir") outDir = argv[i + 1];
		else { std::cerr << "unknown option " << key << '\n'; return 2; }
	}
	if (manifestPath.empty() || reportsDir.empty() || outDir.empty()) {
		std::cerr << "usage: style_keymap --manifest FILE --reports DIR --out-dir DIR\n";
		return 2;
	}
	std::string text;
	if (!readFile(manifestPath, text)) { std::cerr << "cannot read manifest\n"; return 2; }
	const auto manifest = parseManifest(text);
	if (!manifest) { std::cerr << "invalid manifest\n"; return 3; }

	std::map<std::string, KeyUse> use;           // all maps, to propose a complete key map
	std::map<std::string, KeyUse> useStyle;      // style maps only, for the review
	for (const auto& entry : manifest->entries) {
		std::string csv;
		if (!readFile(reportsDir / entry.alias / "run" / "analysis" / "ground_keys.csv", csv)) {
			std::cerr << "missing ground_keys.csv for " << entry.alias << '\n';
			return 3;
		}
		std::string_view rest = csv;
		bool first = true;
		while (!rest.empty()) {
			const size_t end = rest.find('\n');
			std::string_view line = rest.substr(0, end);
			rest = end == std::string_view::npos ? std::string_view {} : rest.substr(end + 1);
			if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
			if (first) { first = false; continue; }
			if (line.empty()) continue;
			const auto f = csvFields(line);
			if (f.size() < 3) { std::cerr << "bad csv line in " << entry.alias << '\n'; return 3; }
			const uint64_t tiles = std::strtoull(f[2].c_str(), nullptr, 10);
			use[f[1]].tiles += tiles;
			use[f[1]].maps.insert(entry.alias);
			if (entry.role == ReferenceRole::Style) {
				useStyle[f[1]].tiles += tiles;
				useStyle[f[1]].maps.insert(entry.alias);
			}
		}
	}

	std::vector<std::string> keys;
	for (const auto& [key, u] : use) keys.push_back(key); // std::map order: sorted by name
	const auto keymap = proposeKeyMap(keys, defaultKeyRules());
	const auto serialized = serializeKeyMap(keymap);
	if (!serialized) { std::cerr << "cannot serialize key map\n"; return 4; }
	fs::create_directories(outDir);
	{ std::ofstream f(outDir / "keymap.tsv", std::ios::binary); f << *serialized; }

	std::array<uint64_t, styleKindCount> tilesPerKind {};
	std::array<uint32_t, styleKindCount> keysPerKind {};
	uint64_t styleTotal = 0;
	for (const auto& e : keymap) {
		const auto it = useStyle.find(e.key);
		const uint64_t t = it == useStyle.end() ? 0 : it->second.tiles;
		tilesPerKind[static_cast<size_t>(e.kind)] += t;
		++keysPerKind[static_cast<size_t>(e.kind)];
		styleTotal += t;
	}
	std::string md = "# G4-B key map proposal (review)\n\nTiles are counted over the style maps only, all floors. Share = permille of those tiles.\n\n| Kind | Keys | Tiles | Share |\n| --- | ---: | ---: | ---: |\n";
	for (size_t k = 0; k < styleKindCount; ++k) {
		md += std::format("| {} | {} | {} | {} |\n", styleKindName(static_cast<StyleKind>(k)), keysPerKind[k], tilesPerKind[k], styleTotal ? tilesPerKind[k] * 1000 / styleTotal : 0);
	}
	md += "\n| Key | Kind | Rule | Tiles (style maps) | Style maps |\n| --- | --- | --- | ---: | ---: |\n";
	std::vector<const KeyMapEntry*> order;
	for (const auto& e : keymap) order.push_back(&e);
	std::stable_sort(order.begin(), order.end(), [&](const KeyMapEntry* a, const KeyMapEntry* b) {
		auto tiles = [&](const KeyMapEntry* e) { const auto it = useStyle.find(e->key); return it == useStyle.end() ? uint64_t {0} : it->second.tiles; };
		return tiles(a) > tiles(b);
	});
	for (const KeyMapEntry* e : order) {
		const auto it = useStyle.find(e->key);
		md += std::format("| `{}` | {} | {} | {} | {} |\n", e->key.empty() ? "(no ground)" : e->key, styleKindName(e->kind),
			e->rule.empty() ? "-" : e->rule, it == useStyle.end() ? 0 : it->second.tiles, it == useStyle.end() ? 0 : it->second.maps.size());
	}
	{ std::ofstream f(outDir / "keymap-review.md", std::ios::binary); f << md; }
	return 0;
}
