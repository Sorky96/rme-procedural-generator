// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/generation_selection.h"

#include "editor/editor.h"
#include "editor/selection.h"
#include "map/map.h"
#include "map/tile.h"

#include <algorithm>
#include <new>
#include <wx/thread.h>

namespace rme::procedural {
namespace {

constexpr size_t cancelInterval = 4096;
constexpr uint64_t allocationAllowance = 32;

// Exclusive limits from both the map header and the inclusive Position limits.
uint64_t columnLimit(const Map& map) noexcept {
	return uint64_t(std::clamp<int64_t>(map.getWidth(), 0, int64_t(MAP_MAX_WIDTH) + 1));
}
uint64_t rowLimit(const Map& map) noexcept {
	return uint64_t(std::clamp<int64_t>(map.getHeight(), 0, int64_t(MAP_MAX_HEIGHT) + 1));
}
bool validFloor(int64_t z) noexcept {
	return z >= 0 && z < int64_t(MAP_LAYERS) && z <= int64_t(MAP_MAX_LAYER);
}

bool insideMap(const Map& map, const Position& position) noexcept {
	return position.x >= 0 && position.y >= 0 && validFloor(position.z) &&
		uint64_t(position.x) < columnLimit(map) && uint64_t(position.y) < rowLimit(map);
}

// A live selection entry that belongs to this map at its own position. Selection's own contract
// keeps stored pointers valid; only the pointer value is compared against the map's tile.
bool liveMember(const Map& map, const Tile* tile, Position& position) noexcept {
	if (!tile) return false;
	position = tile->getPosition();
	return insideMap(map, position) && map.getTile(position) == tile;
}

} // namespace

std::expected<GenerationSelection, SelectionMaskError> captureGenerationSelection(
	Editor& editor, SelectionMaskBudget budget, std::stop_token stop) {
	if (!wxIsMainThread()) return std::unexpected(SelectionMaskError::WrongThread);
	if (stop.stop_requested()) return std::unexpected(SelectionMaskError::Cancelled);
	Selection& selection = editor.selection;
	if (selection.isBusy() || selection.isDeferred()) return std::unexpected(SelectionMaskError::Busy);

	const Map& map = editor.map;
	const std::vector<Tile*>& tiles = selection.getTiles();
	if (tiles.empty()) return std::unexpected(SelectionMaskError::Empty);

	// Pass 1: identity, floor, strict Position order and bounds; no allocation. Entries are sorted
	// by Position, so the first and last give the row bounds.
	Position first;
	Position previous;
	int64_t minX = 0, maxX = 0;
	for (size_t i = 0; i < tiles.size(); ++i) {
		if (i % cancelInterval == 0 && stop.stop_requested()) return std::unexpected(SelectionMaskError::Cancelled);
		Position position;
		if (!liveMember(map, tiles[i], position)) return std::unexpected(SelectionMaskError::InvalidSelection);
		if (i == 0) {
			first = position;
			minX = maxX = position.x;
		} else {
			if (position.z != previous.z) return std::unexpected(SelectionMaskError::MultipleFloors);
			if (!(previous < position)) return std::unexpected(SelectionMaskError::InvalidSelection);
			minX = std::min<int64_t>(minX, position.x);
			maxX = std::max<int64_t>(maxX, position.x);
		}
		previous = position;
	}
	const int64_t minY = first.y;
	const int64_t maxY = previous.y;

	// Coordinates are within [0, MAP_MAX_*], so widths fit uint32 and the product fits uint64.
	const GenerationRegion region { int32_t(minX), int32_t(minY), int32_t(first.z),
		uint32_t(maxX - minX + 1), uint32_t(maxY - minY + 1) };
	const uint64_t cells = uint64_t(region.width) * region.height;
	if (cells > budget.maxCells) return std::unexpected(SelectionMaskError::CellLimit);
	const uint64_t fixedBytes = sizeof(GenerationSelection) + allocationAllowance;
	if (budget.maxBytes < fixedBytes || cells > budget.maxBytes - fixedBytes ||
		cells > std::vector<uint8_t>().max_size()) return std::unexpected(SelectionMaskError::MemoryLimit);
	if (stop.stop_requested()) return std::unexpected(SelectionMaskError::Cancelled);

	try {
		GenerationSelection result { region, map.getGeneration(), uint64_t(tiles.size()), fixedBytes + cells,
			std::vector<uint8_t>(size_t(cells), 0) };
		// Pass 2: same entries, already validated, nothing yielded in between.
		for (size_t i = 0; i < tiles.size(); ++i) {
			if (i % cancelInterval == 0 && stop.stop_requested()) return std::unexpected(SelectionMaskError::Cancelled);
			const Position position = tiles[i]->getPosition();
			result.mask[size_t(position.y - minY) * region.width + size_t(position.x - minX)] = 1;
		}
		if (stop.stop_requested()) return std::unexpected(SelectionMaskError::Cancelled);
		return result;
	} catch (const std::bad_alloc&) {
		return std::unexpected(SelectionMaskError::AllocationFailed);
	}
}

std::expected<void, SelectionMaskError> validateGenerationSelection(
	Editor& editor, const GenerationSelection& captured, std::stop_token stop) {
	if (!wxIsMainThread()) return std::unexpected(SelectionMaskError::WrongThread);
	if (stop.stop_requested()) return std::unexpected(SelectionMaskError::Cancelled);
	Selection& selection = editor.selection;
	if (selection.isBusy() || selection.isDeferred()) return std::unexpected(SelectionMaskError::Busy);

	const Map& map = editor.map;
	if (captured.mapGeneration != map.getGeneration()) return std::unexpected(SelectionMaskError::StaleSelection);

	// Stored geometry and mask shape, checked in uint64 before any indexing.
	const auto& region = captured.region;
	if (region.x < 0 || region.y < 0 || !validFloor(region.z) || region.width == 0 || region.height == 0 ||
		uint64_t(region.x) + region.width > columnLimit(map) || uint64_t(region.y) + region.height > rowLimit(map) ||
		uint64_t(region.width) * region.height != uint64_t(captured.mask.size()) || captured.selectedCells == 0) {
		return std::unexpected(SelectionMaskError::StaleSelection);
	}

	uint64_t ones = 0;
	for (size_t i = 0; i < captured.mask.size(); ++i) {
		if (i % cancelInterval == 0 && stop.stop_requested()) return std::unexpected(SelectionMaskError::Cancelled);
		const uint8_t value = captured.mask[i];
		if (value > 1) return std::unexpected(SelectionMaskError::StaleSelection);
		ones += value;
	}
	if (ones != captured.selectedCells) return std::unexpected(SelectionMaskError::StaleSelection);

	const std::vector<Tile*>& tiles = selection.getTiles();
	if (uint64_t(tiles.size()) != captured.selectedCells) return std::unexpected(SelectionMaskError::StaleSelection);

	// Unique current members, each on a captured 1 byte, with equal counts: the sets are identical.
	// The same pass collects the live tight bounds; entries are in z, y, x order, so the first and
	// last entries give the row bounds.
	Position first;
	Position previous;
	int64_t minX = 0, maxX = 0;
	for (size_t i = 0; i < tiles.size(); ++i) {
		if (i % cancelInterval == 0 && stop.stop_requested()) return std::unexpected(SelectionMaskError::Cancelled);
		Position position;
		if (!liveMember(map, tiles[i], position)) return std::unexpected(SelectionMaskError::InvalidSelection);
		if (i == 0) {
			first = position;
			minX = maxX = position.x;
		} else {
			if (!(previous < position)) return std::unexpected(SelectionMaskError::InvalidSelection);
			minX = std::min<int64_t>(minX, position.x);
			maxX = std::max<int64_t>(maxX, position.x);
		}
		previous = position;
		if (position.z != region.z || position.x < region.x || position.y < region.y ||
			uint64_t(position.x - region.x) >= region.width || uint64_t(position.y - region.y) >= region.height) {
			return std::unexpected(SelectionMaskError::StaleSelection);
		}
		const size_t index = size_t(position.y - region.y) * region.width + size_t(position.x - region.x);
		if (captured.mask[index] != 1) return std::unexpected(SelectionMaskError::StaleSelection);
	}
	// Equal sets do not prove canonical geometry: a region padded with all-zero rows or columns
	// would still match. Require the stored region to be exactly the tight live bounding rectangle.
	// All positions passed liveMember, so the extents are within [0, MAP_MAX_*] and fit uint32.
	const GenerationRegion tight { int32_t(minX), int32_t(first.y), int32_t(first.z),
		uint32_t(maxX - minX + 1), uint32_t(int64_t(previous.y) - first.y + 1) };
	if (!(region == tight)) return std::unexpected(SelectionMaskError::StaleSelection);
	if (stop.stop_requested()) return std::unexpected(SelectionMaskError::Cancelled);
	return {};
}

} // namespace rme::procedural
