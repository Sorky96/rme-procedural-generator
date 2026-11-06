// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/generation_input.h"
#include "map/tile.h"
#include <memory>

class Map;

namespace rme::procedural {

struct SnapshotCell {
	std::unique_ptr<Tile> original;
	uint16_t spawnCount = 0;
	uint16_t waypointCount = 0;
	uint16_t townCount = 0;
	std::vector<uint32_t> houseExits;
};

struct SnapshotBudget { uint64_t maxCells = 0; uint64_t maxBytes = 0; };

struct GenerationSnapshot {
	GenerationInput input;
	// Two-cell halo: border targets need their own eight-neighbour context.
	GenerationRegion context;
	uint32_t mapWidth = 0;
	uint32_t mapHeight = 0;
	uint32_t mapFormat = 0;
	uint32_t mapClientVersion = 0;
	uint64_t estimatedBytes = 0;
	std::vector<SnapshotCell> cells;
};

enum class SnapshotError {
	WrongThread, InvalidInput, Limit, Overflow, Unsupported, Cancelled, AllocationFailed
};

// GUI thread, no yield and no mutation. Limits are checked before tile copies.
// The estimate covers this owned snapshot, not the whole generation job/RSS.
std::expected<GenerationSnapshot, SnapshotError> captureGenerationSnapshot(
	const Map& map, const GenerationConfig& config, CoreBudget coreBudget,
	SnapshotBudget budget, std::span<const uint8_t> mask = {}, std::stop_token stop = {});

// Re-checks owned tile values and location metadata as well as revision stamps.
[[nodiscard]] bool snapshotMatchesMap(const Map& map, const GenerationSnapshot& snapshot) noexcept;

} // namespace rme::procedural
