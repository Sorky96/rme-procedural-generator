// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/style_profile.h"
#include "procedural/core/surface.h"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

namespace rme::procedural {

// Fit of the surface generator's tuning to a style target (plan 4.5, level L1): coordinate descent
// over fixed discrete grids, measured on generated maps. Everything is integer math and a pure
// function of (tuning, side, seeds), so a fit is repeatable byte for byte.

// Kinds the generator produces, projected to the profile's: Lake and River are Water; Preserve cells
// are not tiles and are skipped.
[[nodiscard]] StyleKind styleKindOf(SurfaceKind kind) noexcept;

// Tile and neighbour-pair counts of a generated grid, in the same form as a reference map's.
[[nodiscard]] StyleStats statsOfSurface(const SurfaceGrid& grid);

struct EvalSpec {
	uint32_t side = 160;               // generated square of side x side cells
	std::vector<uint64_t> seeds;       // one generated map per seed, counts are summed
};

enum class FitError : uint8_t { InvalidSpec, Generator, Cancelled, AllocationFailed };

// Summed counts over the seeds. Generator failures (invalid tuning) are reported, never skipped.
[[nodiscard]] std::expected<StyleStats, FitError> evaluateTuning(
	const SurfaceTuning& tuning, const EvalSpec& spec, std::stop_token stop);

// Share of walkable cells (Grass, Dirt, Shore) that lie in the largest 4-connected walkable component,
// in ppm; nullopt when the grid has no walkable cell. Lake, River, Rock and Preserve are not walkable.
[[nodiscard]] std::optional<uint32_t> walkableConnectivityPpm(const SurfaceGrid& grid);

// L1 distances between two targets, in ppm: shares over the 5 kinds, adjacency over the 25 cells.
struct TargetDistance {
	uint64_t shares = 0;
	uint64_t adjacency = 0;
	[[nodiscard]] uint64_t total() const noexcept { return shares + adjacency; }
	bool operator==(const TargetDistance&) const = default;
};
[[nodiscard]] TargetDistance distance(const StyleTarget& a, const StyleTarget& b) noexcept;

// Median over the seeds of the walkable connectivity of the generated grids, in ppm (seeds without a
// walkable cell are skipped; none at all gives 0).
[[nodiscard]] std::expected<uint32_t, FitError> connectivityMedianPpm(
	const SurfaceTuning& tuning, const EvalSpec& spec, std::stop_token stop);

struct FitOptions {
	EvalSpec eval;
	uint32_t maxPasses = 4;
	// 0 = no constraint. Otherwise a candidate value is rejected when the median connectivity of what it
	// generates on the eval seeds is below this percentage of the start tuning's median.
	uint32_t minConnectivityPercent = 0;
};

struct FitStep {
	SurfaceTuning tuning;
	TargetDistance train;
	TargetDistance validation;
	uint32_t connectivityPpm = 0; // median over the eval seeds, measured only when the constraint is on
};

// steps[0] is `start`; every further step is one full pass over the parameters. The search stops when
// a pass changes nothing or after maxPasses. A parameter keeps its value unless another grid value is
// strictly better on the train target. The validation target is only measured, never searched on.
[[nodiscard]] std::expected<std::vector<FitStep>, FitError> fitTuning(
	const StyleTarget& train, const StyleTarget& validation, const SurfaceTuning& start,
	const FitOptions& options, std::stop_token stop);

// Index of the step with the smallest validation distance; the earliest on a tie, so the start wins ties.
[[nodiscard]] size_t selectStep(std::span<const FitStep> steps) noexcept;

} // namespace rme::procedural
