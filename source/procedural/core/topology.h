// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/semantic_map.h"

#include <array>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <vector>

namespace rme::procedural {

// Regions and connectivity of a SemanticMap. Everything is derived from cell traits and
// ground keys: no item ids and no names. Connectivity is 4-neighbour (diagonals do not join).
struct TopologyOptions {
	// A ground key is impassable when blocking tiles / tiles >= this percentage (1..100).
	uint32_t impassablePercent = 90;
	// Walkable components with fewer cells than this count as small pockets.
	uint32_t smallComponentCells = 16;
};

inline constexpr size_t sizeBucketCount = 24; // bucket i holds component sizes in [2^i, 2^(i+1))
inline constexpr size_t topComponentCount = 5;

struct ComponentStats {
	uint32_t count = 0;
	uint64_t cells = 0;
	uint64_t largest = 0;
	uint32_t smallCount = 0;
	uint64_t smallCells = 0;
	std::array<uint64_t, topComponentCount> top {}; // largest sizes, descending, zero padded
	std::array<uint32_t, sizeBucketCount> buckets {};
};

struct FloorTopology {
	FloorBounds bounds;
	uint64_t tiles = 0;
	uint64_t walkableCells = 0;   // tile exists, has ground, not blocking, no unknown content
	uint64_t unknownCells = 0;
	uint64_t floorChangeCells = 0;
	uint64_t teleportCells = 0;
	ComponentStats walkable;      // 4-connected components of walkable cells
	uint64_t bottleneckCells = 0; // walkable cells whose removal increases the number of same-floor components
};

// Same-ground regions: 4-connected cells that share one ground key (key 0 included).
struct KeyRegionStats {
	bool impassable = false;
	uint64_t tiles = 0;
	uint32_t regions = 0;
	uint64_t largest = 0;
	// Regions whose cells never touch the edge of their floor's occupied box. A lake inside
	// land is enclosed; a sea that runs off the mapped area is not.
	uint32_t enclosed = 0;
};

struct TopologyReport {
	std::vector<FloorTopology> floors;
	std::vector<KeyRegionStats> keys; // indexed like SemanticMap::groundKeys()
};

enum class TopologyError : uint8_t { InvalidOptions, Cancelled, AllocationFailed };

// Working memory is about 9 bytes per cell of the largest floor, reused across floors.
// Output depends only on the map and options.
[[nodiscard]] std::expected<TopologyReport, TopologyError> analyzeTopology(
	const SemanticMap& map, const SemanticSummary& summary, const TopologyOptions& options, std::stop_token stop);

} // namespace rme::procedural
