// SPDX-License-Identifier: GPL-3.0-or-later
// Offline tool (G4-F iteration 3): one profile per style (family of maps), fitted and evaluated exactly as fixed in
// docs/procedural/G4_METRICS.md, section "Iteracja 3", before this tool was run.
//   A  each style profile against the manual presets on the style's own target (in-sample)
//   B  leave-one-map-out inside styles that have at least two maps with a target
//   C  which style target each style profile is closest to (distinguishability)
//
// Usage: style_styles --manifest FILE --fingerprints DIR --stats DIR --out FILE
//                     [--profiles-out DIR --keymap FILE --client ID --otb-sha HEX --dat-sha HEX [--usable-brushes FILE] [--usable-doodads FILE] [--meander N]]
// With --profiles-out each style profile is also written as <style>.profile (g4-profile/4) for the generator dialog.

#include "procedural/core/decor_context.h"
#include "procedural/core/reference_split.h"
#include "procedural/core/style_fit.h"
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

// Fixed by G4_METRICS.md.
constexpr uint32_t fitSide = 160, evalSide = 256;
constexpr uint64_t firstSeed = 1001;
constexpr size_t seedCount = 32;
constexpr uint64_t minNaturalTiles = 1000;
constexpr uint32_t minConnectivityPercent = 97;

bool readFile(const fs::path& path, std::string& out) {
	std::ifstream in(path, std::ios::binary);
	if (!in) return false;
	out.assign(std::istreambuf_iterator<char>(in), {});
	return true;
}

// One CSV record: comma separated, optionally double-quoted with "" as an escaped quote.
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

std::string quote(const std::string& s) { return '"' + s + '"'; }

struct Generated {
	std::optional<StyleTarget> target; // nullopt: no natural tile or pair
	DecorContext decor;
	std::optional<uint32_t> p1;
};

struct StyleProfileFit {
	SurfaceTuning surface;
	DecorTuning decor;
};

struct Summary {
	int64_t median = 0, p10 = 0, p90 = 0;
	size_t better = 0, worse = 0, n = 0;
	enum class Verdict { Better, Worse, Undecided, NotApplicable } verdict = Verdict::NotApplicable;
};

const char* verdictName(Summary::Verdict v) {
	switch (v) {
		case Summary::Verdict::Better: return "better";
		case Summary::Verdict::Worse: return "worse";
		case Summary::Verdict::Undecided: return "undecided";
		case Summary::Verdict::NotApplicable: break;
	}
	return "n/a";
}

int64_t medianOf(std::vector<int64_t> v) {
	if (v.empty()) return 0;
	std::sort(v.begin(), v.end());
	const size_t n = v.size();
	return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}

Summary summarize(std::vector<int64_t> diffs) {
	Summary s;
	s.n = diffs.size();
	if (diffs.empty()) return s;
	for (const int64_t d : diffs) { s.better += d < 0; s.worse += d > 0; }
	std::sort(diffs.begin(), diffs.end());
	const size_t n = diffs.size();
	s.median = n % 2 ? diffs[n / 2] : (diffs[n / 2 - 1] + diffs[n / 2]) / 2;
	s.p10 = diffs[(n - 1) * 10 / 100];
	s.p90 = diffs[(n - 1) * 90 / 100];
	const size_t needed = (n * 3 + 3) / 4;
	if (s.median < 0 && s.better >= needed) s.verdict = Summary::Verdict::Better;
	else if (s.median > 0 && s.worse >= needed) s.verdict = Summary::Verdict::Worse;
	else s.verdict = Summary::Verdict::Undecided;
	return s;
}

std::string summaryJson(const Summary& s) {
	return std::format("{{\"seeds\": {}, \"median\": {}, \"p10\": {}, \"p90\": {}, \"candidate_better\": {}, \"candidate_worse\": {}, \"verdict\": {}}}",
		s.n, s.median, s.p10, s.p90, s.better, s.worse, quote(verdictName(s.verdict)));
}

std::string tuningJson(const SurfaceTuning& t, const DecorTuning& d) {
	return std::format("{{\"featureSize\": {}, \"lakePermille\": {}, \"riverCount\": {}, \"shoreWidth\": {}, \"rockPermille\": {}, \"dirtPermille\": {}, \"transitionWidth\": {}, \"decor\": [{}, {}, {}, {}, {}, {}]}}",
		t.featureSize, t.lakePermille, t.riverCount, t.shoreWidth, t.rockPermille, t.dirtPermille, t.transitionWidth,
		d.treeScalePermille, d.waterTreePermille, d.bushPermille, d.waterBushPermille, d.dirtStonePermille, d.rockStonePermille);
}

} // namespace

