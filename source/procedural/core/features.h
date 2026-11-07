// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/semantic_map.h"
#include "procedural/core/topology.h"

#include <array>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <vector>

namespace rme::procedural {

// Measurable features of a reference map (plan section 4.2). Counts are integers; ratios are
// derived by the report renderers in integer permille so output does not depend on the FPU or locale.

inline constexpr std::array<uint32_t, 3> blockScales {16, 32, 64};
inline constexpr size_t fractionBins = 10;     // decile histogram, bin 9 includes 100 %
inline constexpr size_t distanceBinCount = 6;  // d=1, 2, 3-4, 5-8, 9-16, 17+ or no source on the floor

struct FeatureOptions {
	// The adjacency matrix is dense (keys^2 per direction); more ground keys than this is refused.
	uint32_t maxKeysForAdjacency = 512;
};

// Neighbour pairs of existing tiles. east[a * keyCount + b]: a at (x, y) and b at (x + 1, y);
// south likewise for (x, y + 1). West and north are the transposes, so they are not stored.
struct AdjacencyMatrix {
	size_t keyCount = 0;
	std::vector<uint64_t> east;
	std::vector<uint64_t> south;
};

// Aligned absolute blocks of scale x scale cells that contain at least one tile. Each histogram
// counts blocks by decile of: occupancy (tiles / scale^2), walkable share of the block's tiles, and
// share of the block's tiles on impassable ground.
struct ScaleStats {
	uint32_t scale = 0;
	uint64_t blocks = 0;
	std::array<uint64_t, fractionBins> occupancy {};
	std::array<uint64_t, fractionBins> walkableShare {};
	std::array<uint64_t, fractionBins> impassableShare {};
};

struct FloorFeature {
	int z = 0;
	uint64_t tiles = 0;
	std::array<uint64_t, traitCount> traitTiles {};
};

// Tiles that are not themselves impassable ground, binned by 4-neighbour distance (capped) to the
// nearest impassable-ground tile on the same floor. Rates of a trait in a bin = trait / tiles.
struct DistanceFeature {
	std::array<uint64_t, distanceBinCount> tiles {};
	std::array<uint64_t, distanceBinCount> doodad {};
	std::array<uint64_t, distanceBinCount> wall {};
	std::array<uint64_t, distanceBinCount> border {};
};

struct FeatureSet {
	AdjacencyMatrix adjacency;
	std::array<ScaleStats, blockScales.size()> scales {};
	std::vector<FloorFeature> floors;
	DistanceFeature distance;
};

enum class FeatureError : uint8_t { TooManyKeys, Cancelled, AllocationFailed, InvalidInput };

// `topology.keys` supplies which ground keys are impassable. Checks stop_requested once per row
// and every few thousand queue pops. Working memory about 5 bytes per cell of the largest floor.
[[nodiscard]] std::expected<FeatureSet, FeatureError> extractFeatures(
	const SemanticMap& map, const TopologyReport& topology, const FeatureOptions& options, std::stop_token stop);

} // namespace rme::procedural
