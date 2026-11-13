// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/generation_input.h"
#include "procedural/adapter/ground_resolver.h"
#include "procedural/core/decoration.h"
#include "procedural/core/surface.h"
#include "procedural/core/underground.h"

class DoodadBrush;

namespace rme::procedural {

struct BridgeBrush { uint16_t horizontal = 0, vertical = 0; };
std::expected<BridgeBrush, ResolveError> resolveBridgeBrush(std::string_view name);
struct CaveDoorBrush { uint16_t wall = 0, horizontal = 0, vertical = 0; };
std::expected<CaveDoorBrush, ResolveError> resolveCaveDoorBrush(std::string_view name);

struct CaveExitBrush {
	struct Items { uint16_t ramp = 0, tail = 0, opening = 0; };
	std::vector<ExitShape> shapes;
	std::vector<Items> items;
};

struct CaveBrushes : CaveExitBrush {
	std::vector<uint16_t> entrances;
	uint16_t ground = 0;
	uint16_t wall = 0;
};

[[nodiscard]] bool hasPlateau(const GroundProfile& profile) noexcept;
[[nodiscard]] bool validPlateauProfile(const GroundProfile& profile, const GenerationConfig& config) noexcept;
std::expected<CaveExitBrush, ResolveError> resolvePlateauBrushes(const GroundProfile& profile, const GenerationConfig& config);

// Iteration 43: storeys need walled buildings and a ramp-like stairs doodad (same shape rules as cave exits);
// the stairs name is set exactly when the option is on.
[[nodiscard]] bool validStoreyProfile(const GroundProfile& profile, const GenerationConfig& config) noexcept;
std::expected<CaveExitBrush, ResolveError> resolveBuildingStairsBrush(std::string_view name);

[[nodiscard]] bool hasCave(const GroundProfile& profile) noexcept;
[[nodiscard]] bool validCaveProfile(const GroundProfile& profile, const GenerationConfig& config) noexcept;
// Translates a ramp-like doodad (every alternate a composite: a floor-change tile, a tile on the floor
// above and at most one tail tile) into exit shapes in alternate order. GUI thread, client loaded.
std::expected<CaveExitBrush, ResolveError> resolveCaveExitBrush(const DoodadBrush& exit);
std::expected<CaveBrushes, ResolveError> resolveCaveBrushes(const GroundProfile& profile, const GenerationConfig& config);

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
