// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/style_profile.h"
#include "procedural/core/surface.h"

#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace rme::procedural {

// Shape of bodies of water (D9, water part): rivers and lakes told apart by the shape of the mask alone, the same
// way for a reference map and for a generated grid. Integer math only. Definitions are fixed in
// docs/procedural/G4_METRICS.md (iteration 4); a lattice skeleton is an approximation of a river's centre line.

inline constexpr uint32_t riverMinArea = 40;           // tiles
inline constexpr uint32_t riverMinSkeleton = 20;       // skeleton tiles
inline constexpr uint32_t riverMaxWidthMilli = 7000;   // area / skeleton length, in thousandths of a tile
inline constexpr uint32_t seaMinArea = 20000;          // larger bodies are the sea and are skipped
inline constexpr uint32_t straightRunLength = 12;      // consecutive skeleton tiles in one row or column

struct RiverComponent {
	uint32_t area = 0;      // tiles
	uint32_t skeleton = 0;  // skeleton tiles (length estimate)
	uint32_t side = 0;      // longer side of the bounding box
	uint32_t straight = 0;  // skeleton tiles inside runs of at least straightRunLength in one row or column
	uint32_t branches = 0;  // skeleton tiles with at least 3 skeleton tiles among their 8 neighbours
	bool touchesOpen = false; // (channels only) touches open water or the border of the area
	bool operator==(const RiverComponent&) const = default;
};

struct WaterShapes {
	std::vector<RiverComponent> rivers;
	std::vector<uint32_t> lakeAreas;   // other bodies below seaMinArea
	uint64_t seaTiles = 0;             // tiles of bodies at or above seaMinArea
};

// Adds the bodies of one floor: `water` has width * height cells, nonzero = water. 4-connected bodies.
// Checks stop once per body. Working memory: the cells of one body plus a bitmap of its bounding box.
// `openWater` (empty or one cell per `water` cell) only fills RiverComponent::touchesOpen.
[[nodiscard]] std::expected<void, StatsError> addWaterShapes(
	std::span<const uint8_t> water, uint32_t width, uint32_t height, WaterShapes& out, std::stop_token stop,
	std::span<const uint8_t> openWater = {});

// Channels (iteration 5): the narrow part of the water, whether or not it joins the sea. Open water is the
// morphological opening of the water mask with a square of channelWindow cells; a channel is water minus open water.
inline constexpr uint32_t channelWindow = 13;
struct ChannelShapes {
	WaterShapes shapes;           // channel components in `rivers`
	uint64_t regionCells = 0;     // cells of the examined area (present tiles of a reference, all cells of a grid)
	uint64_t maskCells = 0;       // (key shapes only) tiles in the mask
};
// Fills `open` (same size as `water`) with the open water of one floor.
void openWaterMask(std::span<const uint8_t> water, uint32_t width, uint32_t height, std::vector<uint8_t>& open);
[[nodiscard]] std::expected<ChannelShapes, StatsError> extractChannelShapes(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop);
// Reference side, any set of ground keys (iteration 9, paths): narrow bodies of tiles whose ground key is listed, same
// component definition as the rivers. `shapes.rivers` are the accepted components, `touchesOpen` is not filled.
[[nodiscard]] std::expected<ChannelShapes, StatsError> extractKeyShapes(
	const SemanticMap& map, std::span<const std::string_view> keys, int zMin, int zMax, std::stop_token stop);
[[nodiscard]] std::expected<ChannelShapes, StatsError> channelShapesOf(const SurfaceGrid& grid, std::stop_token stop = {});

struct ChannelSummary {
	uint32_t count = 0;               // components
	uint32_t lengthPer64k = 0;        // skeleton tiles per 65536 cells of the area
	uint32_t widthMilli = 0;          // 1000 * area / skeleton, over all components
	uint32_t branchesPer100Milli = 0; // 1000 * (100 * branches / skeleton)
	uint32_t connectedPermille = 0;   // components touching open water or the border
	bool operator==(const ChannelSummary&) const = default;
};
[[nodiscard]] ChannelSummary summarizeChannels(const ChannelShapes& channels);

// Reference side: Water kind through `keymap` on the floors zMin..zMax; absent tiles are not water.
[[nodiscard]] std::expected<WaterShapes, StatsError> extractWaterShapes(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop);

// Generator side: Lake and River cells.
[[nodiscard]] std::expected<WaterShapes, StatsError> waterShapesOf(const SurfaceGrid& grid, std::stop_token stop = {});

// Medians over river components: tortuosity 1000 * skeleton / side (permille, 1000 = straight), straight share
// in permille of skeleton tiles, width 1000 * area / skeleton. count == 0 means there is no river.
struct RiverSummary {
	uint32_t count = 0;
	uint32_t tortuosityPermille = 0;
	uint32_t straightPermille = 0;
	uint32_t widthMilli = 0;
	bool operator==(const RiverSummary&) const = default;
};
[[nodiscard]] RiverSummary summarizeRivers(std::span<const RiverComponent> rivers);

// Text form for private local files: `g4-water/1`, then `river area skeleton side straight` and `lake area` lines
// and a `sea tiles` line. Round trip is lossless.
inline constexpr std::string_view waterShapesSchema = "g4-water/1";
[[nodiscard]] std::string serializeWaterShapes(const WaterShapes& shapes);
[[nodiscard]] std::expected<WaterShapes, ProfileError> parseWaterShapes(std::string_view text);

} // namespace rme::procedural
