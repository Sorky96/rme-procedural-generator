// SPDX-License-Identifier: GPL-3.0-or-later
// Offline tool (G4-F): evaluates the fitted profiles of each fold on the TEST maps of that fold against the
// manual presets, exactly as fixed in docs/procedural/G4_METRICS.md before this tool was run.
//
// Usage: style_eval --manifest FILE --fingerprints DIR --stats DIR --profiles DIR --client ID --otb-sha HEX
//                   --dat-sha HEX --out FILE
// --profiles holds fold-N.profile written by style_fit; --stats is the same directory style_fit read.

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
#include <string>

using namespace rme::procedural;
namespace fs = std::filesystem;

namespace {

// Fixed by G4_METRICS.md.
constexpr uint32_t evalSide = 256;
constexpr uint64_t firstSeed = 1001;
constexpr size_t seedCount = 32;
constexpr uint64_t minNaturalTiles = 1000;

bool readFile(const fs::path& path, std::string& out) {
	std::ifstream in(path, std::ios::binary);
	if (!in) return false;
	out.assign(std::istreambuf_iterator<char>(in), {});
	return true;
}

std::string quote(const std::string& s) { return '"' + s + '"'; }

struct SeedResult {
	TargetDistance distance;        // S1 shares, S2 adjacency
	DecorContext decor;
	std::optional<uint32_t> p1;
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

// diffs = candidate - baseline per seed, smaller is better. Pre-registered: better = median < 0 and the
// candidate wins in at least 75 % of the seeds; worse is the mirror image.
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
	const size_t needed = (n * 3 + 3) / 4; // ceil(0.75 n)
	if (s.median < 0 && s.better >= needed) s.verdict = Summary::Verdict::Better;
	else if (s.median > 0 && s.worse >= needed) s.verdict = Summary::Verdict::Worse;
	else s.verdict = Summary::Verdict::Undecided;
	return s;
}

std::string summaryJson(const Summary& s) {
	return std::format("{{\"seeds\": {}, \"median\": {}, \"p10\": {}, \"p90\": {}, \"candidate_better\": {}, \"candidate_worse\": {}, \"verdict\": {}}}",
		s.n, s.median, s.p10, s.p90, s.better, s.worse, quote(verdictName(s.verdict)));
}

int64_t medianOf(std::vector<int64_t> v) {
	if (v.empty()) return 0;
	std::sort(v.begin(), v.end());
	const size_t n = v.size();
	return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}

} // namespace

