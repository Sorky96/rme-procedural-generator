// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/generation.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace rme::procedural {

// Surface algorithm v2 is a separate generator from the v1 grass/dirt/water roll.
// v1 types and fingerprints are untouched; v2 has its own kinds and fingerprint domain.
inline constexpr uint32_t surfaceAlgorithmVersion = 2;

enum class SurfaceKind : uint8_t { Preserve = 0, Grass, Dirt, Rock, Shore, Lake, River, Path };

// All fields below are pure functions of (seed, absolute coordinates) and of
// whole-region thresholds. A mask or protection therefore never changes the
// value of another cell. Integer math only, so results do not depend on the FPU.
struct SurfaceConfig {
	GenerationRegion region;
	uint64_t seed = 0;
	SurfaceTuning tuning;
};

// Working memory is an upper-bound estimate, not a measurement.
struct SurfaceBudget { uint64_t maxCells = 0; uint64_t maxOutputBytes = 0; uint64_t maxWorkingBytes = 0; };
struct SurfaceLayout { size_t cellCount = 0; size_t outputBytes = 0; uint64_t workingBytes = 0; };

enum class SurfaceError {
	InvalidRegion, InvalidParameters, InvalidMask, NoWritableCells,
	CellLimit, OutputLimit, WorkingLimit, SizeOverflow, Cancelled, AllocationFailed
};

struct SurfaceGrid {
	GenerationRegion region;
	std::vector<SurfaceKind> cells;
	[[nodiscard]] bool valid() const noexcept;
	uint64_t fingerprint() const;
};

// Empty mask/protection: same rules as validateGeneration. Rivers end at a lake or
// at the border of the generated rectangle, never inside the land.
// Shared integer noise for generators that must agree with the surface fields.
uint32_t surfaceValueNoise(uint64_t seed, uint64_t stream, uint32_t x, uint32_t y, uint32_t step);
uint16_t surfaceLattice(uint64_t seed, uint64_t stream, uint32_t x, uint32_t y);

std::expected<SurfaceLayout, SurfaceError> validateSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget,
	std::span<const uint8_t> mask = {}, std::span<const uint8_t> protection = {},
	std::stop_token stop = {});

std::expected<SurfaceGrid, SurfaceError> generateSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget,
	std::span<const uint8_t> mask = {}, std::span<const uint8_t> protection = {},
	std::stop_token stop = {});

} // namespace rme::procedural