int main(int argc, char** argv) {
	fs::path manifestPath, fingerprintDir, statsDir, outPath, profilesOut, keymapPath, usableBrushesPath, usableDoodadsPath;
	std::string client, otbSha, datSha;
	uint32_t meander = 0;
	for (int i = 1; i + 1 < argc; i += 2) {
		const std::string key = argv[i];
		const char* value = argv[i + 1];
		if (key == "--manifest") manifestPath = value;
		else if (key == "--fingerprints") fingerprintDir = value;
		else if (key == "--stats") statsDir = value;
		else if (key == "--out") outPath = value;
		else if (key == "--profiles-out") profilesOut = value;
		else if (key == "--keymap") keymapPath = value;
		else if (key == "--usable-brushes") usableBrushesPath = value;
		else if (key == "--usable-doodads") usableDoodadsPath = value;
		else if (key == "--meander") meander = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
		else if (key == "--client") client = value;
		else if (key == "--otb-sha") otbSha = value;
		else if (key == "--dat-sha") datSha = value;
		else { std::cerr << "unknown option " << key << '\n'; return 2; }
	}
	if (manifestPath.empty() || fingerprintDir.empty() || statsDir.empty() || outPath.empty()) {
		std::cerr << "missing option; see the header of style_styles_main.cpp\n";
		return 2;
	}
	std::string text;
	if (!readFile(manifestPath, text)) { std::cerr << "cannot read manifest\n"; return 2; }
	const auto manifest = parseManifest(text);
	if (!manifest) { std::cerr << "invalid manifest\n"; return 3; }

	std::vector<Fingerprint> prints;
	std::vector<MapStats> stats;
	std::vector<MapDecor> decors;
	std::vector<std::vector<KeyTiles>> doodadItems; // per map: surface items per doodad brush (empty when the file is absent)
	std::vector<std::vector<KeyTiles>> keyTiles; // per map: tiles per ground key on z <= 7 (empty when the file is absent)
	for (const auto& entry : manifest->entries) {
		if (!readFile(fingerprintDir / (entry.alias + ".fp"), text)) { std::cerr << "missing fingerprint " << entry.alias << '\n'; return 3; }
		auto fp = parseFingerprint(text);
		if (!fp) { std::cerr << "invalid fingerprint " << entry.alias << '\n'; return 3; }
		prints.push_back(std::move(*fp));
		if (!readFile(statsDir / entry.alias / "run" / "analysis" / "kind-stats-surface.txt", text)) { std::cerr << "missing kind stats " << entry.alias << '\n'; return 3; }
		auto st = parseStats(text);
		if (!st) { std::cerr << "invalid kind stats " << entry.alias << '\n'; return 3; }
		stats.push_back({entry.alias, "", *st});
		if (!readFile(statsDir / entry.alias / "run" / "analysis" / "decor-context-surface.txt", text)) { std::cerr << "missing decor context " << entry.alias << '\n'; return 3; }
		auto dc = parseDecorContext(text);
		if (!dc) { std::cerr << "invalid decor context " << entry.alias << '\n'; return 3; }
		decors.push_back({entry.alias, "", *dc});
		doodadItems.emplace_back();
		if (readFile(statsDir / entry.alias / "run" / "analysis" / "doodad-brushes.csv", text)) {
			std::string_view rest = text;
			bool first = true;
			while (!rest.empty()) {
				const size_t end = rest.find('\n');
				std::string_view line = rest.substr(0, end);
				rest = end == std::string_view::npos ? std::string_view {} : rest.substr(end + 1);
				if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
				if (first) { first = false; continue; }
				if (line.empty()) continue;
				const auto f = csvFields(line);
				if (f.size() >= 2) doodadItems.back().push_back({f[0], std::strtoull(f[1].c_str(), nullptr, 10)});
			}
		}
		keyTiles.emplace_back();
		if (readFile(statsDir / entry.alias / "run" / "analysis" / "ground-keys-surface.csv", text)) {
			std::string_view rest = text;
			bool first = true;
			while (!rest.empty()) {
				const size_t end = rest.find('\n');
				std::string_view line = rest.substr(0, end);
				rest = end == std::string_view::npos ? std::string_view {} : rest.substr(end + 1);
				if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
				if (first) { first = false; continue; }
				if (line.empty()) continue;
				const auto f = csvFields(line);
				if (f.size() >= 2) keyTiles.back().push_back({f[0], std::strtoull(f[1].c_str(), nullptr, 10)});
			}
		}
	}
	const FamilyOptions familyOptions;
	const auto specific = withoutGenericWindows(prints, genericThreshold(prints.size(), 4));
	const auto families = assignFamilies(*manifest, specific, familyOptions);
	for (size_t i = 0; i < stats.size(); ++i) {
		stats[i].family = families.family[i];
		decors[i].family = families.family[i];
	}

	// Styles: families of style maps; a map is usable when it has a target of its own.
	struct Style {
		std::string name;
		std::vector<size_t> maps;    // style-role maps
		std::vector<size_t> usable;  // of them, with a target
		std::optional<TargetBuild> target;
		DecorContext decorTarget;
		StyleProfileFit profile;
		std::vector<Generated> generated;
	};
	std::map<std::string, Style> byName;
	for (size_t i = 0; i < manifest->entries.size(); ++i) {
		if (manifest->entries[i].role != ReferenceRole::Style) continue;
		Style& s = byName[families.family[i]];
		s.name = families.family[i];
		s.maps.push_back(i);
		const std::vector<MapStats> one {stats[i]};
		if (buildTarget(one, minNaturalTiles)) s.usable.push_back(i);
	}
	auto gather = [&](const std::vector<size_t>& indices) {
		std::vector<MapStats> v;
		for (const size_t i : indices) v.push_back(stats[i]);
		return v;
	};
	auto gatherDecor = [&](const std::vector<size_t>& indices) {
		std::vector<MapDecor> v;
		for (const size_t i : indices) v.push_back(decors[i]);
		return v;
	};

	FitOptions fit;
	fit.eval = {fitSide, {1, 2, 3, 4, 5, 6}};
	fit.maxPasses = 4;
	fit.minConnectivityPercent = minConnectivityPercent;
	auto fitProfile = [&](const std::vector<size_t>& trainMaps, StyleProfileFit& out) -> bool {
		const auto target = buildTarget(gather(trainMaps), minNaturalTiles);
		if (!target) return false;
		const auto steps = fitTuning(target->target, target->target, SurfaceTuning {}, fit, {});
		if (!steps) { std::cerr << "fit failed\n"; std::exit(4); }
		out.surface = steps->back().tuning;
		const DecorContext decorTarget = buildDecorTarget(gatherDecor(trainMaps));
		const auto decorFit = fitDecorTuning(decorTarget, out.surface, fit.eval, {});
		if (!decorFit) { std::cerr << "decor fit failed\n"; std::exit(4); }
		out.decor = *decorFit;
		return true;
	};

	const uint64_t cells = static_cast<uint64_t>(evalSide) * evalSide;
	const SurfaceBudget budget {cells, cells * 8, cells * 64 + (1u << 20)};
	auto generate = [&](const StyleProfileFit& p) {
		std::vector<Generated> out;
		for (size_t k = 0; k < seedCount; ++k) {
			const uint64_t seed = firstSeed + k;
			SurfaceConfig config {{100, 100, 7, evalSide, evalSide}, seed, p.surface};
			const auto grid = generateSurface(config, MapExtent {65001, 65001, 16}, budget);
			if (!grid) { std::cerr << "generation failed\n"; std::exit(4); }
			const auto placed = generateDecoration(*grid, seed, p.decor);
			if (!placed) { std::cerr << "decoration failed\n"; std::exit(4); }
			Generated g;
			g.target = targetFromStats(statsOfSurface(*grid));
			g.decor = decorContextOf(*grid, *placed);
			g.p1 = walkableConnectivityPpm(*grid);
			out.push_back(std::move(g));
		}
		return out;
	};
	auto distanceTo = [](const Generated& g, const StyleTarget& target) {
		return g.target ? distance(*g.target, target) : TargetDistance {2'000'000, 2'000'000};
	};

	const StyleProfileFit manual;
	const std::vector<Generated> baseline = generate(manual);

	std::vector<Style*> styles; // those with a target, in name order
	for (auto& [name, s] : byName) {
		if (s.usable.empty()) continue;
		if (auto built = buildTarget(gather(s.usable), minNaturalTiles)) s.target = std::move(*built);
		s.decorTarget = buildDecorTarget(gatherDecor(s.usable));
		if (!fitProfile(s.usable, s.profile)) continue;
		s.generated = generate(s.profile);
		styles.push_back(&s);
	}

	// Comparison of one generated set with the baseline on one target: S1, S2, S3, P1.
	struct Compare {
		Summary s1, s2, s3;
		uint32_t s3Terms = 0;
		int64_t p1Base = 0, p1Cand = 0;
		bool p1Ok = false;
	};
	auto compare = [&](const std::vector<Generated>& cand, const StyleTarget& target, const DecorContext& decorTarget) {
		Compare c;
		std::vector<int64_t> d1, d2, d3, pb, pc;
		const DecorDistance probe = decorDistance(decorTarget, decorTarget);
		c.s3Terms = probe.terms;
		for (size_t k = 0; k < seedCount; ++k) {
			const TargetDistance b = distanceTo(baseline[k], target), p = distanceTo(cand[k], target);
			d1.push_back(static_cast<int64_t>(p.shares) - static_cast<int64_t>(b.shares));
			d2.push_back(static_cast<int64_t>(p.adjacency) - static_cast<int64_t>(b.adjacency));
			if (probe.terms > 0) {
				const DecorDistance db = decorDistance(decorTarget, baseline[k].decor), dc = decorDistance(decorTarget, cand[k].decor);
				d3.push_back(static_cast<int64_t>(dc.sumPermille * 1000 / dc.terms) - static_cast<int64_t>(db.sumPermille * 1000 / db.terms));
			}
			if (baseline[k].p1 && cand[k].p1) { pb.push_back(*baseline[k].p1); pc.push_back(*cand[k].p1); }
		}
		c.s1 = summarize(d1);
		c.s2 = summarize(d2);
		c.s3 = summarize(d3);
		c.p1Base = medianOf(pb);
		c.p1Cand = medianOf(pc);
		c.p1Ok = !pb.empty() && c.p1Cand * 100 >= c.p1Base * 95;
		return c;
	};
	auto compareJson = [&](const Compare& c) {
		return std::format("\"S1_shares_ppm\": {}, \"S2_adjacency_ppm\": {}, \"S3_vegetation_microprm\": {}, \"S3_terms\": {}, \"P1_baseline_median_ppm\": {}, \"P1_candidate_median_ppm\": {}, \"P1_candidate_at_least_95_percent\": {}",
			summaryJson(c.s1), summaryJson(c.s2), summaryJson(c.s3), c.s3Terms, c.p1Base, c.p1Cand, c.p1Ok ? "true" : "false");
	};

	if (!profilesOut.empty()) {
		if (keymapPath.empty() || client.empty() || otbSha.empty() || datSha.empty() || manifest->entries.front().client != client) {
			std::cerr << "--profiles-out needs --keymap, --client (the manifest's), --otb-sha and --dat-sha\n";
			return 2;
		}
		if (!readFile(keymapPath, text)) { std::cerr << "cannot read key map\n"; return 2; }
		const auto keymap = parseKeyMap(text);
		if (!keymap) { std::cerr << "invalid key map\n"; return 3; }
		std::optional<std::set<std::string>> usableBrushes;
		if (!usableBrushesPath.empty()) {
			if (!readFile(usableBrushesPath, text)) { std::cerr << "cannot read the list of usable brushes\n"; return 2; }
			usableBrushes.emplace();
			std::string_view rest = text;
			while (!rest.empty()) {
				const size_t end = rest.find('\n');
				std::string_view line = rest.substr(0, end);
				rest = end == std::string_view::npos ? std::string_view {} : rest.substr(end + 1);
				if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
				if (!line.empty()) usableBrushes->insert(std::string(line));
			}
		}
		std::optional<std::set<std::string>> usableDoodads;
		if (!usableDoodadsPath.empty()) {
			if (!readFile(usableDoodadsPath, text)) { std::cerr << "cannot read the list of usable doodad brushes\n"; return 2; }
			usableDoodads.emplace();
			std::string_view rest = text;
			while (!rest.empty()) {
				const size_t end = rest.find('\n');
				std::string_view line = rest.substr(0, end);
				rest = end == std::string_view::npos ? std::string_view {} : rest.substr(end + 1);
				if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
				if (!line.empty()) usableDoodads->insert(std::string(line));
			}
		}
		fs::create_directories(profilesOut);
		for (const Style* st : styles) {
			StyleProfile profile;
			profile.client = client;
			profile.otbSha256 = otbSha;
			profile.datSha256 = datSha;
			profile.level = StyleLevel::Surface;
			profile.zMin = 0;
			profile.zMax = 7;
			profile.sources = st->target->used;
			profile.keymap = *keymap;
			for (const size_t i : st->usable) {
				for (size_t a = 0; a < styleKindCount; ++a) {
					profile.stats.kindTiles[a] += stats[i].stats.kindTiles[a];
					for (size_t b = 0; b < styleKindCount; ++b) profile.stats.pairs[a][b] += stats[i].stats.pairs[a][b];
				}
			}
			profile.target = st->target->target;
			profile.tuning = st->profile.surface;
			profile.tuning->meander = meander; // river swing fitted on the reference rivers (water_eval); the report above is without it
			profile.decorTarget = st->decorTarget;
			profile.decorTuning = st->profile.decor;
			// Brushes and doodads the style used, from every style map of the family (the thresholds guard small samples).
			std::map<std::string, uint64_t> summed;
			for (const size_t i : st->maps) {
				if (keyTiles[i].empty()) { std::cerr << "missing ground-keys-surface.csv for " << manifest->entries[i].alias << '\n'; return 3; }
				for (const KeyTiles& t : keyTiles[i]) summed[t.key] += t.tiles;
			}
			std::vector<KeyTiles> merged;
			for (const auto& [key, tiles] : summed) merged.push_back({key, tiles});
			profile.palette = choosePalette(*keymap, merged, 200, 25, usableBrushes ? &*usableBrushes : nullptr);
			std::map<std::string, uint64_t> doodadSums;
			for (const size_t i : st->maps) for (const KeyTiles& t : doodadItems[i]) doodadSums[t.key] += t.tiles;
			std::vector<KeyTiles> doodadMerged;
			for (const auto& [key, items] : doodadSums) doodadMerged.push_back({key, items});
			profile.decorPalette = chooseDecorPalette(doodadMerged, 30, 25, usableDoodads ? &*usableDoodads : nullptr);
			std::cout << st->name << " doodads: Tree=" << (profile.decorPalette[0].empty() ? "-" : profile.decorPalette[0]) << " Bush="
				<< (profile.decorPalette[1].empty() ? "-" : profile.decorPalette[1]) << " Stone=" << (profile.decorPalette[2].empty() ? "-" : profile.decorPalette[2]) << '\n';
			std::cout << st->name << " palette:";
			for (size_t k = 0; k < naturalKindCount; ++k) std::cout << ' ' << styleKindName(static_cast<StyleKind>(k)) << '=' << (profile.palette[k].empty() ? "-" : profile.palette[k]);
			std::cout << '\n';
			const auto out = serializeProfile(profile);
			if (!out) { std::cerr << "cannot serialize profile " << st->name << '\n'; return 4; }
			std::ofstream f(profilesOut / (st->name + ".profile"), std::ios::binary);
			f << *out;
			if (!f) { std::cerr << "cannot write profile " << st->name << '\n'; return 4; }
		}
	}

	std::string report = "{\n  \"schema\": \"g4-styles-report/1\",\n";
	report += std::format("  \"fit_side\": {}, \"eval_side\": {}, \"eval_seeds\": [{}, {}], \"min_connectivity_percent\": {},\n  \"styles\": [\n", fitSide, evalSide, firstSeed, firstSeed + seedCount - 1, minConnectivityPercent);

	size_t betterBoth = 0, worseBoth = 0, p1Failing = 0;
	for (size_t si = 0; si < styles.size(); ++si) {
		Style& s = *styles[si];
		const Compare a = compare(s.generated, s.target->target, s.decorTarget);
		betterBoth += a.s1.verdict == Summary::Verdict::Better && a.s2.verdict == Summary::Verdict::Better;
		worseBoth += a.s1.verdict == Summary::Verdict::Worse && a.s2.verdict == Summary::Verdict::Worse;
		p1Failing += !a.p1Ok;
		std::string maps = "[";
		for (size_t i = 0; i < s.usable.size(); ++i) maps += std::format("{}{}", i ? ", " : "", quote(manifest->entries[s.usable[i]].alias));
		report += std::format("    {{\"style\": {}, \"maps_with_target\": {}, \"profile\": {}, \"A\": {{{}}}}}{}\n",
			quote(s.name), maps + "]", tuningJson(s.profile.surface, s.profile.decor), compareJson(a), si + 1 < styles.size() ? "," : "");
	}
	report += "  ],\n";

	// C: median over seeds of S1 + S2 for profile i against target j; baseline as an extra row.
	auto cell = [&](const std::vector<Generated>& gens, const StyleTarget& target) {
		std::vector<int64_t> v;
		for (const Generated& g : gens) { const TargetDistance d = distanceTo(g, target); v.push_back(static_cast<int64_t>(d.total())); }
		return medianOf(v);
	};
	size_t recognized = 0;
	report += "  \"C_matrix_median_S1_plus_S2\": {\n    \"targets\": [";
	for (size_t j = 0; j < styles.size(); ++j) report += std::format("{}{}", j ? ", " : "", quote(styles[j]->name));
	report += "],\n    \"rows\": [\n";
	for (size_t i = 0; i < styles.size(); ++i) {
		report += std::format("      {{\"profile\": {}, \"distances\": [", quote(styles[i]->name));
		for (size_t j = 0; j < styles.size(); ++j) report += std::format("{}{}", j ? ", " : "", cell(styles[i]->generated, styles[j]->target->target));
		report += "]},\n";
	}
	report += "      {\"profile\": \"manual\", \"distances\": [";
	for (size_t j = 0; j < styles.size(); ++j) report += std::format("{}{}", j ? ", " : "", cell(baseline, styles[j]->target->target));
	report += "]}\n    ],\n    \"recognized\": [";
	bool firstRecognized = true;
	for (size_t j = 0; j < styles.size(); ++j) {
		const int64_t own = cell(styles[j]->generated, styles[j]->target->target);
		bool strictlyBest = true;
		for (size_t i = 0; i < styles.size(); ++i) if (i != j && cell(styles[i]->generated, styles[j]->target->target) <= own) strictlyBest = false;
		recognized += strictlyBest;
		report += std::format("{}{}", firstRecognized ? "" : ", ", strictlyBest ? "true" : "false");
		firstRecognized = false;
	}
	report += "]\n  },\n";

	// B: leave one map out inside styles with at least two maps that have a target.
	report += "  \"B_leave_one_map_out\": [\n";
	bool firstB = true, bWorseBoth = false;
	for (const Style* s : styles) {
		if (s->usable.size() < 2) continue;
		for (const size_t held : s->usable) {
			std::vector<size_t> rest;
			for (const size_t i : s->usable) if (i != held) rest.push_back(i);
			StyleProfileFit p;
			if (!fitProfile(rest, p)) continue;
			const std::vector<MapStats> heldStats {stats[held]};
			const auto target = buildTarget(heldStats, minNaturalTiles);
			const DecorContext decorTarget = buildDecorTarget(gatherDecor({held}));
			const Compare b = compare(generate(p), target->target, decorTarget);
			bWorseBoth = bWorseBoth || (b.s1.verdict == Summary::Verdict::Worse && b.s2.verdict == Summary::Verdict::Worse);
			p1Failing += !b.p1Ok;
			report += std::format("{}    {{\"style\": {}, \"held_out\": {}, \"profile\": {}, {}}}", firstB ? "" : ",\n", quote(s->name), quote(manifest->entries[held].alias), tuningJson(p.surface, p.decor), compareJson(b));
			firstB = false;
		}
	}
	report += "\n  ],\n";

	const bool aPass = styles.size() > 0 && betterBoth * 3 >= styles.size() * 2 && worseBoth == 0;
	const bool cPass = styles.size() > 0 && recognized * 3 >= styles.size() * 2;
	const bool p1Pass = p1Failing == 0;
	report += std::format("  \"gate\": {{\"styles\": {}, \"A_better_in_S1_and_S2\": {}, \"A_worse_in_S1_and_S2\": {}, \"A_pass\": {}, \"C_recognized\": {}, \"C_pass\": {}, \"B_any_worse_in_both\": {}, \"P1_profiles_failing\": {}, \"P1_pass\": {}, \"result\": {}}}\n}}\n",
		styles.size(), betterBoth, worseBoth, aPass ? "true" : "false", recognized, cPass ? "true" : "false", bWorseBoth ? "true" : "false", p1Failing, p1Pass ? "true" : "false",
		quote(aPass && cPass && p1Pass ? "met in the measured scope (in-sample; not a held-out result)" : "not met"));
	std::ofstream f(outPath, std::ios::binary);
	f << report;
	return f ? 0 : 4;
}
