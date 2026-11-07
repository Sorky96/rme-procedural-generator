// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/generation_input.h"
#include "procedural/adapter/ground_resolver.h"
#include "procedural/core/decoration.h"
#include "procedural/core/surface.h"

namespace rme::procedural {

// Resolves a surface grid into the same GroundPlan the rest of the pipeline already consumes.
// profile.count must be GroundProfile::surfaceCount, with names in SurfaceKind order after
// Preserve: Grass, Dirt, Rock, Shore, Lake, River. GUI thread only. Placement only: borders are
// produced later by the existing GroundBorderCalculator, never here. The variant seed is
// input.config.seed, and the grid must come from the same region and seed. With
// input.config.vegetation, decor must be the DecorGrid of that grid and profile.decor must name
// three doodad brushes with single-tile items; otherwise decor must be null and decorCount 0.
// keepClear (empty or one byte per cell) marks writable cells the grid left as Preserve on purpose;
// they get no item, and every other writable cell must still be placed.
std::expected<GroundPlan, ResolveError> resolveSurfacePlan(
	const GenerationInput& input, const SurfaceGrid& grid, const GroundProfile& profile,
	uint64_t maxPlanBytes, std::stop_token stop = {}, const DecorGrid* decor = nullptr,
	std::span<const uint8_t> keepClear = {});

} // namespace rme::procedural
