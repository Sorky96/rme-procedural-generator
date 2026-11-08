// SPDX-License-Identifier: GPL-3.0-or-later
// Offline tool (G4-C): per leave-one-family-out fold, builds the target from the TRAIN maps, fits the surface
// tuning to it (validation maps only choose the step) and writes a profile per fold plus one fitted on all
// style maps. It never reads the test maps' statistics: held-out evaluation is a separate step (G4-F).
//
// Usage: style_fit --manifest FILE --fingerprints DIR --stats DIR --keymap FILE --client ID --otb-sha HEX
//                  --dat-sha HEX --out-dir DIR [--side N] [--min-natural N]
// DIR for --stats holds <alias>/run/analysis/kind-stats-surface.txt (written by the reference import probe).

#include "procedural/core/reference_split.h"
#include "procedural/core/style_fit.h"
#include "procedural/core/style_profile.h"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
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

std::string quote(const std::string& s) { return '"' + s + '"'; }

std::string tuningJson(const SurfaceTuning& t) {
	return std::format("{{\"featureSize\": {}, \"lakePermille\": {}, \"minLakeCells\": {}, \"riverCount\": {}, \"minRiverLength\": {}, \"shoreWidth\": {}, \"rockPermille\": {}, \"minRockCells\": {}, \"dirtPermille\": {}, \"transitionWidth\": {}}}",
		t.featureSize, t.lakePermille, t.minLakeCells, t.riverCount, t.minRiverLength, t.shoreWidth, t.rockPermille, t.minRockCells, t.dirtPermille, t.transitionWidth);
}

std::string distanceJson(const TargetDistance& d) {
	return std::format("{{\"shares\": {}, \"adjacency\": {}, \"total\": {}}}", d.shares, d.adjacency, d.total());
}

} // namespace

