// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/surface.h"
#include <array>
#include <cstdint>
#include <span>
#include <stop_token>
#include <vector>

namespace rme::procedural {

inline constexpr uint32_t defaultGrassPatchStep = 10; // lattice step of the grass variant patches, in cells
inline constexpr size_t grassPatchKinds = 3;          // grass mix brushes at most
// Patch noise seed per grass mix brush (XORed with the generation seed).
inline constexpr std::array<uint64_t, grassPatchKinds> grassPatchStreams {0x73325f6d69786772ULL, 0x73325f6d69786773ULL, 0x73325f6d69786774ULL};

// Smooth value noise in 0..65535 on a `step` lattice, anchored to map coordinates so a region and its neighbour
// agree at their edge.
[[nodiscard]] uint32_t grassPatchValue(uint64_t seed, int64_t x, int64_t y, uint32_t step = defaultGrassPatchStep) noexcept;

// Grass mix per cell (0 = the Grass brush, m + 1 = mix m; 0 off the Grass cells). Each mix with a share in turn takes
// the unclaimed Grass cells whose own patch noise lies lowest, as many as its share (permille) of all Grass cells,
// so the shares hold for any region; then a cell with fewer than two 4-neighbours of its own mix joins the mix most
// of its Grass neighbours have (two passes): one-cell strips and spurs draw broken borders. Empty without a share
// or when cancelled.
[[nodiscard]] std::vector<uint8_t> assignGrassPatches(const SurfaceGrid& grid, uint64_t seed,
	std::span<const uint16_t, grassPatchKinds> permille, uint32_t step = defaultGrassPatchStep, std::stop_token stop = {});

} // namespace rme::procedural
