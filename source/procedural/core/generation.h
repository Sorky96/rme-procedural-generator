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

enum class GenerationAlgorithm : uint8_t { GroundRoll = 1, Surface = 2 };

// Tunables of the surface algorithm (v2), ignored by GroundRoll. Ranges are enforced by
// validateSurface.
// The first eleven members are the ones a style profile stores, in that order (positional initialisation in tests and in
// the profile parser relies on it); the members after them are set by the user in the generator dialog only.
struct SurfaceTuning {
	uint32_t featureSize = 24;        // coarsest noise lattice step in cells, 1..4096
	uint32_t lakePermille = 60;       // share of lowest cells that start as lake, 0..500
	uint32_t minLakeCells = 12;       // smaller 4-connected lakes become land
	uint32_t riverCount = 3;          // sources drawn from rock cells, 0..1024
	uint32_t minRiverLength = 20;     // own cells of an accepted river; shorter ones are dropped
	uint32_t shoreWidth = 3;          // rings of Shore around lakes, 0..8; ring 0 always, outer rings by noise
	uint32_t rockPermille = 80;       // at least this share of highest cells is rock, 0..500
	uint32_t minRockCells = 8;        // smaller 4-connected rock patches become land
	uint32_t dirtPermille = 300;      // share of driest cells that are dirt, 0..1000
	uint32_t transitionWidth = 6000;  // half width of the grass/dirt dithered band, 0..32768
	uint32_t meander = 0;             // river sideways swing in cells, 0..8; 0 keeps the original straight-ish routes
	uint32_t pathLength = 0;          // target skeleton cells of paths per 65536 cells, 0..400; 0 = no paths (not part of a style profile)
	uint32_t buildingDensity = 0;     // target number of building foundations per 65536 cells, 0..100; 0 = none (not part of a style profile)
	uint32_t fordLength = 0;          // longest water crossing (cells) a ford may span, 0..256; 0 = no fords (not part of a style profile)
	uint32_t rockPasses = 0;          // 1 = trails may cross bands of rock (iteration 15); 0 = they go around (not part of a style profile)
	uint32_t shoreSmoothing = 0;      // majority-filter passes that round the sand / grass edge near lakes, 0..4; 0 = as generated (not part of a style profile)
	uint32_t shoreRadius = 0;         // window radius of the shore smoothing, 0..8; 0 = 3 (not part of a style profile)
	uint32_t repairReach = 0;        // 1 = join every component that holds a door or an exit to the start with a ford up to 256 cells long (iteration 23); not part of a style profile
	bool operator==(const SurfaceTuning&) const = default;
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

// Chances above 1000 (treeScalePermille above maxTreeScalePermille) are refused by the generator.
[[nodiscard]] constexpr bool valid(const DecorTuning& t) noexcept {
	return t.treeScalePermille <= maxTreeScalePermille && t.waterTreePermille <= 1000 && t.bushPermille <= 1000 &&
		t.waterBushPermille <= 1000 && t.dirtStonePermille <= 1000 && t.rockStonePermille <= 1000;
}

struct GenerationConfig {
	GenerationRegion region;
	uint64_t seed = 0;
	uint32_t grassWeight = 70;
	uint32_t dirtWeight = 20;
	uint32_t waterWeight = 10;
	GenerationAlgorithm algorithm = GenerationAlgorithm::GroundRoll;
	SurfaceTuning surface;
	// Surface with vegetation only: chances of the doodads placed on top.
	DecorTuning decor;
	// Surface only: adds single-tile trees, bushes and stones from doodad brushes.
	bool vegetation = false;
	bool operator==(const GenerationConfig&) const = default;
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
