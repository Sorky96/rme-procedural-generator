// SPDX-License-Identifier: GPL-3.0-or-later
// Offline tool (G4 iteration 4, D9 water part and D6): measures river shape in the reference maps, fits the river
// meander of the generator to it and evaluates the fit on another map, exactly as fixed in docs/procedural/
// G4_METRICS.md ("Iteracja 4") before this tool was run.
//
// Usage: water_eval --manifest FILE --shapes DIR --out FILE
// DIR holds <alias>/run/analysis/water-shapes-surface.txt (written by the reference import probe).

#include "procedural/core/reference_split.h"
#include "procedural/core/style_fit.h"
#include "procedural/core/water_shape.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>

using namespace rme::procedural;
namespace fs = std::filesystem;

namespace {

// Fixed by G4_METRICS.md.
constexpr uint32_t fitSide = 160, evalSide = 256;
constexpr size_t fitSeedCount = 6, evalSeedCount = 32;
constexpr uint64_t firstEvalSeed = 1001;
constexpr size_t minRivers = 5;
constexpr uint32_t meanderGrid[] {0, 1, 2, 3, 4, 5, 6, 8};

bool readFile(const fs::path& path, std::string& out) {
	std::ifstream in(path, std::ios::binary);
	if (!in) return false;
	out.assign(std::istreambuf_iterator<char>(in), {});
	return true;
}

std::string quote(const std::string& s) { return '"' + s + '"'; }

int64_t medianOf(std::vector<int64_t> v) {
	if (v.empty()) return 0;
	std::sort(v.begin(), v.end());
	const size_t n = v.size();
	return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}

struct Summary {
	int64_t median = 0, p10 = 0, p90 = 0;
	size_t better = 0, worse = 0, n = 0;
	const char* verdict = "n/a";
};

// diffs = candidate - baseline, smaller is better: better = median < 0 and at least 75 % of the seeds.
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
	s.verdict = s.median < 0 && s.better >= needed ? "better" : (s.median > 0 && s.worse >= needed ? "worse" : "undecided");
	return s;
}

std::string summaryJson(const Summary& s) {
	return std::format("{{\"seeds\": {}, \"median\": {}, \"p10\": {}, \"p90\": {}, \"candidate_better\": {}, \"candidate_worse\": {}, \"verdict\": {}}}",
		s.n, s.median, s.p10, s.p90, s.better, s.worse, quote(s.verdict));
}

struct SeedRivers {
	RiverSummary rivers;               // count == 0: no river on this seed
	std::optional<uint32_t> walkable;  // P1
	bool reachesWater = true;          // structural invariant of the river cells
};

// Every River cell must be 4-connected through River cells to a Lake cell or the border of the area.
bool riversReachWater(const SurfaceGrid& grid) {
	const size_t w = grid.region.width, h = grid.region.height;
	std::vector<uint8_t> seen(grid.cells.size(), 0);
	std::vector<size_t> stack;
	for (size_t start = 0; start < grid.cells.size(); ++start) {
		if (seen[start] || grid.cells[start] != SurfaceKind::River) continue;
		bool reaches = false;
		stack.assign(1, start);
		seen[start] = 1;
		while (!stack.empty()) {
			const size_t i = stack.back();
			stack.pop_back();
			const size_t x = i % w, y = i / w;
			reaches |= x == 0 || y == 0 || x + 1 == w || y + 1 == h;
			auto visit = [&](size_t j) {
				if (grid.cells[j] == SurfaceKind::Lake) reaches = true;
				if (!seen[j] && grid.cells[j] == SurfaceKind::River) { seen[j] = 1; stack.push_back(j); }
			};
			if (x > 0) visit(i - 1);
			if (x + 1 < w) visit(i + 1);
			if (y > 0) visit(i - w);
			if (y + 1 < h) visit(i + w);
		}
		if (!reaches) return false;
	}
	return true;
}

