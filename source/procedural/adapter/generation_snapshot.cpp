// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/generation_snapshot.h"
#include "procedural/adapter/snapshot_memory.h"
#include "procedural/adapter/snapshot_equal.h"

#include "map/map.h"
#include "game/item.h"
#include "app/managers/version_manager.h"
#include <algorithm>
#include <limits>
#include <new>
#include <wx/thread.h>

namespace rme::procedural {

std::expected<GenerationSnapshot, SnapshotError> captureGenerationSnapshot(
	const Map& map, const GenerationConfig& config, CoreBudget coreBudget,
	SnapshotBudget budget, std::span<const uint8_t> mask, std::stop_token stop) {
	if (!wxIsMainThread()) return std::unexpected(SnapshotError::WrongThread);
	if (stop.stop_requested()) return std::unexpected(SnapshotError::Cancelled);
	auto input = captureGenerationInput(map, config, coreBudget, budget.maxBytes, mask, stop);
	if (!input) return std::unexpected(input.error() == InputError::Cancelled ? SnapshotError::Cancelled :
		input.error() == InputError::AllocationFailed ? SnapshotError::AllocationFailed :
		input.error() == InputError::InputLimit ? SnapshotError::Limit : SnapshotError::InvalidInput);
	const auto& region = config.region;
	const int64_t left = std::max<int64_t>(0, int64_t(region.x) - 2);
	const int64_t top = std::max<int64_t>(0, int64_t(region.y) - 2);
	const int64_t right = std::min<int64_t>(int64_t(MAP_MAX_WIDTH) + 1, int64_t(region.x) + region.width + 2);
	const int64_t bottom = std::min<int64_t>(int64_t(MAP_MAX_HEIGHT) + 1, int64_t(region.y) + region.height + 2);
	const GenerationRegion context {int32_t(left), int32_t(top), region.z, uint32_t(right-left), uint32_t(bottom-top)};
	const uint64_t count = uint64_t(context.width) * context.height;
	if (count > budget.maxCells) return std::unexpected(SnapshotError::Limit);
	if (count > std::vector<SnapshotCell>().max_size() ||
		count > (std::numeric_limits<uint64_t>::max() - sizeof(GenerationSnapshot)) / sizeof(SnapshotCell)) {
		return std::unexpected(SnapshotError::Overflow);
	}
	uint64_t bytes = sizeof(GenerationSnapshot);
	const auto charge = [&](uint64_t amount) {
		if (bytes > budget.maxBytes || amount > budget.maxBytes - bytes) return false;
		bytes += amount;
		return true;
	};
	// Vector storage plus conservative bookkeeping allowance, not an RSS bound.
	if (!charge(count * sizeof(SnapshotCell)) || !charge(3 * 64) ||
		!charge(uint64_t(input->mask.size())) || !charge(uint64_t(input->protection.size()))) {
		return std::unexpected(SnapshotError::Limit);
	}
	try {
	// First pass does not clone any map payload. Source stays on the GUI thread.
	for (uint32_t y = 0; y < context.height; ++y) {
		if (stop.stop_requested()) return std::unexpected(SnapshotError::Cancelled);
		for (uint32_t x = 0; x < context.width; ++x) {
			const auto* location = map.getTileL(int(context.x + x), int(context.y + y), context.z);
			if (!location) continue;
			if (const auto* exits = location->getHouseExits(); exits && !exits->empty()) {
				if (exits->size() > std::numeric_limits<uint64_t>::max() / sizeof(uint32_t)) {
					return std::unexpected(SnapshotError::Overflow);
				}
				if (!charge(uint64_t(exits->size()) * sizeof(uint32_t)) || !charge(64)) return std::unexpected(SnapshotError::Limit);
			}
			if (const auto* tile = location->get()) {
				const auto memory = estimateSnapshotTileMemory(*tile, budget.maxBytes - bytes, stop);
				if (!memory) return std::unexpected(memory.error() == SnapshotMemoryError::Cancelled ? SnapshotError::Cancelled :
					memory.error() == SnapshotMemoryError::Overflow ? SnapshotError::Overflow :
					memory.error() == SnapshotMemoryError::Limit ? SnapshotError::Limit : SnapshotError::Unsupported);
				if (!charge(memory->bytes)) return std::unexpected(SnapshotError::Limit);
			}
		}
	}
		GenerationSnapshot result {std::move(*input), context, uint32_t(map.getWidth()), uint32_t(map.getHeight()),
			uint32_t(map.getVersion().otbm), uint32_t(map.getVersion().client), bytes,
			std::vector<SnapshotCell>(size_t(count))};
		for (uint32_t y = 0; y < context.height; ++y) {
			if (stop.stop_requested()) return std::unexpected(SnapshotError::Cancelled);
			for (uint32_t x = 0; x < context.width; ++x) {
				const auto* location = map.getTileL(int(context.x + x), int(context.y + y), context.z);
				if (!location) continue;
				auto& cell = result.cells[size_t(y) * context.width + x];
				cell.spawnCount = location->getSpawnCount();
				cell.waypointCount = location->getWaypointCount();
				cell.townCount = location->getTownCount();
				if (const auto* exits = location->getHouseExits()) cell.houseExits.assign(exits->begin(), exits->end());
				if (const auto* tile = location->get()) {
					cell.original = tile->deepCopy();
					if (!snapshotTilesEqual(tile, cell.original.get())) return std::unexpected(SnapshotError::Unsupported);
				}
			}
		}
		if (stop.stop_requested()) return std::unexpected(SnapshotError::Cancelled);
		return result;
	} catch (const std::bad_alloc&) {
		return std::unexpected(SnapshotError::AllocationFailed);
	}
}

bool emptyTerrainContent(const Tile* tile) noexcept {
	return !tile || (!tile->ground && tile->items.empty() && !tile->creature && !tile->spawn &&
		!tile->house_id && !tile->mapflags && !tile->extra && !(tile->statflags & ~uint16_t(TILESTATE_BLOCKING)));
}

PreservedContent preservedContentOf(const GenerationSnapshot& snapshot) {
	PreservedContent out;
	const auto& region = snapshot.input.config.region;
	const auto& context = snapshot.context;
	for (uint32_t y = 0; y < region.height; ++y) {
		for (uint32_t x = 0; x < region.width; ++x) {
			const size_t index = size_t(y) * region.width + x;
			if (index < snapshot.input.mask.size() && snapshot.input.mask[index] && !snapshot.input.protection[index]) ++out.writable;
			const auto& cell = snapshot.cells[size_t(region.y + int(y) - context.y) * context.width + size_t(region.x + int(x) - context.x)];
			const Tile* tile = cell.original.get();
			const bool kept = (tile && !emptyTerrainContent(tile)) || cell.spawnCount || cell.waypointCount || cell.townCount || !cell.houseExits.empty();
			if (!kept) continue;
			++out.tiles;
			out.spawns += cell.spawnCount || (tile && tile->spawn);
			out.waypoints += cell.waypointCount != 0;
			out.towns += cell.townCount != 0;
			out.houseExits += !cell.houseExits.empty();
			if (!tile) continue;
			out.houseTiles += tile->house_id != 0;
			out.zoneTiles += !tile->getZoneIds().empty();
			for (const auto& item : tile->items) out.idItems += item && (item->getUniqueID() || item->getActionID());
			out.idItems += tile->ground && (tile->ground->getUniqueID() || tile->ground->getActionID());
		}
	}
	return out;
}

uint64_t preservedContacts(const GenerationSnapshot& snapshot, std::span<const SurfaceKind> cells) {
	const auto& region = snapshot.input.config.region;
	const auto& context = snapshot.context;
	if (cells.size() != size_t(region.width) * region.height) return 0;
	const auto written = [&](int64_t x, int64_t y) {
		return x >= 0 && y >= 0 && x < int64_t(region.width) && y < int64_t(region.height) &&
			cells[size_t(y) * region.width + size_t(x)] != SurfaceKind::Preserve;
	};
	uint64_t contacts = 0;
	for (uint32_t cy = 0; cy < context.height; ++cy) {
		for (uint32_t cx = 0; cx < context.width; ++cx) {
			const auto& cell = snapshot.cells[size_t(cy) * context.width + cx];
			const Tile* tile = cell.original.get();
			if (!cell.spawnCount && !(tile && (tile->house_id || tile->spawn))) continue;
			const int64_t x = int64_t(context.x) + cx - region.x, y = int64_t(context.y) + cy - region.y;
			contacts += written(x - 1, y) || written(x + 1, y) || written(x, y - 1) || written(x, y + 1);
		}
	}
	return contacts;
}

bool snapshotMatchesMap(const Map& map, const GenerationSnapshot& snapshot) noexcept {
	return snapshotMatchesMapAtRevision(map, snapshot, snapshot.input.mapRevision);
}

bool snapshotMatchesMapAtRevision(const Map& map, const GenerationSnapshot& snapshot, uint64_t revision) noexcept {
	if (!wxIsMainThread() || map.getGeneration() != snapshot.input.mapGeneration ||
		map.getChangeTracker().getGeneration() != revision ||
		g_version.GetAssetRevision() != snapshot.input.assetRevision ||
		uint32_t(map.getWidth()) != snapshot.mapWidth || uint32_t(map.getHeight()) != snapshot.mapHeight ||
		uint32_t(map.getVersion().otbm) != snapshot.mapFormat || uint32_t(map.getVersion().client) != snapshot.mapClientVersion) return false;
	const auto& context = snapshot.context;
	if (context.x < 0 || context.y < 0 || context.z < 0 || context.z >= MAP_LAYERS || !context.width || !context.height ||
		uint64_t(context.x) + context.width > uint64_t(MAP_MAX_WIDTH) + 1 ||
		uint64_t(context.y) + context.height > uint64_t(MAP_MAX_HEIGHT) + 1 ||
		uint64_t(context.width) * context.height != snapshot.cells.size()) return false;
	for (uint32_t y = 0; y < context.height; ++y) {
		for (uint32_t x = 0; x < context.width; ++x) {
			const Position position(int(context.x + x), int(context.y + y), context.z);
			const auto* location = map.getTileL(position);
			const auto& cell = snapshot.cells[size_t(y) * context.width + x];
			if (cell.original && cell.original->getPosition() != position) return false;
			if (!snapshotTilesEqual(location ? location->get() : nullptr, cell.original.get())) return false;
			if (cell.spawnCount != (location ? location->getSpawnCount() : 0) ||
				cell.waypointCount != (location ? location->getWaypointCount() : 0) ||
				cell.townCount != (location ? location->getTownCount() : 0)) return false;
			const auto* exits = location ? location->getHouseExits() : nullptr;
			if ((!exits && !cell.houseExits.empty()) || (exits && *exits != cell.houseExits)) return false;
		}
	}
	return true;
}


std::vector<uint8_t> surfaceKeepClear(const GenerationSnapshot& snapshot, bool outerBorderBrushes) {
	const auto& input = snapshot.input;
	const auto& region = input.config.region;
	const auto& context = snapshot.context;
	std::vector<uint8_t> result(input.mask.size(), 0);
	const auto writableAt = [&](int64_t x, int64_t y) {
		if (x < 0 || y < 0 || x >= int64_t(region.width) || y >= int64_t(region.height)) return false;
		const size_t index = size_t(y) * region.width + size_t(x);
		return input.mask[index] && !input.protection[index];
	};
	const auto holdsGround = [&](int64_t x, int64_t y) {
		const int64_t cx = region.x + x - context.x, cy = region.y + y - context.y;
		if (cx < 0 || cy < 0 || cx >= int64_t(context.width) || cy >= int64_t(context.height)) return false;
		const auto& original = snapshot.cells[size_t(cy) * context.width + size_t(cx)].original;
		return original && original->ground;
	};
	for (int64_t y = 0; y < int64_t(region.height); ++y) {
		for (int64_t x = 0; x < int64_t(region.width); ++x) {
			if (!writableAt(x, y)) continue;
			for (int64_t dy = -1; dy <= 1; ++dy) {
				for (int64_t dx = -1; dx <= 1; ++dx) {
					if ((dx || dy) && !writableAt(x + dx, y + dy) && (outerBorderBrushes || holdsGround(x + dx, y + dy))) {
						result[size_t(y) * region.width + size_t(x)] = 1;
					}
				}
			}
		}
	}
	return result;
}

} // namespace rme::procedural
