// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/png_writer.h"
#include "procedural/core/semantic_map.h"
#include "procedural/core/topology.h"

#include <cstdint>
#include <expected>
#include <stop_token>
#include <string>
#include <vector>

namespace rme::procedural {

// Fixed palette slots. Ground keys follow from firstGroundSlot.
enum class SemanticClass : uint8_t {
	Empty = 0,         // no tile
	Unknown = 1,       // some item on the tile is unknown to the client
	Wall = 2,
	Door = 3,
	Teleport = 4,
	FloorChange = 5,
	BlockedByItem = 6, // an item blocks a tile whose ground is passable
	NoGround = 7,      // tile without ground
	GroundNoBrush = 8  // ground that no ground brush owns
};
inline constexpr uint8_t firstGroundSlot = 9; // ground key k >= 1 uses slot 9 + (k - 1) % 247

struct SemanticLegendEntry {
	uint8_t index = 0;
	PngColor color;
	std::string label;
};

struct SemanticFloorImage {
	FloorBounds bounds;            // pixel (0,0) is the floor box's (x0, y0); one pixel per tile
	std::vector<uint8_t> png;
};

struct SemanticImages {
	std::vector<SemanticLegendEntry> legend;
	std::vector<SemanticFloorImage> floors;
};

enum class SemanticImageError : uint8_t { Cancelled, Png, AllocationFailed };

// One indexed PNG per floor, classes by priority: unknown, wall, door, teleport, floor change,
// blocked-by-item, then the ground key's colour (impassable keys, from topology.keys, are darker).
// With more than 247 ground keys, slots are reused and the legend lists every key against its slot.
// Output depends only on the map and topology. Working memory is about 2 bytes per cell of the
// largest floor (indices and encoded image). Checks stop_requested once per row.
[[nodiscard]] std::expected<SemanticImages, SemanticImageError> renderSemanticImages(
	const SemanticMap& map, const TopologyReport& topology, std::stop_token stop);

// Legend and floor boxes as JSON (integers only). Box offsets are map coordinates: local use only.
[[nodiscard]] std::string renderSemanticLegendJson(const SemanticImages& images);

} // namespace rme::procedural
