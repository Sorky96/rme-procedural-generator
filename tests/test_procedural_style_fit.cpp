// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_fit.h"

#include <iostream>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

size_t K(StyleKind k) { return static_cast<size_t>(k); }

void testProjection() {
	check(styleKindOf(SurfaceKind::Lake) == StyleKind::Water && styleKindOf(SurfaceKind::River) == StyleKind::Water, "lake and river are water");
	check(styleKindOf(SurfaceKind::Grass) == StyleKind::Grass && styleKindOf(SurfaceKind::Dirt) == StyleKind::Dirt &&
	      styleKindOf(SurfaceKind::Rock) == StyleKind::Rock && styleKindOf(SurfaceKind::Shore) == StyleKind::Shore, "direct kinds");

	// G G L      Preserve cells are not tiles and break pairs.
	// R P S
	SurfaceGrid grid;
	grid.region = {0, 0, 7, 3, 2};
	grid.cells = {SurfaceKind::Grass, SurfaceKind::Grass, SurfaceKind::Lake, SurfaceKind::River, SurfaceKind::Preserve, SurfaceKind::Shore};
	const StyleStats s = statsOfSurface(grid);
	check(s.kindTiles[K(StyleKind::Grass)] == 2 && s.kindTiles[K(StyleKind::Water)] == 2 && s.kindTiles[K(StyleKind::Shore)] == 1, "tiles per kind");
	check(s.pairs[K(StyleKind::Grass)][K(StyleKind::Grass)] == 1 && s.pairs[K(StyleKind::Grass)][K(StyleKind::Water)] == 2 &&
	      s.pairs[K(StyleKind::Water)][K(StyleKind::Shore)] == 1, "pairs east and south");
	uint64_t pairs = 0;
	for (const auto& row : s.pairs) for (const uint64_t v : row) pairs += v;
	check(pairs == 4, "no pair with a preserved cell");
}

void testConnectivity() {
	// G D W S     walkable: 4 cells in two components (3 and 1) -> 750000;
	// G R W K     (R = River, K = Rock); water, rock and Preserve are not walkable.
	SurfaceGrid grid;
	grid.region = {0, 0, 7, 4, 2};
	grid.cells = {SurfaceKind::Grass, SurfaceKind::Dirt, SurfaceKind::Lake, SurfaceKind::Shore,
		SurfaceKind::Grass, SurfaceKind::River, SurfaceKind::Lake, SurfaceKind::Rock};
	check(walkableConnectivityPpm(grid) == 750000, "largest component is G D G (3 of 4 walkable cells)");
	grid.cells[4] = SurfaceKind::Preserve;
	check(walkableConnectivityPpm(grid) == 666666, "a preserved cell is not walkable: components of 2 and 1");
	grid.cells = std::vector<SurfaceKind>(8, SurfaceKind::Lake);
	check(!walkableConnectivityPpm(grid).has_value(), "no walkable cell: nullopt");
	grid.cells = std::vector<SurfaceKind>(8, SurfaceKind::Grass);
	check(walkableConnectivityPpm(grid) == 1'000'000, "one component: everything");
	grid.cells = {SurfaceKind::Grass, SurfaceKind::Lake, SurfaceKind::Grass, SurfaceKind::Lake,
		SurfaceKind::Lake, SurfaceKind::Grass, SurfaceKind::Lake, SurfaceKind::Grass};
	check(walkableConnectivityPpm(grid) == 250000, "diagonal neighbours do not connect: four singletons");
}

void testDistance() {
	StyleTarget a, b;
	a.sharesPpm = {500000, 500000, 0, 0, 0};
	b.sharesPpm = {400000, 300000, 100000, 100000, 100000};
	a.adjacencyPpm[0][0] = 1000;
	b.adjacencyPpm[0][0] = 4000;
	const TargetDistance d = distance(a, b);
	check(d.shares == 100000 + 200000 + 100000 + 100000 + 100000 && d.adjacency == 3000, "L1 distances");
	check(distance(a, b) == distance(b, a) && distance(a, a).total() == 0, "symmetric, zero on itself");
}

void testEvaluate() {
	SurfaceTuning t;
	EvalSpec spec {64, {1, 2}};
	const auto both = evaluateTuning(t, spec, {});
	const auto one = evaluateTuning(t, {64, {1}}, {});
	const auto two = evaluateTuning(t, {64, {2}}, {});
	check(both && one && two, "evaluates");
	if (!(both && one && two)) return;
	bool summed = true;
	for (size_t a = 0; a < styleKindCount; ++a) summed = summed && both->kindTiles[a] == one->kindTiles[a] + two->kindTiles[a];
	check(summed, "counts are summed over seeds");
	uint64_t tiles = 0;
	for (const uint64_t v : both->kindTiles) tiles += v;
	check(tiles == 2 * 64 * 64, "every cell of an unmasked map is a tile");
	check(*evaluateTuning(t, spec, {}) == *both, "repeatable");
	check(evaluateTuning(t, {64, {}}, {}).error() == FitError::InvalidSpec, "no seeds refused");
	check(evaluateTuning(t, {4, {1}}, {}).error() == FitError::InvalidSpec, "tiny side refused");
	SurfaceTuning bad;
	bad.lakePermille = 900;
	check(evaluateTuning(bad, spec, {}).error() == FitError::Generator, "invalid tuning is a generator error, not skipped");
	std::stop_source source;
	source.request_stop();
	check(evaluateTuning(t, spec, source.get_token()).error() == FitError::Cancelled, "cancelled");
}

