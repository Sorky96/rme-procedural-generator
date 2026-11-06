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

enum class SurfaceKind : uint8_t { Preserve = 0, Grass, Dirt, Rock, Shore, Lake, River };

// All fields below are pure functions of (seed, absolute coordinates) and of
// whole-region thresholds. A mask or protection therefore never changes the
// value of another cell. Integer math only, so results do not depend on the FPU.
struct SurfaceConfig {
	GenerationRegion region;
	uint64_t seed = 0;
	uint32_t featureSize = 24;        // coarsest noise lattice step in cells, 1..4096
	uint32_t lakePermille = 60;       // share of lowest cells that start as lake, 0..500
	uint32_t minLakeCells = 12;       // smaller 4-connected lakes become land
	uint32_t riverCount = 3;          // sources drawn from the highest quarter, 0..1024
	uint32_t minRiverLength = 20;     // own cells of an accepted river; shorter ones are dropped
	uint32_t shoreWidth = 1;          // 4-connected ring of Shore around lakes, 0..4
	uint32_t rockPermille = 80;       // at least this share of highest cells is rock, 0..500
	uint32_t minRockCells = 8;        // smaller 4-connected rock patches become land
	uint32_t dirtPermille = 300;      // share of driest cells that are dirt, 0..1000
	uint32_t transitionWidth = 6000;  // half width of the grass/dirt dithered band, 0..32768
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
std::expected<SurfaceLayout, SurfaceError> validateSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget,
	std::span<const uint8_t> mask = {}, std::span<const uint8_t> protection = {},
	std::stop_token stop = {});

std::expected<SurfaceGrid, SurfaceError> generateSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget,
	std::span<const uint8_t> mask = {}, std::span<const uint8_t> protection = {},
	std::stop_token stop = {});

} // namespace rme::procedural