int main(int argc, char** argv) {
	fs::path manifestPath, fingerprintDir, statsDir, profileDir, outPath;
	std::string client, otbSha, datSha;
	for (int i = 1; i + 1 < argc; i += 2) {
		const std::string key = argv[i];
		const char* value = argv[i + 1];
		if (key == "--manifest") manifestPath = value;
		else if (key == "--fingerprints") fingerprintDir = value;
		else if (key == "--stats") statsDir = value;
		else if (key == "--profiles") profileDir = value;
		else if (key == "--client") client = value;
		else if (key == "--otb-sha") otbSha = value;
		else if (key == "--dat-sha") datSha = value;
		else if (key == "--out") outPath = value;
		else { std::cerr << "unknown option " << key << '\n'; return 2; }
	}
	if (manifestPath.empty() || fingerprintDir.empty() || statsDir.empty() || profileDir.empty() || client.empty() || otbSha.empty() || datSha.empty() || outPath.empty()) {
		std::cerr << "missing option; see the header of style_eval_main.cpp\n";
		return 2;
	}
	std::string text;
	if (!readFile(manifestPath, text)) { std::cerr << "cannot read manifest\n"; return 2; }
	const auto manifest = parseManifest(text);
	if (!manifest) { std::cerr << "invalid manifest\n"; return 3; }
	if (manifest->entries.front().client != client) { std::cerr << "manifest client differs from --client\n"; return 3; }

	std::vector<Fingerprint> prints;
	std::vector<MapStats> stats;
	std::vector<MapDecor> decors;
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
	}
	const FamilyOptions familyOptions;
	const auto specific = withoutGenericWindows(prints, genericThreshold(prints.size(), 4));
	const auto families = assignFamilies(*manifest, specific, familyOptions);
	const auto folds = makeFolds(*manifest, families, specific, familyOptions);
	if (!folds) { std::cerr << "cannot make folds\n"; return 3; }
	for (size_t i = 0; i < stats.size(); ++i) {
		stats[i].family = families.family[i];
		decors[i].family = families.family[i];
	}

	const uint64_t cells = static_cast<uint64_t>(evalSide) * evalSide;
	const SurfaceBudget budget {cells, cells * 8, cells * 64 + (1u << 20)};
	auto runSeed = [&](const SurfaceTuning& surface, const DecorTuning& decor, uint64_t seed, const StyleTarget& target) -> std::optional<SeedResult> {
		SurfaceConfig config {{100, 100, 7, evalSide, evalSide}, seed, surface};
		const auto grid = generateSurface(config, MapExtent {65001, 65001, 16}, budget);
		if (!grid) return std::nullopt;
		const auto placed = generateDecoration(*grid, seed, decor);
		if (!placed) return std::nullopt;
		SeedResult r;
		const auto generated = targetFromStats(statsOfSurface(*grid));
		r.distance = generated ? distance(*generated, target) : TargetDistance {2'000'000, 2'000'000};
		r.decor = decorContextOf(*grid, *placed);
		r.p1 = walkableConnectivityPpm(*grid);
		return r;
	};

	struct FoldResult {
		std::string json;
		bool evaluated = false;
		Summary s1, s2, s3;
		bool p1Ok = true;
		int64_t p1BaseMedian = 0, p1CandMedian = 0;
	};
	std::vector<FoldResult> results;
	std::string report = "{\n  \"schema\": \"g4-eval-report/1\",\n";
	report += std::format("  \"client\": {}, \"side\": {}, \"seeds\": [{}, {}], \"min_natural_tiles\": {},\n  \"folds\": [\n", quote(client), evalSide, firstSeed, firstSeed + seedCount - 1, minNaturalTiles);

	for (size_t f = 0; f < folds->size(); ++f) {
		const Fold& fold = (*folds)[f];
		FoldResult fr;
		auto finish = [&](const std::string& body) {
			fr.json = std::format("    {{\"fold\": {}, \"test_family\": {}, \"test_maps\": {}, {}}}", f, quote(fold.testFamily), fold.test.size(), body);
		};
		std::string profileText;
		if (!readFile(profileDir / std::format("fold-{}.profile", f), profileText)) {
			finish("\"evaluated\": false, \"reason\": \"no profile: no usable train or validation target\"");
			results.push_back(std::move(fr));
			continue;
		}
		const auto profile = parseProfile(profileText);
		if (!profile || !profile->tuning || !profile->decorTuning) { std::cerr << "invalid profile for fold " << f << '\n'; return 3; }
		if (checkProfile(*profile, client, otbSha, datSha, 7) != ProfileCompatibility::Compatible) { std::cerr << "profile of fold " << f << " refused: other client or assets\n"; return 3; }
		std::vector<MapStats> testStats;
		std::vector<MapDecor> testDecors;
		for (const size_t i : fold.test) { testStats.push_back(stats[i]); testDecors.push_back(decors[i]); }
		const auto testTarget = buildTarget(testStats, minNaturalTiles);
		if (!testTarget) {
			finish("\"evaluated\": false, \"reason\": \"no usable test target (fewer than 1000 natural tiles on z<=7)\"");
			results.push_back(std::move(fr));
			continue;
		}
		const DecorContext testDecor = buildDecorTarget(testDecors);
		// Terms comparable for this fold's test target (independent of the generated side).
		const DecorDistance probe = decorDistance(testDecor, testDecor);

		std::vector<int64_t> d1, d2, d3, p1Base, p1Cand;
		for (size_t k = 0; k < seedCount; ++k) {
			const uint64_t seed = firstSeed + k;
			const auto base = runSeed(SurfaceTuning {}, DecorTuning {}, seed, testTarget->target);
			const auto cand = runSeed(*profile->tuning, *profile->decorTuning, seed, testTarget->target);
			if (!base || !cand) { std::cerr << "generation failed in fold " << f << " seed " << seed << '\n'; return 4; }
			d1.push_back(static_cast<int64_t>(cand->distance.shares) - static_cast<int64_t>(base->distance.shares));
			d2.push_back(static_cast<int64_t>(cand->distance.adjacency) - static_cast<int64_t>(base->distance.adjacency));
			if (probe.terms > 0) {
				const DecorDistance db = decorDistance(testDecor, base->decor), dc = decorDistance(testDecor, cand->decor);
				d3.push_back(static_cast<int64_t>(dc.sumPermille * 1000 / dc.terms) - static_cast<int64_t>(db.sumPermille * 1000 / db.terms));
			}
			if (base->p1 && cand->p1) { p1Base.push_back(*base->p1); p1Cand.push_back(*cand->p1); }
		}
		fr.evaluated = true;
		fr.s1 = summarize(d1);
		fr.s2 = summarize(d2);
		fr.s3 = summarize(d3);
		fr.p1BaseMedian = medianOf(p1Base);
		fr.p1CandMedian = medianOf(p1Cand);
		fr.p1Ok = !p1Base.empty() && fr.p1CandMedian * 100 >= fr.p1BaseMedian * 95;
		finish(std::format("\"evaluated\": true, \"test_used\": {}, \"S1_shares_ppm\": {}, \"S2_adjacency_ppm\": {}, \"S3_vegetation_microprm\": {}, \"S3_terms\": {}, \"P1_pairs\": {}, \"P1_baseline_median_ppm\": {}, \"P1_candidate_median_ppm\": {}, \"P1_candidate_at_least_95_percent\": {}",
			testTarget->used.size(), summaryJson(fr.s1), summaryJson(fr.s2), summaryJson(fr.s3), probe.terms, p1Base.size(), fr.p1BaseMedian, fr.p1CandMedian, fr.p1Ok ? "true" : "false"));
		results.push_back(std::move(fr));
	}
	for (size_t f = 0; f < results.size(); ++f) report += results[f].json + (f + 1 < results.size() ? ",\n" : "\n");
	report += "  ],\n";

	// Pre-registered gate.
	size_t evaluated = 0, s1Better = 0, s2Better = 0, bothWorse = 0, s3Better = 0, s3Worse = 0, s3Folds = 0, p1Fail = 0;
	for (const FoldResult& r : results) {
		if (!r.evaluated) continue;
		++evaluated;
		s1Better += r.s1.verdict == Summary::Verdict::Better;
		s2Better += r.s2.verdict == Summary::Verdict::Better;
		bothWorse += r.s1.verdict == Summary::Verdict::Worse && r.s2.verdict == Summary::Verdict::Worse;
		if (r.s3.n > 0) {
			++s3Folds;
			s3Better += r.s3.verdict == Summary::Verdict::Better;
			s3Worse += r.s3.verdict == Summary::Verdict::Worse;
		}
		p1Fail += !r.p1Ok;
	}
	const bool s1Pass = evaluated > 0 && s1Better * 3 >= evaluated * 2, s2Pass = evaluated > 0 && s2Better * 3 >= evaluated * 2;
	const bool bothWorseNone = bothWorse == 0, s3Pass = s3Better > s3Worse, p1Pass = p1Fail == 0;
	const bool gate = s1Pass && s2Pass && bothWorseNone && s3Pass && p1Pass;
	report += std::format("  \"gate\": {{\"evaluated_folds\": {}, \"S1_better_folds\": {}, \"S1_two_thirds\": {}, \"S2_better_folds\": {}, \"S2_two_thirds\": {}, \"folds_worse_in_both_S1_and_S2\": {}, \"S3_folds_with_context\": {}, \"S3_better\": {}, \"S3_worse\": {}, \"S3_more_better_than_worse\": {}, \"P1_folds_failing\": {}, \"result\": {}}}\n}}\n",
		evaluated, s1Better, s1Pass ? "true" : "false", s2Better, s2Pass ? "true" : "false", bothWorse, s3Folds, s3Better, s3Worse, s3Pass ? "true" : "false", p1Fail, quote(gate ? "met in the measured scope" : "not met"));
	std::ofstream f(outPath, std::ios::binary);
	f << report;
	return f ? 0 : 4;
}
