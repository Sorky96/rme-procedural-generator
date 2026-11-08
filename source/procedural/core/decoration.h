// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/surface.h"

namespace rme::procedural {

// Single-tile vegetation and detail kinds placed on top of the surface ground. Pure function of
// (seed, absolute coordinates, the surface grid), integer math only. Only Grass and Dirt cells
// ever receive a kind; water, shore, rock and Preserve cells stay None. Water and rock distances
// look at the grid as given, so cells next to a Preserve cell treat it as plain land.
enum class DecorKind : uint8_t { None = 0, Tree, Bush, Stone };
inline constexpr size_t decorKindCount = 3;

struct DecorGrid {
	GenerationRegion region;
	std::vector<DecorKind> cells;
	[[nodiscard]] bool valid() const noexcept;
};

// Chances in permille of a cell that is eligible. The defaults are the generator's built-in rules, so
// generating with a default DecorTuning is identical to generating without one.
struct DecorTuning {
	uint32_t treeScalePermille = 1000;  // multiplies the forest and the clearing tree chance (1000 = as built in)
	uint32_t waterTreePermille = 0;     // tree chance on grass within reach of water, shore or river
	uint32_t bushPermille = 40;         // bush chance on grass elsewhere
	uint32_t waterBushPermille = 60;    // bush chance on grass within reach of water
	uint32_t dirtStonePermille = 30;    // stone chance on dirt away from rock
	uint32_t rockStonePermille = 80;    // stone chance on dirt (and stone window on grass) within reach of rock
	bool operator==(const DecorTuning&) const = default;
};
inline constexpr uint32_t maxTreeScalePermille = 9000;

// Working memory estimate (flags and output), not a measurement.
inline constexpr uint64_t decorationBytesPerCell = 4;

std::expected<DecorGrid, SurfaceError> generateDecoration(
	const SurfaceGrid& grid, uint64_t seed, std::stop_token stop = {});

// A chance above 1000 (treeScalePermille above maxTreeScalePermille) is SurfaceError::InvalidParameters.
std::expected<DecorGrid, SurfaceError> generateDecoration(
	const SurfaceGrid& grid, uint64_t seed, const DecorTuning& tuning, std::stop_token stop = {});

} // namespace rme::procedural
