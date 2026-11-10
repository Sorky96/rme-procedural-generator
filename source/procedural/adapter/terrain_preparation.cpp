// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/terrain_preparation.h"
#include "procedural/adapter/door_types.h"

#include "app/managers/version_manager.h"
#include "editor/operations/draw_operations.h"
#include "map/map.h"
#include "item_definitions/core/item_definition_store.h"
#include "procedural/adapter/snapshot_memory.h"
#include "procedural/adapter/surface_resolver.h"
#include "procedural/core/decoration.h"

#include <algorithm>
#include <limits>
#include <new>
#include <optional>
#include <wx/thread.h>

namespace rme::procedural {
namespace {

constexpr uint64_t maxNameBytes = 256;

std::unexpected<PreparationError> fail(PreparationError error) {
	return std::unexpected<PreparationError>(std::move(error));
}

PreparationError fromSurface(SurfaceError error) {
	switch (error) {
		case SurfaceError::Cancelled: return PreparationInputError::Cancelled;
		case SurfaceError::AllocationFailed: return PreparationInputError::AllocationFailed;
		case SurfaceError::CellLimit: return GenerationError::CellLimit;
		case SurfaceError::OutputLimit: case SurfaceError::WorkingLimit: return PreparationInputError::MemoryLimit;
		case SurfaceError::SizeOverflow: return GenerationError::SizeOverflow;
		case SurfaceError::InvalidRegion: return GenerationError::InvalidRegion;
		case SurfaceError::InvalidMask: return GenerationError::InvalidMask;
		case SurfaceError::NoWritableCells: return GenerationError::NoWritableCells;
		case SurfaceError::InvalidParameters: break;
	}
	return PreparationInputError::InvalidParameters;
}

bool validName(uint64_t length) noexcept {
	return length >= 1 && length <= maxNameBytes;
}

// Heap bound for one copied std::string of `length` characters: capacity may round up past the
// length, so charge twice the terminated size plus one allocation allowance. length <= 256 here.
uint64_t stringCopyBytes(uint64_t length) noexcept {
	return 2 * (length + 1) + kSnapshotAllocationOverhead;
}

struct PreparationPreflight {
	uint64_t cellCount;
	uint64_t remaining;
	uint64_t semanticBytes;
	uint64_t planBytes;
	uint64_t assetRevision;
};

constexpr uint64_t infrastructureBytes = sizeof(PreparedTerrain) + preparationScratchBytes;

std::expected<PreparationPreflight, PreparationError> preflight(
	const Map& map, const GenerationConfig& config, const GroundProfile& profile,
	PreparationBudget budget, std::span<const uint8_t> mask, std::stop_token stop) {
	if (!wxIsMainThread()) return fail(PreparationInputError::WrongThread);
	if (stop.stop_requested()) return fail(PreparationInputError::Cancelled);
	if (!g_version.IsVersionLoaded() || !g_version.getLoadedVersion()) return fail(PreparationInputError::NoClient);

	// One joint counter; an amount is subtracted only after it is known to fit.
	uint64_t remaining = budget.maxBytes;
	const auto reserve = [&remaining](uint64_t amount) noexcept {
		if (amount > remaining) return false;
		remaining -= amount;
		return true;
	};

	if (!reserve(infrastructureBytes)) return fail(PreparationInputError::MemoryLimit);

	// Same extent as captureGenerationInput: header dimensions clipped to the inclusive
	// Position limits made exclusive, and every map layer.
	const MapExtent extent {
		uint32_t(std::min(map.getWidth(), MAP_MAX_WIDTH + 1)),
		uint32_t(std::min(map.getHeight(), MAP_MAX_HEIGHT + 1)), MAP_LAYERS
	};
	const auto layout = validateGeneration(config, extent, { budget.maxCells, UINT64_MAX }, mask, {}, stop);
	if (!layout) return fail(layout.error());

	// Two-cell halo exactly as captureGenerationSnapshot computes it: clipped to Position
	// limits, not the map header. The validated region keeps right > left and bottom > top.
	const auto& region = config.region;
	const int64_t left = std::max<int64_t>(0, int64_t(region.x) - 2);
	const int64_t top = std::max<int64_t>(0, int64_t(region.y) - 2);
	const int64_t right = std::min<int64_t>(int64_t(MAP_MAX_WIDTH) + 1, int64_t(region.x) + region.width + 2);
	const int64_t bottom = std::min<int64_t>(int64_t(MAP_MAX_HEIGHT) + 1, int64_t(region.y) + region.height + 2);
	const uint64_t contextCells = uint64_t(right - left) * uint64_t(bottom - top);
	if (contextCells > budget.maxContextCells) return fail(PreparationInputError::ContextLimit);
	if (budget.maxChanges == 0) return fail(PreparationInputError::ChangeLimit);

	const uint8_t expectedBrushes = config.algorithm == GenerationAlgorithm::Surface ?
		GroundProfile::surfaceCount : GroundProfile::groundRollCount;
	if (config.algorithm == GenerationAlgorithm::Surface) {
		const uint8_t required = config.surface.buildingDensity || config.surface.campCount || hasCave(profile) ? GroundProfile::surfaceWithFloorCount :
			config.surface.pathLength ? GroundProfile::surfaceWithPathCount : GroundProfile::surfaceCount;
		if (profile.count != required) return fail(PreparationInputError::InvalidProfile);
		if (!profile.wall.empty() && !config.surface.buildingDensity) return fail(PreparationInputError::InvalidProfile);
		if (!profile.door.empty() && (profile.wall.empty() || !doorTypeOf(profile.door))) return fail(PreparationInputError::InvalidProfile);
		if (!profile.campfire.empty() && profile.count != GroundProfile::surfaceWithFloorCount) return fail(PreparationInputError::InvalidProfile);
	} else if (profile.count != expectedBrushes) {
		return fail(PreparationInputError::InvalidProfile);
	}
    if (!validPlateauProfile(profile, config) || !validCaveProfile(profile, config)) {
        return fail(PreparationInputError::InvalidProfile);
    }
    if (hasCave(profile)) {
		if (region.z >= MAP_MAX_LAYER) return fail(PreparationInputError::NoLowerFloor);
		for (uint32_t y = 0; y < region.height; ++y) {
			if (stop.stop_requested()) return fail(PreparationInputError::Cancelled);
			for (uint32_t x = 0; x < region.width; ++x) {
				if (!emptyTerrainContent(map.getTile(Position(region.x + int(x), region.y + int(y), region.z + 1))))
					return fail(PreparationInputError::LowerFloorOccupied);
			}
		}
	}
    if (hasPlateau(profile)) {
        if (region.z < int(config.surface.plateauLevels)) {
            return fail(PreparationInputError::NoUpperFloor);
        }
        for (uint32_t level = 1; level <= config.surface.plateauLevels; ++level) {
            for (uint32_t y = 0; y < region.height; ++y) {
                if (stop.stop_requested()) {
                    return fail(PreparationInputError::Cancelled);
                }
                for (uint32_t x = 0; x < region.width; ++x) {
                    if (!emptyTerrainContent(map.getTile(Position(region.x + int(x), region.y + int(y), region.z - int(level))))) {
                        return fail(PreparationInputError::UpperFloorOccupied);
                    }
                }
            }
        }
        auto brushes = resolvePlateauBrushes(profile, config);
        if (!brushes) {
            return fail(brushes.error());
        }
    }
    const bool vegetation = config.vegetation;
	if (vegetation && config.algorithm != GenerationAlgorithm::Surface) return fail(PreparationInputError::InvalidParameters);
	if (vegetation && !valid(config.decor)) return fail(PreparationInputError::InvalidParameters);
	if (profile.decorCount != (vegetation ? GroundProfile::decorationCount : 0)) return fail(PreparationInputError::InvalidProfile);
	uint64_t profileBytes = 0;
	for (const auto& name : profile.activeDecor()) {
		if (!validName(name.size())) return fail(PreparationInputError::InvalidProfile);
		profileBytes += stringCopyBytes(name.size());
	}
	for (const auto& name : profile.active()) {
		if (!validName(name.size())) return fail(PreparationInputError::InvalidProfile);
		profileBytes += stringCopyBytes(name.size());
	}
	if (!profile.wall.empty()) {
		if (!validName(profile.wall.size())) return fail(PreparationInputError::InvalidProfile);
		profileBytes += stringCopyBytes(profile.wall.size());
	}
	if (!profile.door.empty()) {
		if (!validName(profile.door.size())) return fail(PreparationInputError::InvalidProfile);
		profileBytes += stringCopyBytes(profile.door.size());
	}
	if (!profile.campfire.empty()) {
		if (!validName(profile.campfire.size())) return fail(PreparationInputError::InvalidProfile);
		profileBytes += stringCopyBytes(profile.campfire.size());
	}
	for (const auto* name : {&profile.caveEntrance, &profile.caveExit, &profile.caveGround, &profile.caveWall}) {
		if (!name->empty()) profileBytes += stringCopyBytes(name->size());
	}
	// Borrowed, allocation-free length check: an oversized client ID is rejected before any
	// stage copies it. The view is used only here and never stored.
	const uint64_t clientLength = g_version.GetCurrentVersionIDView().size();
	if (!validName(clientLength)) return fail(PreparationInputError::InvalidProfile);

	// Plan ownership: the GroundPlan object, its row-major item vector, one heap string per
	// brush name in the retained plan.profile (profileBytes sums the three name bounds), and two
	// client strings: the retained plan.client and the resolver's local GetCurrentVersionID()
	// copy. The preview's temporary client copy for its comparison is covered by the scratch
	// allowance. Each fixed term is at most a few KiB; the vector term is checked below.
	const uint64_t cellCount = layout->cellCount;
	const uint64_t fixedPlanBytes = sizeof(GroundPlan) + kSnapshotAllocationOverhead +
		profileBytes + 2 * stringCopyBytes(clientLength);
	if (cellCount > (std::numeric_limits<uint64_t>::max() - fixedPlanBytes) / sizeof(uint16_t)) {
		return fail(PreparationInputError::MemoryLimit);
	}
	const uint64_t planBytes = fixedPlanBytes + cellCount * sizeof(uint16_t) * ((vegetation ? 2 : 1) + (profile.wall.empty() ? 0 : 1) + (profile.campfire.empty() ? 0 : 1) + (hasCave(profile) ? 2 : 0)) + (profile.door.empty() ? 0 : cellCount);

	const uint64_t outputBytes = layout->outputBytes;
	if (outputBytes > std::numeric_limits<uint64_t>::max() - kSnapshotAllocationOverhead) {
		return fail(PreparationInputError::MemoryLimit);
	}
	uint64_t semanticBytes = outputBytes + kSnapshotAllocationOverhead;
	if (config.algorithm == GenerationAlgorithm::Surface) {
		// The surface generator also holds working fields and queues; reserve its bound up front.
		const auto surface = validateSurface({ config.region, config.seed, config.surface }, extent,
			{ budget.maxCells, UINT64_MAX, UINT64_MAX }, mask, {}, stop);
		if (!surface) return fail(fromSurface(surface.error()));
		if (surface->workingBytes > std::numeric_limits<uint64_t>::max() - semanticBytes) {
			return fail(PreparationInputError::MemoryLimit);
		}
		semanticBytes += surface->workingBytes;
		// surfaceKeepClear() and the combined protection: one byte per cell each, alive while the grid is built.
		if (cellCount > (std::numeric_limits<uint64_t>::max() - semanticBytes - 2 * kSnapshotAllocationOverhead) / 2) {
			return fail(PreparationInputError::MemoryLimit);
		}
		semanticBytes += 2 * cellCount + 2 * kSnapshotAllocationOverhead;
		if (hasCave(profile)) {
			// Cave cells, exit search occupancy, landings, queues, brush geometry and retained sparse plan.
			// Conservative bound; charged before generating and never refunded.
			if (cellCount > (UINT64_MAX - semanticBytes - 65536) / 128) return fail(PreparationInputError::MemoryLimit);
			semanticBytes += cellCount * 128 + 65536;
		}
        if (hasPlateau(profile)) {
            if (cellCount > (UINT64_MAX - semanticBytes - 65536) / 192) {
                return fail(PreparationInputError::MemoryLimit);
            }
            semanticBytes += (cellCount * 192 + 65536) * config.surface.plateauLevels;
        }
        if (vegetation) {
			if (cellCount > (std::numeric_limits<uint64_t>::max() - semanticBytes) / decorationBytesPerCell) {
				return fail(PreparationInputError::MemoryLimit);
			}
			semanticBytes += cellCount * decorationBytesPerCell;
		}
	}

	// Reserved before any copy of map data; kept even after the grid is released.
	if (!reserve(semanticBytes) || !reserve(planBytes)) return fail(PreparationInputError::MemoryLimit);

	return PreparationPreflight { cellCount, remaining, semanticBytes, planBytes, g_version.GetAssetRevision() };
}

TerrainPreparationSteps runPreparation(
	const Map& map, const GenerationConfig& config, const GroundProfile& profile,
	PreparationBudget budget, std::span<const uint8_t> mask, std::stop_token stop, PreparationPreflight limits) {
	if (!wxIsMainThread()) co_return fail(PreparationInputError::WrongThread);
	if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
	if (g_version.GetAssetRevision() != limits.assetRevision) co_return fail(PreviewError::StaleSnapshot);
	try {
		const auto cellCount = limits.cellCount;
		auto& remaining = limits.remaining;
		const auto semanticBytes = limits.semanticBytes;
		const auto planBytes = limits.planBytes;
		const auto reserve = [&remaining](uint64_t amount) noexcept {
			if (amount > remaining) return false;
			remaining -= amount;
			return true;
		};

		auto snapshot = captureGenerationSnapshot(map, config, { budget.maxCells, semanticBytes },
			{ budget.maxContextCells, remaining }, mask, stop);
		if (!snapshot) co_return fail(snapshot.error());
		const uint64_t snapshotBytes = snapshot->estimatedBytes;
		if (!reserve(snapshotBytes)) co_return fail(PreparationInputError::MemoryLimit);

		// Count actual writable cells before generating: a sparse mask is judged by what it writes.
		// Capture always owns two full-size masks. A mismatch is malformed resolver input, reported
		// in the resolver's own error domain rather than silently undercounted.
		const auto& input = snapshot->input;
		if (input.mask.size() != cellCount || input.protection.size() != cellCount) {
			co_return fail(ResolveError::InvalidInput);
		}
		uint64_t writable = 0;
		for (size_t i = 0; i < input.mask.size(); ++i) {
			if (i % 4096 == 0 && stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
			writable += (input.mask[i] && !input.protection[i]) ? 1 : 0;
		}
		if (writable > budget.maxChanges) co_return fail(PreparationInputError::ChangeLimit);

		co_await std::suspend_always{};
		if (g_version.GetAssetRevision() != input.assetRevision) co_return fail(PreviewError::StaleSnapshot);

		// Surface semantics stay alive for the lower preview; their full working estimate remains charged.
		GroundPlan plan;
		std::unique_ptr<PreparedTerrain> cave;
        std::unique_ptr<PreparedTerrain> plateau;
        {
			if (input.config.algorithm == GenerationAlgorithm::Surface) {
				const auto keepClear = surfaceKeepClear(*snapshot, profileNeedsEdgeClearance(profile));
				std::vector<uint8_t> protection = input.protection;
				for (size_t i = 0; i < protection.size(); ++i) protection[i] |= keepClear[i];
				auto grid = generateSurface({ input.config.region, input.config.seed, input.config.surface }, input.extent,
					{ budget.maxCells, semanticBytes, semanticBytes }, input.mask, protection, stop);
				if (!grid) co_return fail(fromSurface(grid.error()));
				std::optional<DecorGrid> decor;
				if (input.config.vegetation) {
					auto generated = generateDecoration(*grid, input.config.seed, input.config.decor, stop);
					if (!generated) co_return fail(fromSurface(generated.error()));
					decor = std::move(*generated);
				}
				const auto resolve = [&]() { return resolveSurfacePlan(input, *grid, profile, planBytes, stop, decor ? &*decor : nullptr, keepClear); };
				if (hasCave(profile)) {
					auto brushes = resolveCaveBrushes(profile, input.config);
					if (!brushes) co_return fail(brushes.error());
					auto generated = generateCave(*grid, input.config.seed, stop);
					if (!generated) co_return fail(fromSurface(generated.error()));
					std::vector<uint8_t> occupied;
					if (decor) {
						occupied.resize(cellCount);
						for (size_t i = 0; i < cellCount; ++i) occupied[i] = decor->cells[i] != DecorKind::None;
					}
					auto exits = placeCaveExits(*grid, *generated, brushes->shapes, occupied, stop);
					if (!exits) co_return fail(exits.error() == UndergroundError::Cancelled ? PreparationInputError::Cancelled :
						exits.error() == UndergroundError::AllocationFailed ? PreparationInputError::AllocationFailed : PreparationInputError::InvalidParameters);
					auto resolved = resolve();
					if (!resolved) co_return fail(resolved.error());
					plan = std::move(*resolved);
					auto lowerConfig = input.config;
					++lowerConfig.region.z;
					if (!reserve(sizeof(PreparedTerrain) + planBytes)) co_return fail(PreparationInputError::MemoryLimit);
					auto lowerSnapshot = captureGenerationSnapshot(map, lowerConfig, {budget.maxCells, semanticBytes},
						{budget.maxContextCells, remaining}, mask, stop);
					if (!lowerSnapshot) co_return fail(lowerSnapshot.error());
					if (!reserve(lowerSnapshot->estimatedBytes)) co_return fail(PreparationInputError::MemoryLimit);
					GroundPlan lowerPlan = {lowerConfig, profile, plan.client, input.mapGeneration, input.mapRevision, input.assetRevision,
						generated->fingerprint(), std::vector<uint16_t>(cellCount, 0)};
					lowerPlan.caveFloor.resize(cellCount);
					lowerPlan.exits.assign(cellCount, 0);
					plan.entrances.assign(cellCount, 0);
					for (size_t i = 0; i < cellCount; ++i) {
						const bool floor = generated->cells[i] == CaveKind::Floor;
						bool wall = false;
						const int x = int(i % grid->region.width), y = int(i / grid->region.width);
						if (!floor) for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
							if (x + dx >= 0 && y + dy >= 0 && x + dx < int(grid->region.width) && y + dy < int(grid->region.height))
								wall |= generated->cells[size_t(y + dy) * grid->region.width + size_t(x + dx)] == CaveKind::Floor;
						}
						lowerPlan.caveFloor[i] = floor ? 1 : wall ? 2 : 0;
						if ((floor || wall) && lowerSnapshot->input.mask[i] && !lowerSnapshot->input.protection[i]) lowerPlan.items[i] = floor ? brushes->ground : brushes->wall;
					}
					for (const auto& exit : exits->exits) {
						const auto e = grid->entrances[exit.entrance], p = exit.cell;
						if (!plan.items[e] || !lowerPlan.items[p]) continue;
						const auto& ids = brushes->items[exit.shape];
						const auto& shape = brushes->shapes[exit.shape];
						const auto tail = size_t(int64_t(p) + shape.tailDx + int64_t(shape.tailDy) * grid->region.width);
						if (shape.hasTail && !lowerPlan.items[tail]) continue;
						plan.entrances[e] = brushes->entrances[(uint64_t(e) + input.config.seed) % brushes->entrances.size()];
						if (input.config.surface.caveEntranceStyle == CaveEntranceStyle::FloorHole) plan.items[e] = plan.entrances[e];
						if (!plan.decor.empty()) plan.decor[e] = 0;
						lowerPlan.exits[p] = ids.ramp;
						if (shape.hasTail) lowerPlan.exits[tail] = ids.tail;
					}
					auto lowerSteps = DrawOperations::prepareTerrainPreviewSteps(map, *lowerSnapshot, lowerPlan,
						{budget.maxChanges, remaining}, stop);
					auto lower = lowerSteps.advance();
					while (!lower) {
						co_await std::suspend_always{};
						if (g_version.GetAssetRevision() != input.assetRevision) co_return fail(PreviewError::StaleSnapshot);
						lower = lowerSteps.advance();
					}
					if (!*lower) co_return fail(lower->error());
					const auto lowerPreviewBytes = (**lower).getEstimatedPeakBytes();
					if (!reserve(lowerPreviewBytes)) co_return fail(PreparationInputError::MemoryLimit);
					const PreparationMemory lowerMemory {sizeof(PreparedTerrain), lowerSnapshot->estimatedBytes, 0, planBytes,
						lowerPreviewBytes, sizeof(PreparedTerrain) + lowerSnapshot->estimatedBytes + planBytes + lowerPreviewBytes};
					cave = std::make_unique<PreparedTerrain>(PreparedTerrain {std::move(*lowerSnapshot), std::move(lowerPlan), std::move(**lower), lowerMemory});
				} else {
					auto resolved = resolve();
					if (!resolved) co_return fail(resolved.error());
					plan = std::move(*resolved);
				}
                if (hasPlateau(profile)) {
                    auto brushes = resolvePlateauBrushes(profile, input.config);
                    if (!brushes) {
                        co_return fail(brushes.error());
                    }
                    std::vector<uint8_t> occupied(cellCount);
                    for (size_t i = 0; i < cellCount; ++i) {
                        occupied[i] = (!plan.decor.empty() && plan.decor[i]) || (!plan.entrances.empty() && plan.entrances[i])
                            || (!plan.exits.empty() && plan.exits[i]);
                    }
                    if (cave) {
                        constexpr int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
                        // Reserve cave ramp surface destinations as well as entrances.
                        auto cb = resolveCaveBrushes(profile, input.config);
                        if (!cb) {
                            co_return fail(cb.error());
                        }
                        for (size_t i = 0; i < cellCount; ++i) {
                            if (cave->plan.exits[i]) {
                                for (size_t s = 0; s < cb->items.size(); ++s) {
                                    if (cave->plan.exits[i] == cb->items[s].ramp) {
                                        auto d = size_t(cb->shapes[s].direction);
                                        auto dest = int64_t(i) + dx[d] + int64_t(dy[d]) * grid->region.width;
                                        if (dest >= 0 && size_t(dest) < cellCount) {
                                            occupied[size_t(dest)] = 1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    auto generated = generatePlateau(
                        *grid, input.config.seed, input.config.surface.plateauMinTop, brushes->shapes, occupied, stop,
                        input.config.surface.plateauMaxPercent
                    );
                    if (!generated) {
                        co_return fail(fromSurface(generated.error()));
                    }
                    std::vector<PlateauGrid> floors;
                    floors.push_back(std::move(*generated));
                    for (uint32_t level = 2; level <= input.config.surface.plateauLevels; ++level) {
                        auto next = generateNextPlateau(
                            floors.back(), input.config.seed ^ (uint64_t(level) << 48), brushes->shapes, stop,
                            input.config.surface.plateauMaxPercent
                        );
                        if (!next) {
                            co_return fail(fromSurface(next.error()));
                        }
                        if (!next->components) {
                            break;
                        }
                        floors.push_back(std::move(*next));
                    }
                    PreparedTerrain* lowerUnit = nullptr;
                    for (size_t level = 0; level < floors.size(); ++level) {
                        auto* generated = &floors[level];
                        auto& lowerPlan = lowerUnit ? lowerUnit->plan : plan;
                        auto upperConfig = input.config;
                        upperConfig.region.z -= int(level + 1);
                        if (!reserve(sizeof(PreparedTerrain) + planBytes)) {
                            co_return fail(PreparationInputError::MemoryLimit);
                        }
                        auto upperSnapshot = captureGenerationSnapshot(
                            map, upperConfig, {budget.maxCells, semanticBytes}, {budget.maxContextCells, remaining}, mask, stop
                        );
                        if (!upperSnapshot) {
                            co_return fail(upperSnapshot.error());
                        }
                        if (!reserve(upperSnapshot->estimatedBytes)) {
                            co_return fail(PreparationInputError::MemoryLimit);
                        }
                        SurfaceGrid upperGrid;
                        upperGrid.region = upperConfig.region;
                        upperGrid.cells.resize(cellCount, SurfaceKind::Preserve);
                        std::vector<uint8_t> clear(cellCount);
                        for (size_t i = 0; i < cellCount; ++i) {
                            clear[i] = generated->cells[i] == PlateauKind::None;
                            if (!clear[i]) {
                                upperGrid.cells[i] = SurfaceKind::Grass;
                            }
                        }
                        std::optional<DecorGrid> upperDecor;
                        if (input.config.vegetation) {
                            auto d = generatePlateauDecoration(
                                *generated, input.config.seed ^ (uint64_t(level) << 48), input.config.decor, stop
                            );
                            if (!d) {
                                co_return fail(fromSurface(d.error()));
                            }
                            upperDecor = std::move(*d);
                        }
                        auto upperProfile = profile;
                        upperProfile.brushes[0] = profile.plateauGround;
                        upperProfile.brushes[2] = profile.plateauGround;
                        upperProfile.wall.clear();
                        upperProfile.door.clear();
                        upperProfile.campfire.clear();
                        auto upperPlan = resolveSurfacePlan(
                            upperSnapshot->input, upperGrid, upperProfile, remaining, stop, upperDecor ? &*upperDecor : nullptr, clear
                        );
                        if (!upperPlan) {
                            co_return fail(upperPlan.error());
                        }
                        upperPlan->profile = profile;
                        upperPlan->plateauFloor.resize(cellCount);
                        upperPlan->walls.clear();
                        upperPlan->doors.clear();
                        upperPlan->fires.clear();
                        for (size_t i = 0; i < cellCount; ++i) {
                            upperPlan->plateauFloor[i] = generated->landings[i] ? 3
                                : generated->cells[i] == PlateauKind::Top       ? 1
                                : generated->cells[i] == PlateauKind::Edge      ? 2
                                                                                : 0;
                        }
                        lowerPlan.plateauRamps.assign(cellCount, 0);
                        for (const auto& ramp : generated->ramps) {
                            const auto& ids = brushes->items[ramp.shape];
                            const auto& shape = brushes->shapes[ramp.shape];
                            if (!lowerPlan.items[ramp.cell] || !upperPlan->items[ramp.destination]) {
                                co_return fail(PreparationInputError::InvalidParameters);
                            }
                            lowerPlan.plateauRamps[ramp.cell] = ids.ramp;
                            if (shape.hasTail) {
                                auto t = size_t(int64_t(ramp.cell) + shape.tailDx + int64_t(shape.tailDy) * grid->region.width);
                                if (!lowerPlan.items[t]) {
                                    co_return fail(PreparationInputError::InvalidParameters);
                                }
                                lowerPlan.plateauRamps[t] = ids.tail;
                            }
                        }
                        // Upper transitions clear lower vegetation around P and its tail before rebuilding its preview.
                        if (level && !lowerPlan.decor.empty()) {
                            for (const auto& ramp : generated->ramps) {
                                int x = int(ramp.cell % grid->region.width), y = int(ramp.cell / grid->region.width);
                                for (int yy = std::max(0, y - 1); yy <= std::min(int(grid->region.height) - 1, y + 1); ++yy) {
                                    for (int xx = std::max(0, x - 1); xx <= std::min(int(grid->region.width) - 1, x + 1); ++xx) {
                                        lowerPlan.decor[size_t(yy) * grid->region.width + xx] = 0;
                                    }
                                }
                            }
                        }
                        if (lowerUnit) {
                            auto revised = DrawOperations::prepareTerrainPreviewSteps(
                                map, lowerUnit->snapshot, lowerPlan, {budget.maxChanges, remaining}, stop
                            );
                            auto result = revised.advance();
                            while (!result) {
                                co_await std::suspend_always {};
                                result = revised.advance();
                            }
                            if (!*result) {
                                co_return fail(result->error());
                            }
                            if (!reserve((**result).getEstimatedPeakBytes())) {
                                co_return fail(PreparationInputError::MemoryLimit);
                            }
                            lowerUnit->preview = std::move(**result);
                        }
                        auto steps = DrawOperations::prepareTerrainPreviewSteps(
                            map, *upperSnapshot, *upperPlan, {budget.maxChanges, remaining}, stop
                        );
                        auto upper = steps.advance();
                        while (!upper) {
                            co_await std::suspend_always {};
                            upper = steps.advance();
                        }
                        if (!*upper) {
                            co_return fail(upper->error());
                        }
                        auto bytes = (**upper).getEstimatedPeakBytes();
                        if (!reserve(bytes)) {
                            co_return fail(PreparationInputError::MemoryLimit);
                        }
                        PreparationMemory memory {
                            sizeof(PreparedTerrain),
                            upperSnapshot->estimatedBytes,
                            0,
                            planBytes,
                            bytes,
                            sizeof(PreparedTerrain) + upperSnapshot->estimatedBytes + planBytes + bytes
                        };
                        auto unit = std::make_unique<PreparedTerrain>(
                            PreparedTerrain {std::move(*upperSnapshot), std::move(*upperPlan), std::move(**upper), memory}
                        );
                        auto* ptr = unit.get();
                        if (lowerUnit) {
                            lowerUnit->plateau = std::move(unit);
                        } else {
                            plateau = std::move(unit);
                        }
                        lowerUnit = ptr;
                    }
                }
            } else {
				auto grid = generateTerrain(input.config, input.extent, { budget.maxCells, semanticBytes },
					input.mask, input.protection, stop);
				if (!grid) co_return fail(grid.error());
				auto resolved = resolveGroundPlan(input, *grid, profile, planBytes, stop);
				if (!resolved) co_return fail(resolved.error());
				plan = std::move(*resolved);
			}
		}

		// The preview gets only what is left of the joint budget.
		co_await std::suspend_always{};
		auto steps = DrawOperations::prepareTerrainPreviewSteps(map, *snapshot, plan, { budget.maxChanges, remaining }, stop);
		auto advance = [&]() -> std::optional<std::expected<TerrainPreview, PreviewError>> {
			if (g_version.GetAssetRevision() != input.assetRevision) return std::unexpected(PreviewError::StaleSnapshot);
			return steps.advance();
		};
		auto outcome = advance();
		while (!outcome) {
			co_await std::suspend_always{};
			outcome = advance();
		}
		auto& preview = *outcome;
		if (!preview) co_return fail(preview.error());
		const uint64_t previewBytes = preview->getEstimatedPeakBytes();
		if (!reserve(previewBytes)) co_return fail(PreparationInputError::MemoryLimit);

		if (cave && (cave->preview.getChanges().size() > budget.maxChanges - preview->getChanges().size())) co_return fail(PreparationInputError::ChangeLimit);
        auto extra = cave ? cave->preview.getChanges().size() : 0;
        for (auto* p = plateau.get(); p; p = p->plateau.get()) {
            extra += p->preview.getChanges().size();
            if (!snapshotMatchesMap(map, p->snapshot)) {
                co_return fail(PreviewError::StaleSnapshot);
            }
        }
        if (extra > budget.maxChanges - preview->getChanges().size()) {
            co_return fail(PreparationInputError::ChangeLimit);
        }
        if (cave && !snapshotMatchesMap(map, cave->snapshot)) co_return fail(PreviewError::StaleSnapshot);

		// Check the live source again before returning a result to either caller.
		if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);

		if (!snapshotMatchesMap(map, *snapshot)) co_return fail(PreviewError::StaleSnapshot);

		const PreparationMemory memory {
			infrastructureBytes, snapshotBytes, semanticBytes, planBytes, previewBytes, budget.maxBytes - remaining
		};
        co_return PreparedTerrain {std::move(*snapshot), std::move(plan), std::move(*preview), memory, std::move(cave), std::move(plateau)};
    } catch (const std::bad_alloc&) {
		co_return fail(PreparationInputError::AllocationFailed);
	}
}

} // namespace

std::expected<TerrainPreparationSteps, PreparationError> prepareTerrainSteps(
	const Map& map, const GenerationConfig& config, const GroundProfile& profile,
	PreparationBudget budget, std::span<const uint8_t> mask, std::stop_token stop) {
	try {
		const auto limits = preflight(map, config, profile, budget, mask, stop);
		if (!limits) return fail(limits.error());
		return runPreparation(map, config, profile, budget, mask, stop, *limits);
	} catch (const std::bad_alloc&) {
		return fail(PreparationInputError::AllocationFailed);
	}
}

std::expected<PreparedTerrain, PreparationError> prepareTerrain(
	const Map& map, const GenerationConfig& config, const GroundProfile& profile,
	PreparationBudget budget, std::span<const uint8_t> mask, std::stop_token stop) {
	auto steps = prepareTerrainSteps(map, config, profile, budget, mask, stop);
	if (!steps) return fail(steps.error());
	return steps->finish();
}

} // namespace rme::procedural
