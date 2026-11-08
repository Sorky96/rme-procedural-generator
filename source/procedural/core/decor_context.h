// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/decor_stats.h"
#include "procedural/core/decoration.h"
#include "procedural/core/style_fit.h"
#include "procedural/core/style_profile.h"

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>

namespace rme::procedural {

// Context rules of vegetation (plan G4-D): how often trees, bushes and stones stand on grass and
// dirt, split by whether the tile is within reach of water (water, shore, river) and of rock. The
// reference side counts doodads by the name of their brush (classifyDoodadBrush); the generator side
// counts what generateDecoration placed. Both use the same definition of "within reach": at most
// decorReach 4-neighbour steps, through any cell, from a seed cell.

inline constexpr std::string_view decorContextSchema = "g4-decorctx/1";
[[nodiscard]] std::string serializeDecorContext(const DecorContext& context);
[[nodiscard]] std::expected<DecorContext, ProfileError> parseDecorContext(std::string_view text);

void add(DecorContext& total, const DecorContext& more) noexcept;

// Reference side. Water seeds are Water and Shore tiles, rock seeds Rock tiles, through `keymap`;
// a ground key the key map does not list is an error. Checks stop once per floor row.
[[nodiscard]] std::expected<DecorContext, StatsError> extractDecorContext(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop);

// Generator side; `decor` must come from `grid`.
[[nodiscard]] DecorContext decorContextOf(const SurfaceGrid& grid, const DecorGrid& decor);

// Summed over the seeds of `spec`: surface from `surface`, decoration from `decor`.
[[nodiscard]] std::expected<DecorContext, FitError> evaluateDecor(
	const SurfaceTuning& surface, const DecorTuning& decor, const EvalSpec& spec, std::stop_token stop);

// One map's context with the family it belongs to, for building a target from several maps.
struct MapDecor {
	std::string alias;
	std::string family;
	DecorContext context;
};

// Target of several maps in virtual units: per cell, the weighted mean of the per-map rates (every
// family counts the same, every map in it the same, among the maps with at least minTiles tiles in that
// cell), stored as counts over 1 000 000 virtual tiles. A cell no map qualifies for has 0 tiles.
[[nodiscard]] DecorContext buildDecorTarget(std::span<const MapDecor> maps, uint64_t minTiles = 200);

// Mean absolute difference of the 24 rates (8 contexts x tree/bush/stone) in permille, over the
// contexts where the reference has at least minTiles tiles. `terms` counts the rates compared; no
// comparable context is `terms == 0` (not a perfect score).
struct DecorDistance {
	uint64_t sumPermille = 0;
	uint32_t terms = 0;
	bool operator==(const DecorDistance&) const = default;
};
[[nodiscard]] DecorDistance decorDistance(const DecorContext& reference, const DecorContext& generated, uint64_t minTiles = 200) noexcept;

// Sets each chance of the tuning to the reference rate of its context; chances whose context has
// fewer than 200 tiles keep the built-in value. treeScalePermille is the ratio of the reference tree
// rate on grass away from water to what the built-in rules place on the same surface (needs a nonzero
// built-in rate, else stays 1000), clamped to maxTreeScalePermille. Closed form, no search.
[[nodiscard]] std::expected<DecorTuning, FitError> fitDecorTuning(
	const DecorContext& target, const SurfaceTuning& surface, const EvalSpec& spec, std::stop_token stop);

} // namespace rme::procedural