std::vector<SeedRivers> measure(const SurfaceTuning& tuning, uint32_t side, uint64_t firstSeed, size_t count) {
	std::vector<SeedRivers> out;
	const uint64_t cells = uint64_t(side) * side;
	const SurfaceBudget budget {cells, cells * 8, cells * 64 + (1u << 20)};
	for (size_t k = 0; k < count; ++k) {
		SurfaceConfig config {{100, 100, 7, side, side}, firstSeed + k, tuning};
		const auto grid = generateSurface(config, MapExtent {65001, 65001, 16}, budget);
		if (!grid) { std::cerr << "generation failed\n"; std::exit(4); }
		const auto shapes = waterShapesOf(*grid);
		if (!shapes) { std::cerr << "water shapes failed\n"; std::exit(4); }
		out.push_back({summarizeRivers(shapes->rivers), walkableConnectivityPpm(*grid), riversReachWater(*grid)});
	}
	return out;
}

int64_t gen(const std::vector<SeedRivers>& v, bool tortuosity) {
	std::vector<int64_t> values;
	for (const SeedRivers& s : v) if (s.rivers.count) values.push_back(tortuosity ? s.rivers.tortuosityPermille : s.rivers.straightPermille);
	return medianOf(values);
}

uint32_t fitMeander(const RiverSummary& reference) {
	uint32_t best = 0;
	int64_t bestDistance = INT64_MAX;
	for (const uint32_t m : meanderGrid) {
		SurfaceTuning t;
		t.meander = m;
		const auto seeds = measure(t, fitSide, 1, fitSeedCount);
		const int64_t d = std::llabs(gen(seeds, true) - int64_t(reference.tortuosityPermille)) + std::llabs(gen(seeds, false) - int64_t(reference.straightPermille));
		if (d < bestDistance) { bestDistance = d; best = m; }
	}
	return best;
}

} // namespace

