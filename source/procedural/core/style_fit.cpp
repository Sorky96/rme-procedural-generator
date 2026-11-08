// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_fit.h"

#include <algorithm>
#include <cstdlib>
#include <new>

namespace rme::procedural {
namespace {

// Fixed search grids, in the order they are tried. Chosen to span the validated ranges of
// SurfaceTuning with the manual default inside each grid.
constexpr uint32_t featureSizeGrid[] {8, 12, 16, 24, 32, 48, 64};
constexpr uint32_t lakeGrid[] {0, 20, 40, 60, 100, 150, 200, 300};
constexpr uint32_t riverGrid[] {0, 1, 2, 3, 5, 8};
constexpr uint32_t shoreGrid[] {0, 1, 2, 3, 4};
constexpr uint32_t rockGrid[] {0, 20, 40, 80, 120, 160, 240, 320};
constexpr uint32_t dirtGrid[] {0, 100, 200, 300, 450, 600, 800, 1000};
constexpr uint32_t transitionGrid[] {1000, 3000, 6000, 10000, 16000};

struct Axis {
	uint32_t SurfaceTuning::* field;
	std::span<const uint32_t> grid;
};

const Axis axes[] {
	{&SurfaceTuning::lakePermille, lakeGrid}, {&SurfaceTuning::rockPermille, rockGrid}, {&SurfaceTuning::dirtPermille, dirtGrid},
	{&SurfaceTuning::shoreWidth, shoreGrid}, {&SurfaceTuning::riverCount, riverGrid},
	{&SurfaceTuning::transitionWidth, transitionGrid}, {&SurfaceTuning::featureSize, featureSizeGrid},
};

} // namespace

StyleKind styleKindOf(SurfaceKind kind) noexcept {
	switch (kind) {
		case SurfaceKind::Grass: return StyleKind::Grass;
		case SurfaceKind::Dirt: return StyleKind::Dirt;
		case SurfaceKind::Rock: return StyleKind::Rock;
		case SurfaceKind::Shore: return StyleKind::Shore;
		case SurfaceKind::Lake:
		case SurfaceKind::River: return StyleKind::Water;
		case SurfaceKind::Preserve: break;
	}
	return StyleKind::NoGround;
}

StyleStats statsOfSurface(const SurfaceGrid& grid) {
	StyleStats stats;
	const size_t w = grid.region.width, h = grid.region.height;
	auto exists = [&](size_t i) { return grid.cells[i] != SurfaceKind::Preserve; };
	for (size_t y = 0; y < h; ++y) {
		for (size_t x = 0; x < w; ++x) {
			const size_t i = y * w + x;
			if (!exists(i)) continue;
			const size_t a = static_cast<size_t>(styleKindOf(grid.cells[i]));
			++stats.kindTiles[a];
			if (x + 1 < w && exists(i + 1)) ++stats.pairs[a][static_cast<size_t>(styleKindOf(grid.cells[i + 1]))];
			if (y + 1 < h && exists(i + w)) ++stats.pairs[a][static_cast<size_t>(styleKindOf(grid.cells[i + w]))];
		}
	}
	return stats;
}

std::expected<StyleStats, FitError> evaluateTuning(const SurfaceTuning& tuning, const EvalSpec& spec, std::stop_token stop) {
	if (spec.side < 8 || spec.side > 2048 || spec.seeds.empty()) return std::unexpected(FitError::InvalidSpec);
	const uint64_t cells = static_cast<uint64_t>(spec.side) * spec.side;
	const SurfaceBudget budget {cells, cells * 8, cells * 64 + (1u << 20)};
	StyleStats total;
	try {
		for (const uint64_t seed : spec.seeds) {
			if (stop.stop_requested()) return std::unexpected(FitError::Cancelled);
			SurfaceConfig config {{100, 100, 7, spec.side, spec.side}, seed, tuning};
			const auto grid = generateSurface(config, MapExtent {65001, 65001, 16}, budget, {}, {}, stop);
			if (!grid) return std::unexpected(grid.error() == SurfaceError::Cancelled ? FitError::Cancelled : FitError::Generator);
			const StyleStats one = statsOfSurface(*grid);
			for (size_t a = 0; a < styleKindCount; ++a) {
				total.kindTiles[a] += one.kindTiles[a];
				for (size_t b = 0; b < styleKindCount; ++b) total.pairs[a][b] += one.pairs[a][b];
			}
		}
	} catch (const std::bad_alloc&) {
		return std::unexpected(FitError::AllocationFailed);
	}
	return total;
}

TargetDistance distance(const StyleTarget& a, const StyleTarget& b) noexcept {
	auto diff = [](uint32_t x, uint32_t y) { return static_cast<uint64_t>(x > y ? x - y : y - x); };
	TargetDistance d;
	for (size_t i = 0; i < naturalKindCount; ++i) {
		d.shares += diff(a.sharesPpm[i], b.sharesPpm[i]);
		for (size_t j = 0; j < naturalKindCount; ++j) d.adjacency += diff(a.adjacencyPpm[i][j], b.adjacencyPpm[i][j]);
	}
	return d;
}

namespace {

// Distance of what `tuning` generates to `target`; an empty natural output counts as the worst case.
std::expected<TargetDistance, FitError> measure(const SurfaceTuning& tuning, const StyleTarget& target, const EvalSpec& spec, std::stop_token stop) {
	const auto stats = evaluateTuning(tuning, spec, stop);
	if (!stats) return std::unexpected(stats.error());
	const auto generated = targetFromStats(*stats);
	if (!generated) return TargetDistance {2'000'000, 2'000'000};
	return distance(*generated, target);
}

} // namespace

std::expected<std::vector<FitStep>, FitError> fitTuning(
	const StyleTarget& train, const StyleTarget& validation, const SurfaceTuning& start,
	const FitOptions& options, std::stop_token stop) {
	try {
		std::vector<FitStep> steps;
		SurfaceTuning current = start;
		auto record = [&]() -> std::expected<void, FitError> {
			const auto t = measure(current, train, options.eval, stop);
			if (!t) return std::unexpected(t.error());
			const auto v = measure(current, validation, options.eval, stop);
			if (!v) return std::unexpected(v.error());
			steps.push_back({current, *t, *v});
			return {};
		};
		if (auto r = record(); !r) return std::unexpected(r.error());
		for (uint32_t pass = 0; pass < options.maxPasses; ++pass) {
			bool changed = false;
			uint64_t best = steps.back().train.total();
			for (const Axis& axis : axes) {
				const uint32_t keep = current.*axis.field;
				uint32_t chosen = keep;
				for (const uint32_t value : axis.grid) {
					if (value == keep) continue;
					SurfaceTuning candidate = current;
					candidate.*axis.field = value;
					const auto d = measure(candidate, train, options.eval, stop);
					if (!d) return std::unexpected(d.error());
					if (d->total() < best) { best = d->total(); chosen = value; }
				}
				if (chosen != keep) { current.*axis.field = chosen; changed = true; }
			}
			if (!changed) break;
			if (auto r = record(); !r) return std::unexpected(r.error());
		}
		return steps;
	} catch (const std::bad_alloc&) {
		return std::unexpected(FitError::AllocationFailed);
	}
}

size_t selectStep(std::span<const FitStep> steps) noexcept {
	size_t best = 0;
	for (size_t i = 1; i < steps.size(); ++i) {
		if (steps[i].validation.total() < steps[best].validation.total()) best = i;
	}
	return best;
}

} // namespace rme::procedural