int main(int argc, char** argv) {
	fs::path manifestPath, fingerprintDir, statsDir, keymapPath, outDir;
	std::string client, otbSha, datSha;
	uint32_t side = 160;
	uint64_t minNatural = 1000;
	for (int i = 1; i + 1 < argc; i += 2) {
		const std::string key = argv[i];
		const char* value = argv[i + 1];
		if (key == "--manifest") manifestPath = value;
		else if (key == "--fingerprints") fingerprintDir = value;
		else if (key == "--stats") statsDir = value;
		else if (key == "--keymap") keymapPath = value;
		else if (key == "--client") client = value;
		else if (key == "--otb-sha") otbSha = value;
		else if (key == "--dat-sha") datSha = value;
		else if (key == "--out-dir") outDir = value;
		else if (key == "--side") side = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
		else if (key == "--min-natural") minNatural = std::strtoull(value, nullptr, 10);
		else { std::cerr << "unknown option " << key << '\n'; return 2; }
	}
	if (manifestPath.empty() || fingerprintDir.empty() || statsDir.empty() || keymapPath.empty() || client.empty() || otbSha.empty() || datSha.empty() || outDir.empty()) {
		std::cerr << "missing option; see the header of style_fit_main.cpp\n";
		return 2;
	}
	std::string text;
	if (!readFile(manifestPath, text)) { std::cerr << "cannot read manifest\n"; return 2; }
	const auto manifest = parseManifest(text);
	if (!manifest) { std::cerr << "invalid manifest\n"; return 3; }
	if (manifest->entries.front().client != client) { std::cerr << "manifest client differs from --client\n"; return 3; }
	if (!readFile(keymapPath, text)) { std::cerr << "cannot read key map\n"; return 2; }
	const auto keymap = parseKeyMap(text);
	if (!keymap) { std::cerr << "invalid key map\n"; return 3; }

	const auto& entries = manifest->entries;
	std::vector<Fingerprint> prints;
	std::vector<MapStats> stats;
	for (const auto& entry : entries) {
		if (!readFile(fingerprintDir / (entry.alias + ".fp"), text)) { std::cerr << "missing fingerprint " << entry.alias << '\n'; return 3; }
		auto fp = parseFingerprint(text);
		if (!fp) { std::cerr << "invalid fingerprint " << entry.alias << '\n'; return 3; }
		prints.push_back(std::move(*fp));
		if (!readFile(statsDir / entry.alias / "run" / "analysis" / "kind-stats-surface.txt", text)) { std::cerr << "missing kind stats " << entry.alias << '\n'; return 3; }
		auto st = parseStats(text);
		if (!st) { std::cerr << "invalid kind stats " << entry.alias << '\n'; return 3; }
		stats.push_back({entry.alias, "", *st});
	}
	// Same split as reference_split, with its default parameters.
	const FamilyOptions familyOptions;
	const uint32_t genericAtLeast = genericThreshold(prints.size(), 4);
	const auto specific = withoutGenericWindows(prints, genericAtLeast);
	const auto families = assignFamilies(*manifest, specific, familyOptions);
	const auto folds = makeFolds(*manifest, families, specific, familyOptions);
	if (!folds) { std::cerr << "cannot make folds\n"; return 3; }
	for (size_t i = 0; i < stats.size(); ++i) stats[i].family = families.family[i];

	FitOptions fit;
	fit.eval = {side, {1, 2, 3, 4, 5, 6}};
	fit.maxPasses = 4;
	const SurfaceTuning baseline;
	fs::create_directories(outDir);

	auto gather = [&](const std::vector<size_t>& indices) {
		std::vector<MapStats> maps;
		for (const size_t i : indices) maps.push_back(stats[i]);
		return maps;
	};
	auto sumStats = [&](const std::vector<ProfileSource>& used) {
		StyleStats total;
		for (const auto& source : used) {
			for (const auto& m : stats) {
				if (m.alias != source.alias) continue;
				for (size_t a = 0; a < styleKindCount; ++a) {
					total.kindTiles[a] += m.stats.kindTiles[a];
					for (size_t b = 0; b < styleKindCount; ++b) total.pairs[a][b] += m.stats.pairs[a][b];
				}
			}
		}
		return total;
	};
	auto writeProfile = [&](const std::string& name, const TargetBuild& build, const SurfaceTuning& tuning) {
		StyleProfile profile;
		profile.client = client;
		profile.otbSha256 = otbSha;
		profile.datSha256 = datSha;
		profile.level = StyleLevel::Surface;
		profile.zMin = 0;
		profile.zMax = 7;
		profile.sources = build.used;
		profile.keymap = *keymap;
		profile.stats = sumStats(build.used);
		profile.target = build.target;
		profile.tuning = tuning;
		const auto out = serializeProfile(profile);
		if (!out) return false;
		std::ofstream f(outDir / name, std::ios::binary);
		f << *out;
		return static_cast<bool>(f);
	};

	std::string report = "{\n  \"schema\": \"g4-fit-report/1\",\n";
	report += std::format("  \"client\": {}, \"side\": {}, \"fit_seeds\": [1, 2, 3, 4, 5, 6], \"min_natural_tiles\": {}, \"max_passes\": {},\n", quote(client), side, minNatural, fit.maxPasses);
	report += std::format("  \"baseline\": {},\n  \"folds\": [\n", tuningJson(baseline));
	for (size_t f = 0; f < folds->size(); ++f) {
		const Fold& fold = (*folds)[f];
		const auto train = buildTarget(gather(fold.train), minNatural);
		const auto validation = buildTarget(gather(fold.validation), minNatural);
		if (!train || !validation) {
			report += std::format("    {{\"test_family\": {}, \"error\": \"no usable train or validation target\"}}{}\n", quote(fold.testFamily), f + 1 < folds->size() ? "," : "");
			continue;
		}
		const auto steps = fitTuning(train->target, validation->target, baseline, fit, {});
		if (!steps) { std::cerr << "fit failed in fold " << f << '\n'; return 4; }
		const size_t chosen = selectStep(*steps);
		if (!writeProfile(std::format("fold-{}.profile", f), *train, (*steps)[chosen].tuning)) { std::cerr << "cannot write profile\n"; return 4; }
		auto aliases = [&](const std::vector<ProfileSource>& used) {
			std::string s = "[";
			for (size_t i = 0; i < used.size(); ++i) s += std::format("{}{}", i ? ", " : "", quote(used[i].alias));
			return s + "]";
		};
		report += std::format("    {{\"fold\": {}, \"test_family\": {}, \"validation_family\": {}, \"test_maps\": {}, \"train_used\": {}, \"train_excluded\": {}, \"validation_used\": {}, \"validation_excluded\": {}, \"steps\": [\n",
			f, quote(fold.testFamily), quote(fold.validationFamily), fold.test.size(), aliases(train->used), train->excluded.size(), aliases(validation->used), validation->excluded.size());
		for (size_t s = 0; s < steps->size(); ++s) {
			const FitStep& step = (*steps)[s];
			report += std::format("      {{\"pass\": {}, \"tuning\": {}, \"train\": {}, \"validation\": {}}}{}\n", s, tuningJson(step.tuning), distanceJson(step.train), distanceJson(step.validation), s + 1 < steps->size() ? "," : "");
		}
		report += std::format("    ], \"selected_pass\": {}}}{}\n", chosen, f + 1 < folds->size() ? "," : "");
	}
	report += "  ],\n";

	std::vector<size_t> allStyle;
	for (size_t i = 0; i < entries.size(); ++i) if (entries[i].role == ReferenceRole::Style) allStyle.push_back(i);
	const auto all = buildTarget(gather(allStyle), minNatural);
	if (!all) { std::cerr << "no usable style maps\n"; return 4; }
	const auto allSteps = fitTuning(all->target, all->target, baseline, fit, {});
	if (!allSteps) { std::cerr << "fit failed on all style maps\n"; return 4; }
	if (!writeProfile("profile-all.profile", *all, allSteps->back().tuning)) { std::cerr << "cannot write profile\n"; return 4; }
	report += std::format("  \"all_style_maps\": {{\"families\": {}, \"used\": {}, \"excluded\": {}, \"baseline_distance\": {}, \"fitted_distance\": {}, \"tuning\": {}}}\n}}\n",
		all->familyCount, all->used.size(), all->excluded.size(), distanceJson(allSteps->front().train), distanceJson(allSteps->back().train), tuningJson(allSteps->back().tuning));
	std::ofstream f(outDir / "fit-report.json", std::ios::binary);
	f << report;
	return f ? 0 : 4;
}
