// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/style_profile.h"
#include "procedural/core/surface.h"

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>

namespace rme::procedural {

// Large-scale structure (G4 iteration 6): how often two natural-kind cells d apart have the same kind, for
// several d, and how much that exceeds a random layout with the same kind shares. Integer math only; the
// definitions are fixed in docs/procedural/G4_METRICS.md.
inline constexpr std::array<uint32_t, 3> scaleLags {4, 16, 64};

struct LagStats {
	std::array<uint64_t, 3> same {};   // pairs (horizontal and vertical) with equal kinds, per lag
	std::array<uint64_t, 3> pairs {};  // pairs where both cells are natural kinds, per lag
	std::array<uint64_t, 5> cells {};  // natural cells per kind: Grass, Dirt, Rock, Shore, Water
	bool operator==(const LagStats&) const = default;
	LagStats& operator+=(const LagStats& other) noexcept;
};

// `kinds` has width * height cells: 0..4 = Grass, Dirt, Rock, Shore, Water; anything else is skipped.
void addLagStats(std::span<const uint8_t> kinds, uint32_t width, uint32_t height, LagStats& out);

// Reference side: floors zMin..zMax through `keymap`; absent tiles are skipped.
[[nodiscard]] std::expected<LagStats, StatsError> lagStatsOf(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop);
// Generator side: Lake and River count as Water, Preserve is skipped.
[[nodiscard]] LagStats lagStatsOf(const SurfaceGrid& grid);

// Excess over chance in permille per lag: 1000 * same / pairs - 1000 * sum(share^2). 0 when there are no pairs.
[[nodiscard]] std::array<int32_t, 3> lagExcessPermille(const LagStats& stats);

} // namespace rme::procedural