void testFit() {
	// The target is what a known tuning generates on the same seeds, so a better tuning exists on the grids.
	SurfaceTuning known;
	known.lakePermille = 200;
	known.rockPermille = 160;
	known.dirtPermille = 600;
	const FitOptions options {{64, {1, 2}}, 3};
	const auto stats = evaluateTuning(known, options.eval, {});
	const auto target = targetFromStats(*stats);
	check(stats && target, "target from the known tuning");
	if (!target) return;
	const auto steps = fitTuning(*target, *target, SurfaceTuning {}, options, {});
	check(steps.has_value() && steps->size() >= 2, "at least one pass improved something");
	if (!steps) return;
	check((*steps)[0].tuning == SurfaceTuning {}, "step 0 is the start");
	bool monotone = true;
	for (size_t i = 1; i < steps->size(); ++i) monotone = monotone && (*steps)[i].train.total() <= (*steps)[i - 1].train.total();
	check(monotone, "train distance never increases");
	check(steps->back().train.total() < steps->front().train.total(), "the fit gets closer than the start");
	check(steps->back().train.total() == 0 || steps->back().train.total() * 2 < steps->front().train.total(), "...and by a large margin on an attainable target");
	const auto again = fitTuning(*target, *target, SurfaceTuning {}, options, {});
	check(again && again->size() == steps->size() && again->back().tuning == steps->back().tuning && again->back().train == steps->back().train, "repeatable fit");
	check(selectStep(*steps) == steps->size() - 1, "validation = train: the last step wins");

	// Start already optimal: nothing changes, one step.
	const auto fixed = fitTuning(*target, *target, known, options, {});
	check(fixed && fixed->size() == 1 && (*fixed)[0].train.total() == 0, "an optimal start stays put");
	std::stop_source source;
	source.request_stop();
	check(fitTuning(*target, *target, SurfaceTuning {}, options, source.get_token()).error() == FitError::Cancelled, "fit cancelled");
}

void testConnectivityConstraint() {
	// A target that rewards a lot of water and rock. With the constraint at 100 percent no step may have a
	// lower connectivity than the start, which the unconstrained fit does not respect for this target.
	SurfaceTuning wet;
	wet.lakePermille = 300;
	wet.rockPermille = 320;
	wet.riverCount = 8;
	FitOptions options {{96, {1, 2, 3}}, 3, 0};
	const auto stats = evaluateTuning(wet, options.eval, {});
	const auto target = targetFromStats(*stats);
	check(stats && target, "target from a wet tuning");
	if (!target) return;
	const auto free = fitTuning(*target, *target, SurfaceTuning {}, options, {});
	check(free.has_value(), "unconstrained fit");
	options.minConnectivityPercent = 100;
	const auto bound = fitTuning(*target, *target, SurfaceTuning {}, options, {});
	check(bound.has_value(), "constrained fit");
	if (!free || !bound) return;
	const uint32_t startP1 = *connectivityMedianPpm(SurfaceTuning {}, options.eval, {});
	for (const FitStep& step : *bound) check(step.connectivityPpm >= startP1, "every step keeps at least the start connectivity");
	check((*bound)[0].tuning == SurfaceTuning {} && (*bound)[0].connectivityPpm == startP1, "step 0 is the start, with its connectivity recorded");
	check(*connectivityMedianPpm(free->back().tuning, options.eval, {}) < startP1, "this target does lower the connectivity when nothing stops the fit");
	check(bound->back().train.total() > free->back().train.total(), "the constraint costs distance");
	check(bound->back().train.total() <= bound->front().train.total(), "...but never more than the start");
	check((*free)[0].connectivityPpm == 0, "connectivity is not measured when the constraint is off");
	const auto again = fitTuning(*target, *target, SurfaceTuning {}, options, {});
	check(again && again->back().tuning == bound->back().tuning, "repeatable");
	check(connectivityMedianPpm(SurfaceTuning {}, {96, {}}, {}).error() == FitError::InvalidSpec, "no seeds refused");
	// A start that fails the constraint on its own (everything is water) cannot be improved into a pass.
	SurfaceTuning flooded;
	flooded.lakePermille = 300;
	flooded.riverCount = 8;
	check(*connectivityMedianPpm(flooded, options.eval, {}) <= 1'000'000, "connectivity is a ppm value");
}

void testSelect() {
	auto step = [](uint64_t validation) {
		FitStep s {};
		s.validation.shares = validation;
		return s;
	};
	const std::vector<FitStep> steps {step(500), step(300), step(300), step(900)};
	check(selectStep(steps) == 1, "smallest validation distance, earliest on a tie");
	check(selectStep(std::vector<FitStep> {step(10), step(20)}) == 0, "the start wins when nothing beats it");
	check(selectStep({}) == 0, "empty list is index 0");
}

} // namespace

int main() {
	testProjection();
	testDistance();
	testConnectivity();
	testConnectivityConstraint();
	testEvaluate();
	testSelect();
	testFit();
	if (failures == 0) std::cout << "OK\n";
	return failures == 0 ? 0 : 1;
}
