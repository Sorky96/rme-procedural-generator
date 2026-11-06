// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <random>
#include <span>
#include <stop_token>
#include <vector>

namespace rme::procedural {

// Version 1: row-major cells, one terrain roll per cell (including exclusions).
inline constexpr uint32_t generationAlgorithmVersion = 1;
enum class Terrain : uint8_t { Preserve = 0, Grass = 1, Dirt = 2, Water = 3 };

struct GenerationRegion {
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	bool operator==(const GenerationRegion&) const = default;
};

struct GenerationConfig {
	GenerationRegion region;
	uint64_t seed = 0;
	uint32_t grassWeight = 70;
	uint32_t dirtWeight = 20;
	uint32_t waterWeight = 10;
};

// Exclusive bounds supplied by the adapter from the actual map/client limits.
struct MapExtent { uint32_t width = 0; uint32_t height = 0; uint32_t layers = 0; };

// Core output only. Snapshot, halo, resolver and Undo need separate accounting
// before the future adapter can accept a complete generation job.
struct CoreBudget { uint64_t maxCells = 0; uint64_t maxOutputBytes = 0; };
struct GenerationLayout { size_t cellCount = 0; size_t outputBytes = 0; };
enum class GenerationError {
	InvalidRegion, InvalidWeights, InvalidMask, NoWritableCells,
	CellLimit, OutputLimit, SizeOverflow, Cancelled, AllocationFailed
};

// Independent local streams; never seeds the editor's brush RNG. seed_seq and
// mt19937_64 define the engine sequence; bounded() avoids library distributions.
class StableRandom {
public:
	StableRandom(uint64_t seed, uint64_t stream);
	uint64_t next();
	// Throws std::invalid_argument for an empty range.
	uint64_t bounded(uint64_t exclusiveUpperBound);
private:
	std::mt19937_64 engine;
};

struct SemanticGrid {
	GenerationRegion region;
	std::vector<Terrain> cells;
	[[nodiscard]] bool valid() const noexcept;
	uint64_t fingerprint() const;
};

// Empty mask = whole rectangle; empty protection = no protected cells.
// Nonempty spans must contain exactly width*height bytes, each either 0 or 1.
// Inputs are borrowed read-only for the duration of this synchronous call.
std::expected<GenerationLayout, GenerationError> validateGeneration(
	const GenerationConfig& config, MapExtent extent, CoreBudget budget,
	std::span<const uint8_t> mask = {}, std::span<const uint8_t> protection = {},
	std::stop_token stop = {});

std::expected<SemanticGrid, GenerationError> generateTerrain(
	const GenerationConfig& config, MapExtent extent, CoreBudget budget,
	std::span<const uint8_t> mask = {}, std::span<const uint8_t> protection = {},
	std::stop_token stop = {});

} // namespace rme::procedural
