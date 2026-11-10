// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "editor/operations/draw_operations.h"

#include "editor/editor.h"
#include "editor/action_queue.h"
#include "lua/lua_api.h"
#include "map/map.h"
#include "procedural/adapter/snapshot_memory.h"
#include <algorithm>
#include <array>
#include <new>

namespace {
using namespace rme::procedural;
using Result = std::expected<TerrainCommitReceipt, TerrainCommitError>;
Result failure(TerrainCommitFailure error) { return std::unexpected(TerrainCommitError(error)); }
}

std::expected<rme::procedural::TerrainCommitReceipt, rme::procedural::TerrainCommitError>
DrawOperations::commitTerrainPreview(const PreparedTerrain& prepared, const GenerationConfig& config,
	const GroundProfile& profile, TerrainCommitBudget budget, std::span<const uint8_t> mask, std::stop_token stop) {
	const auto target = validateTerrainTarget(prepared, config, profile, mask, stop);
	if (!target) return std::unexpected(TerrainCommitError(target.error()));
	return commitTerrainUnits(**target, prepared, budget, stop);
}

std::expected<rme::procedural::TerrainCommitReceipt, rme::procedural::TerrainCommitError>
DrawOperations::commitSelectedTerrainPreview(const PreparedTerrain& prepared, const GenerationSelection& selection,
	const GenerationConfig& config, const GroundProfile& profile, TerrainCommitBudget budget, std::stop_token stop) {
	const auto target = validateSelectedTerrainTarget(prepared, selection, config, profile, stop);
	if (!target) return std::unexpected(TerrainCommitError(target.error()));
	return commitTerrainUnits(**target, prepared, budget, stop);
}

std::expected<rme::procedural::TerrainCommitReceipt, rme::procedural::TerrainCommitError>
DrawOperations::commitTerrainUnits(Editor& editor, const PreparedTerrain& prepared, TerrainCommitBudget budget, std::stop_token stop) {
    std::array<const PreparedTerrain*, 5> units {};
    size_t count = 0;
    for (auto* p = prepared.plateau.get(); p; p = p->plateau.get()) {
        if (!p->preview.getChanges().empty()) {
            units[count++] = p;
        }
    }
    std::reverse(units.begin(), units.begin() + count);
    if (prepared.cave && !prepared.cave->preview.getChanges().empty()) {
        units[count++] = prepared.cave.get();
    }
    units[count++] = &prepared;
    if (count == 1) {
        return commitValidatedTerrain(editor, prepared, budget, stop);
    }
    if (!editor.actionQueue || !editor.actionQueue->beginTerrainPair(count)) {
        return failure(TerrainCommitFailure::BusyHistory);
    }
    uint64_t changes = 0, bytes = 0;
    Result result = failure(TerrainCommitFailure::MemoryLimit);
    for (size_t i = 0; i < count; ++i) {
        result = commitValidatedTerrain(editor, *units[i], budget, stop, &prepared);
        if (!result) {
            editor.actionQueue->finishTerrainPair(false);
            return result;
        }
        changes += result->changes;
        bytes += result->transitionBytes;
        if (result->transitionBytes > budget.maxBytes || result->changes > budget.maxChanges) {
            editor.actionQueue->finishTerrainPair(false);
            return failure(TerrainCommitFailure::MemoryLimit);
        }
        budget.maxBytes -= result->transitionBytes;
        budget.maxChanges -= result->changes;
    }
    result->totalBytes += bytes - result->transitionBytes;
    result->changes = changes;
    result->transitionBytes = bytes;
    editor.actionQueue->finishTerrainPair(true);
    return result;
}