int main(int argc, char** argv) {
	fs::path manifestPath, shapesDir, outPath;
	for (int i = 1; i + 1 < argc; i += 2) {
		const std::string key = argv[i];
		if (key == "--manifest") manifestPath = argv[i + 1];
		else if (key == "--shapes") shapesDir = argv[i + 1];
		else if (key == "--out") outPath = argv[i + 1];
		else { std::cerr << "unknown option " << key << '\n'; return 2; }
	}
	if (manifestPath.empty() || shapesDir.empty() || outPath.empty()) { std::cerr << "usage: water_eval --manifest FILE --shapes DIR --out FILE\n"; return 2; }
	std::string text;
	if (!readFile(manifestPath, text)) { std::cerr << "cannot read manifest\n"; return 2; }
	const auto manifest = parseManifest(text);
	if (!manifest) { std::cerr << "invalid manifest\n"; return 3; }

	struct Reference { std::string alias; WaterShapes shapes; RiverSummary summary; };
	std::vector<Reference> references; // style maps that qualify
	std::string report = "{\n  \"schema\": \"g4-water-report/1\",\n  \"reference_maps\": [\n";
	bool firstMap = true;
	for (const auto& entry : manifest->entries) {
		if (entry.role != ReferenceRole::Style) continue;
		if (!readFile(shapesDir / entry.alias / "run" / "analysis" / "water-shapes-surface.txt", text)) { std::cerr << "missing water shapes " << entry.alias << '\n'; return 3; }
		auto shapes = parseWaterShapes(text);
		if (!shapes) { std::cerr << "invalid water shapes " << entry.alias << '\n'; return 3; }
		const RiverSummary summary = summarizeRivers(shapes->rivers);
		report += std::format("{}    {{\"alias\": {}, \"rivers\": {}, \"lakes\": {}, \"sea_tiles\": {}, \"tortuosity_permille\": {}, \"straight_permille\": {}, \"width_milli\": {}, \"qualifies\": {}}}",
			firstMap ? "" : ",\n", quote(entry.alias), summary.count, shapes->lakeAreas.size(), shapes->seaTiles, summary.tortuosityPermille, summary.straightPermille, summary.widthMilli, summary.count >= minRivers ? "true" : "false");
		firstMap = false;
		if (summary.count >= minRivers) references.push_back({entry.alias, std::move(*shapes), summary});
	}
	report += "\n  ],\n";

	const auto baselineSeeds = measure(SurfaceTuning {}, evalSide, firstEvalSeed, evalSeedCount);
	size_t baselineWithRiver = 0;
	for (const auto& s : baselineSeeds) baselineWithRiver += s.rivers.count > 0;
	report += std::format("  \"generator_baseline\": {{\"meander\": 0, \"seeds_with_river\": {}, \"median_tortuosity_permille\": {}, \"median_straight_permille\": {}}},\n", baselineWithRiver, gen(baselineSeeds, true), gen(baselineSeeds, false));

	if (references.size() < 2) {
		report += "  \"folds\": [],\n  \"gate\": {\"result\": \"not possible: fewer than two reference maps with at least 5 river components\"}\n}\n";
		std::ofstream f(outPath, std::ios::binary);
		f << report;
		return f ? 0 : 4;
	}

	report += "  \"folds\": [\n";
	size_t foldCount = 0, foldsPassing = 0;
	bool firstFold = true;
	for (const Reference& train : references) {
		for (const Reference& test : references) {
			if (&train == &test) continue;
			const uint32_t meander = fitMeander(train.summary);
			SurfaceTuning candidateTuning;
			candidateTuning.meander = meander;
			const auto candidate = measure(candidateTuning, evalSide, firstEvalSeed, evalSeedCount);
			std::vector<int64_t> dT, dS, p1Base, p1Candidate;
			size_t paired = 0, invariantFailures = 0;
			for (size_t k = 0; k < evalSeedCount; ++k) {
				invariantFailures += !candidate[k].reachesWater;
				if (baselineSeeds[k].walkable && candidate[k].walkable) { p1Base.push_back(*baselineSeeds[k].walkable); p1Candidate.push_back(*candidate[k].walkable); }
				if (!baselineSeeds[k].rivers.count || !candidate[k].rivers.count) continue;
				++paired;
				dT.push_back(std::llabs(int64_t(candidate[k].rivers.tortuosityPermille) - int64_t(test.summary.tortuosityPermille)) - std::llabs(int64_t(baselineSeeds[k].rivers.tortuosityPermille) - int64_t(test.summary.tortuosityPermille)));
				dS.push_back(std::llabs(int64_t(candidate[k].rivers.straightPermille) - int64_t(test.summary.straightPermille)) - std::llabs(int64_t(baselineSeeds[k].rivers.straightPermille) - int64_t(test.summary.straightPermille)));
			}
			const Summary t = summarize(dT), s = summarize(dS);
			const int64_t p1b = medianOf(p1Base), p1c = medianOf(p1Candidate);
			const bool p1Ok = !p1Base.empty() && p1c * 100 >= p1b * 95;
			const bool pass = std::string(t.verdict) == "better" && std::string(s.verdict) == "better" && p1Ok && invariantFailures == 0;
			++foldCount;
			foldsPassing += pass;
			report += std::format("{}    {{\"train\": {}, \"test\": {}, \"fitted_meander\": {}, \"seeds_with_river_in_both\": {}, \"reference_test\": {{\"tortuosity\": {}, \"straight\": {}}}, \"candidate_median\": {{\"tortuosity\": {}, \"straight\": {}}}, \"tortuosity_distance_change\": {}, \"straight_distance_change\": {}, \"P1_baseline_median_ppm\": {}, \"P1_candidate_median_ppm\": {}, \"P1_at_least_95_percent\": {}, \"river_invariant_failures\": {}, \"pass\": {}}}",
				firstFold ? "" : ",\n", quote(train.alias), quote(test.alias), meander, paired, test.summary.tortuosityPermille, test.summary.straightPermille,
				gen(candidate, true), gen(candidate, false), summaryJson(t), summaryJson(s), p1b, p1c, p1Ok ? "true" : "false", invariantFailures, pass ? "true" : "false");
			firstFold = false;
		}
	}
	report += "\n  ],\n";

	// Deployment value: fitted on all qualifying reference maps pooled.
	std::vector<RiverComponent> pooled;
	for (const Reference& r : references) pooled.insert(pooled.end(), r.shapes.rivers.begin(), r.shapes.rivers.end());
	const RiverSummary pooledSummary = summarizeRivers(pooled);
	report += std::format("  \"pooled_reference\": {{\"rivers\": {}, \"tortuosity_permille\": {}, \"straight_permille\": {}, \"fitted_meander\": {}}},\n", pooledSummary.count, pooledSummary.tortuosityPermille, pooledSummary.straightPermille, fitMeander(pooledSummary));
	report += std::format("  \"gate\": {{\"folds\": {}, \"folds_passing\": {}, \"result\": {}}}\n}}\n", foldCount, foldsPassing, quote(foldsPassing == foldCount ? "met in the measured scope" : "not met"));
	std::ofstream f(outPath, std::ios::binary);
	f << report;
	return f ? 0 : 4;
}
