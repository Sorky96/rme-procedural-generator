// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rme::procedural {

// Counts of vegetation by context (plan G4-D). "Within reach" means at most decorReach 4-neighbour
// steps, through any cell, from a seed cell: water, shore or river for water, rock for rock.
inline constexpr uint32_t decorReach = 2;

struct DecorCell {
	uint64_t tiles = 0;   // grass or dirt tiles in this context
	uint64_t trees = 0;   // of them with a tree / bush / stone doodad (a tile can have several)
	uint64_t bushes = 0;
	uint64_t stones = 0;
	bool operator==(const DecorCell&) const = default;
};

struct DecorContext {
	// Index: (ground * 2 + nearWater) * 2 + nearRock, ground 0 = Grass, 1 = Dirt.
	std::array<DecorCell, 8> cell {};
	[[nodiscard]] static constexpr size_t index(bool dirt, bool nearWater, bool nearRock) noexcept {
		return ((dirt ? 2u : 0u) + (nearWater ? 1u : 0u)) * 2u + (nearRock ? 1u : 0u);
	}
	bool operator==(const DecorContext&) const = default;
};

} // namespace rme::procedural