std::expected<rme::procedural::TerrainCommitReceipt, rme::procedural::TerrainCommitError>
DrawOperations::commitValidatedTerrain(Editor& editor, const PreparedTerrain& prepared,
	TerrainCommitBudget budget, std::stop_token stop, const PreparedTerrain* retained) {
	if (!editor.actionQueue || editor.actionQueue->isApplyingTerrain()) return failure(TerrainCommitFailure::BusyHistory);
	if (LuaAPI::isTransactionActive()) return failure(TerrainCommitFailure::ActiveLuaTransaction);
	if (stop.stop_requested()) return failure(TerrainCommitFailure::Cancelled);
	const std::span<const Position> positions = prepared.preview.getChanges();
	if (!budget.maxChanges || positions.size() > budget.maxChanges) return failure(TerrainCommitFailure::ChangeLimit);
	// One subtract-only uint64 budget. The preparation is still owned by the caller throughout.
	uint64_t used = 0;
	if (!budget.maxBytes) return failure(TerrainCommitFailure::MemoryLimit);
	const auto charge = [&](uint64_t count, uint64_t bytes = 1) {
		if (bytes && count > (budget.maxBytes - used) / bytes) return false;
		used += count * bytes;
		return true;
	};
	// Public accounting fields are not authority. Recount retained containers/payload and use the
	// private preview estimate, then retain the larger original preparation charge (no refunds).
	const auto recount = [&](auto&& self, const PreparedTerrain& unit) -> std::expected<void, TerrainCommitFailure> {
		const auto& snapshot = unit.snapshot;
		if (!charge(sizeof(PreparedTerrain) + preparationScratchBytes + 8 * kSnapshotAllocationOverhead) ||
			!charge(snapshot.cells.capacity(), sizeof(SnapshotCell)) ||
			!charge(snapshot.input.mask.capacity()) || !charge(snapshot.input.protection.capacity()) ||
			!charge(unit.plan.items.capacity(), sizeof(uint16_t)) || !charge(unit.plan.decor.capacity(), sizeof(uint16_t)) ||
			!charge(unit.plan.client.capacity() + 1) || !charge(unit.preview.getEstimatedPeakBytes())) {
			return std::unexpected(TerrainCommitFailure::MemoryLimit);
		}
		for (const auto* name : {&unit.plan.profile.wall, &unit.plan.profile.door, &unit.plan.profile.campfire,
			&unit.plan.profile.caveEntrance, &unit.plan.profile.caveExit, &unit.plan.profile.caveGround}) {
			if (!charge(name->capacity() + 1 + kSnapshotAllocationOverhead)) return std::unexpected(TerrainCommitFailure::MemoryLimit);
		}
		for (const auto layer : {std::span(unit.plan.walls), std::span(unit.plan.fires), std::span(unit.plan.entrances),
			std::span(unit.plan.exits)}) {
			if (!charge(layer.size(), sizeof(uint16_t))) return std::unexpected(TerrainCommitFailure::MemoryLimit);
		}
		if (!charge(unit.plan.caveFloor.capacity()) || !charge(unit.plan.doors.capacity())) return std::unexpected(TerrainCommitFailure::MemoryLimit);
		for (const auto& name : unit.plan.profile.active()) {
			if (!charge(name.capacity() + 1 + kSnapshotAllocationOverhead)) return std::unexpected(TerrainCommitFailure::MemoryLimit);
		}
		for (const auto& name : unit.plan.profile.activeDecor()) {
			if (!charge(name.capacity() + 1 + kSnapshotAllocationOverhead)) return std::unexpected(TerrainCommitFailure::MemoryLimit);
		}
		for (const auto& cell : snapshot.cells) {
			if (stop.stop_requested()) return std::unexpected(TerrainCommitFailure::Cancelled);
			if (!charge(cell.houseExits.capacity(), sizeof(uint32_t))) return std::unexpected(TerrainCommitFailure::MemoryLimit);
			if (!cell.original) continue;
			const auto estimate = estimateSnapshotTileMemory(*cell.original, budget.maxBytes - used, stop);
			if (!estimate) return std::unexpected(estimate.error() == SnapshotMemoryError::Cancelled ?
				TerrainCommitFailure::Cancelled : TerrainCommitFailure::MemoryLimit);
			if (!charge(estimate->bytes)) return std::unexpected(TerrainCommitFailure::MemoryLimit);
		}
        if (!charge(unit.plan.plateauFloor.capacity()) || !charge(unit.plan.plateauRamps.capacity(), sizeof(uint16_t))) {
            return std::unexpected(TerrainCommitFailure::MemoryLimit);
        }
        for (const auto* name : {&unit.plan.profile.caveWall, &unit.plan.profile.plateauGround, &unit.plan.profile.plateauRamp}) {
            if (!charge(name->capacity() + 1 + kSnapshotAllocationOverhead)) {
                return std::unexpected(TerrainCommitFailure::MemoryLimit);
            }
        }
        if (unit.plateau) {
            const auto result = self(self, *unit.plateau);
            if (!result) {
                return result;
            }
        }
        if (unit.cave) {
			const auto result = self(self, *unit.cave);
			if (!result) return result;
		}
		used = std::max(used, unit.memory.totalBytes);
		if (used > budget.maxBytes) return std::unexpected(TerrainCommitFailure::MemoryLimit);
		return {};
	};
	if (const auto result = recount(recount, retained ? *retained : prepared); !result) return failure(result.error());
	const auto preparationBytes = used;
	const auto beforeSelection = used;
	if (!charge(uint64_t(editor.selection.size()) + positions.size(), 3 * sizeof(Tile*)) ||
		!charge(kSnapshotAllocationOverhead)) return failure(TerrainCommitFailure::MemoryLimit);
	const auto selectionBytes = used - beforeSelection;
	const auto& region = prepared.snapshot.input.config.region;
	const uint64_t right = uint64_t(region.x) + region.width - 1;
	const uint64_t bottom = uint64_t(region.y) + region.height - 1;
	const uint64_t cells = (right / 64 - uint64_t(region.x) / 64 + 1) *
		(bottom / 64 - uint64_t(region.y) / 64 + 1);
	const uint64_t nodes = (right / 4 - uint64_t(region.x) / 4 + 1) *
		(bottom / 4 - uint64_t(region.y) / 4 + 1);
	// Include both old/new grid vector storage at growth, reservation records, history bookkeeping,
	// copies of new tiles and ownership of displaced tiles. Container/allocator assumptions match
	// snapshot_memory; these policy estimates are not an allocator cap or RSS measurement.
	if (!charge(64 * 1024 + sizeof(Action) + sizeof(BatchAction) + 256 + 8 * kSnapshotAllocationOverhead) ||
		!charge(positions.size(), sizeof(Change) + 3 * sizeof(std::unique_ptr<Change>) +
			BaseMap::TileReservation::entryBytes() + kSnapshotAllocationOverhead) ||
		!charge(cells, sizeof(SpatialHashGrid::GridCell) + kSnapshotAllocationOverhead) ||
		!charge(nodes, sizeof(MapNode) + sizeof(Floor) + 2 * kSnapshotAllocationOverhead) ||
		!charge(uint64_t(editor.map.getGrid().cellCount()) + cells, 3 * sizeof(SpatialHashGrid::CellEntry)) ||
		!charge(uint64_t(editor.actionQueue->getSize()) + 1, 3 * sizeof(std::unique_ptr<BatchAction>))) {
		return failure(TerrainCommitFailure::MemoryLimit);
	}
	for (const auto& position : positions) {
		if (stop.stop_requested()) return failure(TerrainCommitFailure::Cancelled);
		for (const auto* tile : std::array<const Tile*,2> {prepared.preview.getBuffer().getTile(position), editor.map.getTile(position)}) {
			if (!tile) continue;
			const auto estimate = estimateSnapshotTileMemory(*tile, budget.maxBytes - used, stop);
			if (!estimate) return failure(estimate.error() == SnapshotMemoryError::Cancelled ?
				TerrainCommitFailure::Cancelled : TerrainCommitFailure::MemoryLimit);
			if (!charge(estimate->bytes)) return failure(TerrainCommitFailure::MemoryLimit);
		}
	}
	const auto transitionBytes = used - preparationBytes;
	// The existing queue's cached batch memory field is uint32_t. Fail closed before it can wrap.
	if (transitionBytes > UINT32_MAX) return failure(TerrainCommitFailure::MemoryLimit);
	if (positions.empty()) return TerrainCommitReceipt {0, transitionBytes, used, prepared.preview.getFingerprint()};
	try {
		auto action = editor.actionQueue->createAction(ACTION_GENERATE_TERRAIN);
		action->changes.reserve(positions.size());
		action->preparedMemory = size_t(transitionBytes) - sizeof(BatchAction) - 3 * sizeof(Action*);
		action->preparedSelectionBytes = size_t(selectionBytes);
		for (const auto& position : positions) {
			if (stop.stop_requested()) return failure(TerrainCommitFailure::Cancelled);
			// currentFingerprint() already checked non-null tiles and strictly ordered unique,
			// valid positions. No yield or user callback runs between that check and these copies.
			const auto* previewTile = prepared.preview.getBuffer().getTile(position);
			ASSERT(previewTile);
			// Standalone deep copies own their location until the final noexcept exchange.
			// Keep concrete flags, subtypes, order and minimap values exactly as previewed.
			action->addChange(std::make_unique<Change>(previewTile->deepCopy(), position));
		}
		auto batch = editor.actionQueue->createBatch(ACTION_GENERATE_TERRAIN);
		batch->setLabel("Generate Terrain");
		batch->addAction(std::move(action));
		const TerrainCommitReceipt receipt {positions.size(), transitionBytes, used, prepared.preview.getFingerprint()};
		if (!editor.actionQueue->commitTerrainBatch(std::move(batch), positions, stop)) {
			return failure(stop.stop_requested() ? TerrainCommitFailure::Cancelled : TerrainCommitFailure::HistoryRefused);
		}
		return receipt;
	} catch (const std::bad_alloc&) {
		return failure(TerrainCommitFailure::AllocationFailed);
	}
}
