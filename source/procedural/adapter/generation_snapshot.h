// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/generation_input.h"
#include "procedural/core/surface.h"
#include "procedural/core/terrain_report.h"
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

// Writable cells the surface algorithm leaves empty: a border beside existing ground would be
// written onto that ground, outside what the user allowed, and the preview refuses such results.
// A cell qualifies when one of its eight neighbours is not writable (outside the area, masked out or
// protected) and holds ground. One byte per cell, row-major over the region; computed from the
// snapshot alone, so capture and preview agree.
// With outerBorderBrushes (some brush of the profile draws an outer border onto empty neighbours, see
// profileNeedsEdgeClearance) a cell also qualifies beside a non-writable neighbour that holds no ground:
// that empty tile would otherwise receive the border. The cleared cells are inside the area, so the
// border lands on them, and nothing outside the allowed area is touched.
// Includes the cells of the config's protection margin (I63): they stay empty, and the cells beside them are kept clear
// as beside existing content.
[[nodiscard]] std::vector<uint8_t> surfaceKeepClear(const GenerationSnapshot& snapshot, bool outerBorderBrushes = false);

[[nodiscard]] bool emptyTerrainContent(const Tile* tile) noexcept;
// I63: counts of the existing content of the area's cells (not the halo); margin fields and contacts stay zero.
[[nodiscard]] rme::procedural::PreservedContent preservedContentOf(const GenerationSnapshot& snapshot);
// I63: house or spawn tiles of the snapshot (halo included) beside a written cell of `cells` (the area, row-major).
[[nodiscard]] uint64_t preservedContacts(const GenerationSnapshot& snapshot, std::span<const rme::procedural::SurfaceKind> cells);

// Re-checks owned tile values and location metadata as well as revision stamps.
[[nodiscard]] bool snapshotMatchesMap(const Map& map, const GenerationSnapshot& snapshot) noexcept;
// Only for synchronous continuation after a known generator step; still compares every source value.
[[nodiscard]] bool snapshotMatchesMapAtRevision(const Map& map, const GenerationSnapshot& snapshot, uint64_t revision) noexcept;

} // namespace rme::procedural
