// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/terrain_preparation.h"
#include "procedural/adapter/door_types.h"

#include "app/managers/version_manager.h"
#include "brushes/ground/ground_brush.h"
#include "editor/operations/draw_operations.h"
#include "map/map.h"
#include "item_definitions/core/item_definition_store.h"
#include "procedural/adapter/snapshot_memory.h"
#include "procedural/adapter/surface_resolver.h"
#include "procedural/core/decoration.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>
#include <new>
#include <optional>
#include <system_error>
#include <thread>
#include <wx/thread.h>

namespace rme::procedural {
namespace {

constexpr uint64_t maxNameBytes = 256;

// Iteration 49: cells within this Chebyshev distance of an occupied z + 1 tile are never carved. Cave Floor then keeps
// 3 cells from it, its wall ring 2 and the ring's outer borders 1, so no brush or border pass touches the tile.
constexpr int32_t occupiedCaveClearance = 2;

// The cells of the area the cave must not carve: every tile on z + 1 that is not empty or is protected (the same
// test as the lower snapshot's protection, so nothing written there is refused later), widened by
// occupiedCaveClearance. Empty when z + 1 is free, so the cave is generated exactly as before.
std::vector<uint8_t> caveBlockedCells(const Map& map, const GenerationRegion& region, std::stop_token stop) {
	const auto w = region.width, h = region.height;
	std::vector<uint8_t> occupied;
	for (uint32_t y = 0; y < h; ++y) {
		if (stop.stop_requested()) return {};
		for (uint32_t x = 0; x < w; ++x) {
			const auto* location = map.getTileL(region.x + int(x), region.y + int(y), region.z + 1);
			if (!location) continue;
			const auto* tile = location->get();
			const bool taken = location->getSpawnCount() || location->getWaypointCount() || location->getTownCount() ||
				(location->getHouseExits() && !location->getHouseExits()->empty()) ||
				(tile && (!emptyTerrainContent(tile) || tile->statflags));
			if (!taken) continue;
			if (occupied.empty()) occupied.resize(size_t(w) * h);
			occupied[size_t(y) * w + x] = 1;
		}
	}
	if (occupied.empty()) return {};
	std::vector<uint8_t> blocked(occupied.size());
	for (uint32_t y = 0; y < h; ++y) for (uint32_t x = 0; x < w; ++x) {
		if (!occupied[size_t(y) * w + x]) continue;
		const auto y0 = y >= uint32_t(occupiedCaveClearance) ? y - occupiedCaveClearance : 0u;
		const auto x0 = x >= uint32_t(occupiedCaveClearance) ? x - occupiedCaveClearance : 0u;
		for (auto yy = y0; yy <= std::min(h - 1, y + occupiedCaveClearance); ++yy)
			for (auto xx = x0; xx <= std::min(w - 1, x + occupiedCaveClearance); ++xx) blocked[size_t(yy) * w + xx] = 1;
	}
	return blocked;
}

struct SurfaceStage {
	SurfaceGrid grid;
	std::optional<DecorGrid> decor;
};

using SurfaceResult = std::expected<SurfaceStage, SurfaceError>;

struct MountainStage {
	SurfaceGrid surface;
	std::vector<PlateauGrid> floors;
};

std::expected<MountainStage, SurfaceError> buildMountains(SurfaceGrid surface, const GenerationConfig& config,
	std::span<const ExitShape> shapes, std::span<const uint8_t> occupied, std::stop_token stop) {
	std::vector<uint8_t> reserved(occupied.begin(), occupied.end());
	if (config.surface.rockBlocks) {
		// Cap ramps only on the land connected to the start (iteration 44).
		reserved.resize(surface.cells.size());
		if (!occupyOutsideStartLand(surface, reserved, stop)) return std::unexpected(stop.stop_requested() ? SurfaceError::Cancelled : SurfaceError::AllocationFailed);
	}
	auto first = generatePlateau(surface, config.seed, config.surface.plateauMinTop, shapes, reserved, stop, config.surface.plateauMaxPercent);
	if (!first) return std::unexpected(first.error());
	std::vector<PlateauGrid> floors;
	floors.push_back(std::move(*first));
	for (uint32_t level = 2; level <= config.surface.plateauLevels; ++level) {
		auto next = generateNextPlateau(floors.back(), config.seed ^ (uint64_t(level) << 48), shapes, stop, config.surface.plateauMaxPercent);
		if (!next) return std::unexpected(next.error());
		if (!next->components) break;
		floors.push_back(std::move(*next));
	}
	return MountainStage{std::move(surface), std::move(floors)};
}

SurfaceResult buildSurface(const GenerationConfig& config, MapExtent extent, SurfaceBudget budget,
	std::span<const uint8_t> mask, std::span<const uint8_t> protection, std::stop_token stop) {
	auto grid = generateSurface({config.region, config.seed, config.surface}, extent, budget, mask, protection, stop);
	if (!grid) return std::unexpected(grid.error());
	std::optional<DecorGrid> decor;
	if (config.vegetation) {
		auto generated = generateDecoration(*grid, config.seed, config.decor, stop);
		if (!generated) return std::unexpected(generated.error());
		decor = std::move(*generated);
	}
	return SurfaceStage{std::move(*grid), std::move(decor)};
}

// Only owned, plain generation data crosses the thread boundary. Destruction requests stop
// and joins before releasing the result; no callback can outlive the continuation.
template<typename Result>
class CoreTask {
public:
	template<typename Work>
	explicit CoreTask(Work work) : worker([this, work = std::move(work)](std::stop_token stop) mutable {
			try {
				result.emplace(work(stop));
			} catch (...) {
				exception = std::current_exception();
			}
			ready.store(true, std::memory_order_release);
		}) {}

	~CoreTask() { worker.request_stop(); }
	bool done() const noexcept { return ready.load(std::memory_order_acquire); }
	Result take() {
		if (exception) std::rethrow_exception(exception);
		return std::move(*result);
	}

private:
	std::optional<Result> result;
	std::exception_ptr exception;
	std::atomic<bool> ready = false;
	std::jthread worker; // Destroy first: join before touching the other members.
};

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
	PreparationBudget budget, std::span<const uint8_t> mask, std::stop_token stop, bool background) {
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
    if (!validPlateauProfile(profile, config) || !validCaveProfile(profile, config) || !validStoreyProfile(profile, config) ||
        !validDungeonProfile(profile, config)) {
        return fail(PreparationInputError::InvalidProfile);
    }
    const bool storeys = config.surface.buildingStoreys;
    // Dungeons live on the cave floor and avoid its blocked cells (occupied z + 1 tiles, iteration 49).
    const bool dungeons = config.surface.buildingDungeons;
    if (dungeons && !resolveDungeonBrushes(profile)) return fail(PreparationInputError::InvalidProfile);
    if (storeys) {
        if (!resolveBuildingStairsBrush(profile.buildingStairs)) return fail(PreparationInputError::InvalidProfile);
        if (region.z < 1) return fail(PreparationInputError::NoUpperFloor);
        // With mountains the z - 1 check below covers the storeys as well.
        for (uint32_t y = 0; !hasPlateau(profile) && y < region.height; ++y) {
            if (stop.stop_requested()) return fail(PreparationInputError::Cancelled);
            for (uint32_t x = 0; x < region.width; ++x) {
                if (!emptyTerrainContent(map.getTile(Position(region.x + int(x), region.y + int(y), region.z - 1))))
                    return fail(PreparationInputError::UpperFloorOccupied);
            }
        }
    }
    // Occupied tiles on z + 1 do not refuse a cave: the cave leaves them Solid (iteration 49, caveBlockedCells).
    if (hasCave(profile) && region.z >= MAP_MAX_LAYER) return fail(PreparationInputError::NoLowerFloor);
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
	for (const auto* name : {&profile.grassMix[0], &profile.grassMix[1], &profile.grassMix[2], &profile.treeMix[0], &profile.treeMix[1], &profile.treeMix[2], &profile.bushMix[0], &profile.bushMix[1], &profile.bushMix[2]}) {
		if (name->empty()) continue;
		if (!validName(name->size())) return fail(PreparationInputError::InvalidProfile);
		profileBytes += stringCopyBytes(name->size());
	}
	for (const auto& names : profile.grassMixDecor) for (const auto& name : names) {
		if (name.empty()) continue;
		if (!validName(name.size())) return fail(PreparationInputError::InvalidProfile);
		profileBytes += stringCopyBytes(name.size());
	}
	for (const auto* name : {&profile.caveEntrance, &profile.caveExit, &profile.caveGround, &profile.caveWall, &profile.caveDoor, &profile.bridgeBrush, &profile.buildingStairs,
		&profile.dungeonFloor, &profile.dungeonWall, &profile.dungeonStairs, &profile.dungeonDescent}) {
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
	const uint64_t planBytes = fixedPlanBytes + cellCount * sizeof(uint16_t) * ((vegetation ? 2 : 1) + (profile.wall.empty() ? 0 : 1) + (profile.campfire.empty() ? 0 : 1) + (hasCave(profile) ? 2 : 0)) + (profile.door.empty() ? 0 : cellCount)
        + (config.surface.caveDoorCorridors || dungeons ? cellCount * 3 : hasCave(profile) && caveWallBrushOf(profile) ? cellCount * 2 : 0)
        + (storeys ? cellCount * 2 : 0);

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
		if (background) {
			// The worker owns a mask copy; combined protection is moved into it.
			if (cellCount > UINT64_MAX - semanticBytes - kSnapshotAllocationOverhead) return fail(PreparationInputError::MemoryLimit);
			semanticBytes += cellCount + kSnapshotAllocationOverhead;
		}
		if (hasCave(profile)) {
			// Cave cells, exit search occupancy, landings, queues, brush geometry and retained sparse plan.
			// Conservative bound; charged before generating and never refunded.
			if (cellCount > (UINT64_MAX - semanticBytes - 65536) / 128) return fail(PreparationInputError::MemoryLimit);
			semanticBytes += cellCount * 128 + 65536;
		}
        if (config.surface.repairReach || config.surface.passes || storeys || dungeons) {
            // Expanded water-run Dijkstra states (distance, predecessor, heap),
            // full world copies, and provisional resolver plans. Charged before allocation.
            const auto perCell = 1024ULL + 64ULL * config.surface.fordLength;
            if (cellCount > (UINT64_MAX - semanticBytes - 65536) / perCell) return fail(PreparationInputError::MemoryLimit);
            semanticBytes += cellCount * perCell + 65536;
        }
        if (config.surface.passes) {
            // Direction/run Dijkstra states, heap, masks and retained counterfactuals.
            if (cellCount > (UINT64_MAX - semanticBytes - 65536) / 1024) return fail(PreparationInputError::MemoryLimit);
            semanticBytes += cellCount * 1024 + 65536;
        }
        if (hasPlateau(profile) || storeys) {
            if (cellCount > (UINT64_MAX - semanticBytes - 65536) / 192) {
                return fail(PreparationInputError::MemoryLimit);
            }
            semanticBytes += (cellCount * 192 + 65536) * (hasPlateau(profile) ? config.surface.plateauLevels : 1);
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
	// By value: the coroutine frame outlives the caller's effective config (iteration 44).
	const Map& map, GenerationConfig config, const GroundProfile& profile,
	PreparationBudget budget, std::span<const uint8_t> mask, std::stop_token stop, PreparationPreflight limits, bool background) {
	if (!wxIsMainThread()) co_return fail(PreparationInputError::WrongThread);
	if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
	if (g_version.GetAssetRevision() != limits.assetRevision) co_return fail(PreviewError::StaleSnapshot);
	try {
		const auto cellCount = limits.cellCount;
        std::optional<CandidateMetrics> candidateMetrics;
        CopyAssessment copyAssessment;
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
        // Conservative allowance for channel/patch measurement temporaries, charged before allocation.
        if (budget.measureCandidate && !reserve(cellCount * 512 + 65536)) co_return fail(PreparationInputError::MemoryLimit);
        if (budget.copyIndex && !reserve(copyWorkingBytes(cellCount))) co_return fail(PreparationInputError::MemoryLimit);

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
        std::unique_ptr<PreparedTerrain::Levels> levels;
        {
			if (input.config.algorithm == GenerationAlgorithm::Surface) {
				const auto keepClear = surfaceKeepClear(*snapshot, profileNeedsEdgeClearance(profile));
				std::vector<uint8_t> protection = input.protection;
				for (size_t i = 0; i < protection.size(); ++i) protection[i] |= keepClear[i];
				const SurfaceBudget surfaceBudget{budget.maxCells, semanticBytes, semanticBytes};
				SurfaceResult generated = std::unexpected(SurfaceError::Cancelled);
				if (background) {
					CoreTask<SurfaceResult> task([config = input.config, extent = input.extent, surfaceBudget,
						mask = input.mask, protection = std::move(protection)](std::stop_token token) {
						return buildSurface(config, extent, surfaceBudget, mask, protection, token);
					});
					while (!task.done()) {
						if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
						co_await std::suspend_always{};
					}
					generated = task.take();
				} else {
					generated = buildSurface(input.config, input.extent, surfaceBudget, input.mask, protection, stop);
				}
				if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
				if (g_version.GetAssetRevision() != input.assetRevision) co_return fail(PreviewError::StaleSnapshot);
				if (!generated) co_return fail(fromSurface(generated.error()));
				auto* grid = &generated->grid;
				auto& decor = generated->decor;
                std::optional<CaveGrid> levelCave;
                CaveExitPlan levelExits;
                std::vector<ExitShape> levelShapes;
                // Iteration 48c: the ramp edge (cave cell under a hole, its surface destination) of every hole with walkable
                // ground below instead of a ramp.
                std::vector<LevelEdge> holeLandings;
                std::vector<PlateauGrid> levelCaps;
                std::optional<LevelWorld> repairWorld;
                LevelRepairReport repairReport;
                std::vector<MountainPass> passes;
                std::vector<BuildingStorey> levelStoreys;
                CaveExitBrush stairBrush;
                if (input.config.surface.buildingStoreys) {
                    auto resolved = resolveBuildingStairsBrush(profile.buildingStairs);
                    if (!resolved) co_return fail(resolved.error());
                    stairBrush = std::move(*resolved);
                }
                // Walkable ground under a rock-face entrance when the rock brush is impassable: a surface brush of the profile.
                uint16_t entranceGround = 0;
                if (const auto* dirt = g_brushes.getBrush(profile.brushes[1]); dirt && dirt->as<GroundBrush>()) {
                    const auto id = dirt->as<GroundBrush>()->getPlacementItemId();
                    if (id && !g_item_definitions.get(id).hasFlag(ItemFlag::Unpassable)) entranceGround = id;
                }
                const bool walkableRockFace = input.config.surface.rockBlocks && entranceGround &&
                    input.config.surface.caveEntranceStyle == CaveEntranceStyle::RockFace;
                std::vector<BuildingDungeon> levelDungeons;
                // Read here, in the same uninterrupted step that captures the lower snapshot below.
                std::vector<uint8_t> caveBlocked;
                if (hasCave(profile)) {
                    caveBlocked = caveBlockedCells(map, input.config.region, stop);
                    if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
                    // The mask and its copies in the cave grids (generated, repair world, level view).
                    if (!caveBlocked.empty() && !reserve(4 * (cellCount + kSnapshotAllocationOverhead))) co_return fail(PreparationInputError::MemoryLimit);
                }
                std::optional<DungeonBrushes> dungeonBrushes;
                if (input.config.surface.buildingDungeons) {
                    auto resolved = resolveDungeonBrushes(profile);
                    if (!resolved) co_return fail(resolved.error());
                    dungeonBrushes = std::move(*resolved);
                }
                if (input.config.surface.repairReach || input.config.surface.passes || input.config.surface.buildingStoreys ||
                    input.config.surface.buildingDungeons) {
                    std::vector<ExitShape> caveShapes, capShapes;
                    if (hasCave(profile)) {
                        auto brushes = resolveCaveBrushes(profile, input.config);
                        if (!brushes) co_return fail(brushes.error());
                        caveShapes = brushes->shapes;
                    }
                    if (hasPlateau(profile)) {
                        auto brushes = resolvePlateauBrushes(profile, input.config);
                        if (!brushes) co_return fail(brushes.error());
                        capShapes = brushes->shapes;
                    }
                    auto work = [surface = std::move(*grid), decoration = decor ? std::move(*decor) : DecorGrid{},
                        seed = input.config.seed, tuning = input.config.surface, decorTuning = input.config.decor,
                        caveShapes, capShapes, vegetation = input.config.vegetation, stairShapes = stairBrush.shapes,
                        dungeonShapes = dungeonBrushes ? dungeonBrushes->stairs.shapes : std::vector<ExitShape>{}, caveBlocked](std::stop_token token) mutable {
                        return buildLevelWorld(std::move(surface), std::move(decoration), seed, tuning, decorTuning,
                            caveShapes, capShapes, vegetation, token, stairShapes, dungeonShapes, caveBlocked);
                    };
                    std::expected<LevelWorld, ReachabilityError> world = std::unexpected(ReachabilityError::Cancelled);
                    if (background) {
                        CoreTask<decltype(world)> task(std::move(work));
                        while (!task.done()) {
                            if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
                            co_await std::suspend_always{};
                        }
                        world = task.take();
                    } else world = work(stop);
                    if (!world) {
                        if (world.error() == ReachabilityError::AllocationFailed) co_return fail(PreparationInputError::AllocationFailed);
                        co_return fail(world.error() == ReachabilityError::Cancelled ? PreparationInputError::Cancelled : PreparationInputError::InvalidParameters);
                    }
                    passes = world->passes;
                    // Resolve the actual seeded variants on every decorated floor.
                    // Offline measurement deliberately uses conservative class flags instead.
                    const size_t capBase = world->cave ? 2 : 1;
                    std::vector<uint8_t> blocking(cellCount * world->decor.size());
                    bool rockBlocks = false;
                    for (size_t f = 0; f < world->decor.size(); ++f) {
                        // The cave and a storey-only z - 1 have no resolved vegetation or blocking ground here.
                        if ((world->cave && f == 1) || (f >= capBase && f - capBase >= world->caps.size())) continue;
                        auto floorInput = input;
                        auto floorProfile = profile;
                        auto floorGrid = world->surface;
                        std::vector<uint8_t> clear = keepClear;
                        if (f) {
                            const auto& cap = world->caps[f - capBase];
                            floorGrid = {}; floorGrid.region = cap.region;
                            floorGrid.cells.resize(cellCount, SurfaceKind::Preserve);
                            clear.resize(cellCount);
                            for (size_t i = 0; i < cellCount; ++i) {
                                clear[i] = cap.cells[i] == PlateauKind::None;
                                if (!clear[i]) floorGrid.cells[i] = SurfaceKind::Grass;
                            }
                            floorInput.config.region = cap.region;
                            floorProfile.brushes[0] = profile.plateauGround;
                            floorProfile.wall.clear(); floorProfile.door.clear(); floorProfile.campfire.clear();
                            floorProfile.grassMix = {}; floorProfile.grassMixPermille = {};
                            floorProfile.grassMixDecorKeep = GroundProfile {}.grassMixDecorKeep; floorProfile.grassMixDecor = {}; floorProfile.treeMix = {}; floorProfile.treeMixPermille = {}; floorProfile.bushMix = {}; floorProfile.bushMixPermille = {};
                        }
                        auto resolved = resolveSurfacePlan(floorInput, floorGrid, floorProfile, semanticBytes, stop,
                            input.config.vegetation ? &world->decor[f] : nullptr, clear);
                        if (!resolved) co_return fail(resolved.error());
                        for (size_t i = 0; i < cellCount; ++i) {
                            const bool groundBlocks = resolved->items[i] && g_item_definitions.get(resolved->items[i]).hasFlag(ItemFlag::Unpassable);
                            blocking[f * cellCount + i] = groundBlocks ? 2 : (!resolved->decor.empty() && resolved->decor[i] &&
                                g_item_definitions.get(resolved->decor[i]).hasFlag(ItemFlag::Unpassable)) ? 1 : 0;
                            if (!f && floorGrid.cells[i] == SurfaceKind::Rock) rockBlocks |= groundBlocks;
                        }
                    }
                    // A rock-face entrance gets the walkable dirt ground below (iteration 44), not the rock ground resolved
                    // here; marking its rock ground as blocking made every such entrance unreachable, so the repair
                    // removed all of them together with their caves.
                    if (walkableRockFace) for (const auto e : world->surface.entrances)
                        if (world->surface.cells[e] == SurfaceKind::Rock && blocking[e] == 2) blocking[e] = 0;
                    if (input.config.surface.repairReach) {
                        auto work = [ownedWorld = std::move(*world), walls = !profile.wall.empty(), rockBlocks,
                            caveShapes, fordLength = input.config.surface.fordLength, bridges = !profile.bridgeBrush.empty(),
                            blocking = std::move(blocking)](std::stop_token token) mutable {
                            auto repaired = repairLevelReach(ownedWorld, walls, rockBlocks, caveShapes, {}, fordLength, bridges, blocking, token);
                            return std::pair(std::move(ownedWorld), std::move(repaired));
                        };
                        using RepairResult = decltype(work(stop));
                        std::optional<RepairResult> result;
                        if (background) {
                            CoreTask<RepairResult> task(std::move(work));
                            while (!task.done()) {
                                if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
                                co_await std::suspend_always{};
                            }
                            result = task.take();
                        } else result = work(stop);
                        *world = std::move(result->first);
                        auto repaired = std::move(result->second);
                        if (!repaired) co_return fail(repaired.error() == ReachabilityError::Cancelled ? PreparationInputError::Cancelled : PreparationInputError::InvalidParameters);
                        repairReport = *repaired;
                    }
                    *grid = std::move(world->surface);
                    if (decor) *decor = world->decor[0];
                    levelStoreys = world->storeys;
                    levelDungeons = world->dungeons;
                    repairWorld = std::move(*world);
                }
				const auto resolve = [&]() { return resolveSurfacePlan(input, *grid, profile, planBytes, stop, decor ? &*decor : nullptr, keepClear); };
				if (hasCave(profile)) {
					auto brushes = resolveCaveBrushes(profile, input.config);
					if (!brushes) co_return fail(brushes.error());
                    struct CaveStage { SurfaceGrid surface; CaveGrid cave; CaveExitPlan exits; };
                    using CaveResult = std::expected<CaveStage, PreparationError>;
                    CaveResult stage = std::unexpected(PreparationInputError::Cancelled);
                    if (repairWorld) stage = CaveStage{std::move(*grid), *repairWorld->cave, repairWorld->exits};
                    else {
                        auto work = [surface = std::move(*grid), seed = input.config.seed, tuning = input.config.surface,
                            shapes = brushes->shapes, decoration = decor ? *decor : DecorGrid{}, caveBlocked](std::stop_token token) mutable -> CaveResult {
                            auto generated = generateCave(surface, seed, token, nullptr, caveBlocked);
                            if (!generated) return fail(fromSurface(generated.error()));
                            std::vector<uint8_t> occupied;
                            if (!decoration.cells.empty()) {
                                occupied.resize(decoration.cells.size());
                                for (size_t i = 0; i < occupied.size(); ++i) occupied[i] = decoration.cells[i] != DecorKind::None;
                            }
                            const auto caveFailure = [](UndergroundError error) {
                                switch (error) {
                                    case UndergroundError::Cancelled: return PreparationInputError::Cancelled;
                                    case UndergroundError::AllocationFailed: return PreparationInputError::AllocationFailed;
                                    case UndergroundError::InvalidGrid: return PreparationInputError::InvalidParameters;
                                }
                                return PreparationInputError::InvalidParameters;
                            };
                            auto exits = placeCaveExits(surface, *generated, shapes, occupied, token);
                            if (!exits) return fail(caveFailure(exits.error()));
                            if (!caveBlocked.empty()) {
                                const auto removed = removeUnenteredCaveComponents(*generated, *exits, token);
                                if (!removed) return fail(caveFailure(removed.error()));
                            }
                            if (tuning.caveFarExits) {
                                const auto placed = placeFarCaveExits(surface, *generated, *exits, shapes, seed, occupied, token);
                                if (!placed) return fail(caveFailure(placed.error()));
                            }
                            if (tuning.caveDoorCorridors) {
                                const auto placed = placeCaveDoorCorridors(*generated, *exits, shapes, token);
                                if (!placed) return fail(caveFailure(placed.error()));
                            }
                            return CaveStage{std::move(surface), std::move(*generated), std::move(*exits)};
                        };
                        // Keep resolved vegetation unchanged: the cave worker needs only its occupied mask.
                        // The owned decoration above is a copy, charged by semantic working memory.
                        if (background) {
                            CoreTask<CaveResult> task(std::move(work));
                            while (!task.done()) {
                                if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
                                co_await std::suspend_always{};
                            }
                            stage = task.take();
                        } else stage = work(stop);
                    }
                    if (!stage) co_return fail(stage.error());
                    *grid = std::move(stage->surface);
                    auto generated = std::optional<CaveGrid>(std::move(stage->cave));
                    auto exits = std::optional<CaveExitPlan>(std::move(stage->exits));
					levelCave = *generated;
                    levelExits = *exits;
                    levelShapes = brushes->shapes;
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
                    // Iteration 50: a wall-brush cave wall stands on cave ground on every wall band cell; RME's wall
                    // logic in the preview aligns the pieces, so the band shows a wall towards the floor on every side.
                    if (brushes->wallPiece) { lowerPlan.walls.assign(cellCount, 0); lowerPlan.doors.assign(cellCount, 0); }
					for (size_t i = 0; i < cellCount; ++i) {
						const bool floor = generated->cells[i] == CaveKind::Floor;
						bool wall = false;
						const int x = int(i % grid->region.width), y = int(i / grid->region.width);
						if (!floor) for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
							if (x + dx >= 0 && y + dy >= 0 && x + dx < int(grid->region.width) && y + dy < int(grid->region.height))
								wall |= generated->cells[size_t(y + dy) * grid->region.width + size_t(x + dx)] == CaveKind::Floor;
						}
						lowerPlan.caveFloor[i] = floor ? 1 : wall ? 2 : 0;
						if ((floor || wall) && lowerSnapshot->input.mask[i] && !lowerSnapshot->input.protection[i]) {
							lowerPlan.items[i] = floor ? brushes->ground : brushes->wall;
							if (wall && brushes->wallPiece) lowerPlan.walls[i] = brushes->wallPiece;
						}
					}
					for (const auto& exit : exits->exits) {
                        const auto p = exit.cell;
                        if (!lowerPlan.items[p]) continue;
						const auto& ids = brushes->items[exit.shape];
						const auto& shape = brushes->shapes[exit.shape];
						const auto tail = size_t(int64_t(p) + shape.tailDx + int64_t(shape.tailDy) * grid->region.width);
						if (shape.hasTail && !lowerPlan.items[tail]) continue;
                        constexpr int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
                        const auto d = size_t(shape.direction);
                        const auto arrival = size_t(int64_t(p) + dx[d] + int64_t(dy[d]) * grid->region.width);
                        bool hole = false;
                        if (!exit.distant) {
                            const auto e = grid->entrances[exit.entrance];
                            if (!plan.items[e]) continue;
                            plan.entrances[e] = brushes->entrance;
                            if (input.config.surface.caveEntranceStyle == CaveEntranceStyle::FloorHole) {
                                plan.items[e] = plan.entrances[e];
                                hole = true;
                            } else if (grid->cells[e] != SurfaceKind::Rock && brushes->hole) {
                                // An ordinary entrance off the rock (iteration 48) is a floor hole, not the rock-face doodad
                                // standing on grass (iteration 48b).
                                plan.items[e] = plan.entrances[e] = brushes->hole;
                                hole = true;
                            } else if (input.config.surface.rockBlocks && entranceGround && grid->cells[e] == SurfaceKind::Rock) {
                                // Impassable rock: the rock-face entrance stands on the profile's dirt ground (iteration 44).
                                plan.items[e] = entranceGround;
                                if (!plan.decor.empty()) plan.decor[e + grid->region.width] = 0;
                            }
                            if (!plan.decor.empty()) plan.decor[e] = 0;
                        }
                        if (exit.distant && !plan.decor.empty()) plan.decor[arrival] = 0;
                        if (hole && brushes->landing && lowerPlan.caveFloor[p] == 1) {
                            // Under a hole the cave has walkable ground, not a ramp (iteration 48c).
                            lowerPlan.items[p] = brushes->landing;
                            holeLandings.push_back({{1, uint32_t(p)}, {0, uint32_t(arrival)}});
                            continue;
                        }
						lowerPlan.exits[p] = ids.ramp;
						if (shape.hasTail) lowerPlan.exits[tail] = ids.tail;
					}
                    if (input.config.surface.caveDoorCorridors) {
                        auto door = resolveCaveDoorBrush(profile.caveDoor);
                        if (!door) co_return fail(door.error());
                        if (lowerPlan.walls.empty()) { lowerPlan.walls.assign(cellCount, 0); lowerPlan.doors.assign(cellCount, 0); }
                        for (const auto& c : generated->corridors) {
                            const auto side = c.horizontal ? grid->region.width : 1u;
                            for (auto i : {c.door - side, c.door, c.door + side}) lowerPlan.walls[i] = door->wall;
                            lowerPlan.doors[c.door] = 1;
                        }
                    }
                    if (dungeonBrushes) {
                        // Dungeons (iteration 45): floor under every walkable and wall cell, walls on the ring, doors in the
                        // room walls, stairs up on z + 1 and their opening on the surface, all at the core's cells.
                        const auto w = grid->region.width, h = grid->region.height;
                        if (lowerPlan.walls.empty()) { lowerPlan.walls.assign(cellCount, 0); lowerPlan.doors.assign(cellCount, 0); }
                        std::vector<uint8_t> walk(cellCount);
                        for (const auto& d : levelDungeons) if (!markDungeonCells(d, w, h, walk)) co_return fail(PreparationInputError::InvalidParameters);
                        for (size_t i = 0; i < cellCount; ++i) {
                            bool wall = false, side = false;
                            const int x = int(i % w), y = int(i / w);
                            if (!walk[i]) for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                                if (x + dx >= 0 && y + dy >= 0 && x + dx < int(w) && y + dy < int(h)) {
                                    const bool open = walk[size_t(y + dy) * w + size_t(x + dx)] != 0;
                                    wall |= open;
                                    side |= open && (dx == 0 || dy == 0);
                                }
                            if (!walk[i] && !wall) continue;
                            if (!lowerSnapshot->input.mask[i] || lowerSnapshot->input.protection[i] || lowerPlan.caveFloor[i])
                                co_return fail(PreparationInputError::InvalidParameters);
                            // A corner of the ring touches the dungeon only diagonally: its wall piece (a pole at the NE and
                            // SW corners) leaves the tile open, so it stands without floor (iteration 45b).
                            lowerPlan.caveFloor[i] = walk[i] ? 3 : side ? 4 : 5;
                            lowerPlan.items[i] = walk[i] || side ? dungeonBrushes->floor : 0;
                            if (wall) lowerPlan.walls[i] = dungeonBrushes->wall.wall;
                        }
                        for (const auto& d : levelDungeons) {
                            for (const auto& c : d.corridors) for (const auto door : {c.doorA, c.doorB}) {
                                lowerPlan.caveFloor[door] = 4;
                                lowerPlan.walls[door] = dungeonBrushes->wall.wall;
                                lowerPlan.doors[door] = 1;
                            }
                            const auto& shape = dungeonBrushes->stairs.shapes[d.shape];
                            const auto& ids = dungeonBrushes->stairs.items[d.shape];
                            const auto tail = size_t(int64_t(d.descent) + shape.tailDx + int64_t(shape.tailDy) * w);
                            if (!plan.items[d.descent]) co_return fail(PreparationInputError::InvalidParameters);
                            lowerPlan.exits[d.descent] = ids.ramp;
                            if (shape.hasTail) lowerPlan.exits[tail] = ids.tail;
                            // Visible stairs down replace the building floor (iteration 45b).
                            plan.entrances[d.descent] = plan.items[d.descent] = dungeonBrushes->descent;
                            if (!plan.decor.empty()) plan.decor[d.descent] = 0;
                        }
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
                if (input.config.surface.buildingStoreys) {
                    // Ground-floor stairs (and tail) stand on the building floor, drawn like ramp items.
                    plan.stairs.assign(cellCount, 0);
                    for (const auto& s : levelStoreys) {
                        const auto& shape = stairBrush.shapes[s.shape];
                        const auto& ids = stairBrush.items[s.shape];
                        const auto tail = size_t(int64_t(s.stairs) + shape.tailDx + int64_t(shape.tailDy) * grid->region.width);
                        if (!plan.items[s.stairs] || (shape.hasTail && !plan.items[tail])) co_return fail(PreparationInputError::InvalidParameters);
                        plan.stairs[s.stairs] = ids.ramp;
                        if (shape.hasTail) plan.stairs[tail] = ids.tail;
                    }
                }
                if (hasPlateau(profile) || !levelStoreys.empty()) {
                    const bool mountains = hasPlateau(profile);
                    CaveExitBrush brushes;
                    if (mountains) {
                        auto resolved = resolvePlateauBrushes(profile, input.config);
                        if (!resolved) co_return fail(resolved.error());
                        brushes = std::move(*resolved);
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
                        // The surface cell a hole's ramp would lead to stays reserved, so the caps stay as with the ramp.
                        for (const auto& landing : holeLandings) occupied[landing.b.cell] = 1;
                    }
                    auto work = [surface = repairWorld ? *grid : std::move(*grid), config = input.config, shapes = brushes.shapes,
                                 occupied = std::move(occupied)](std::stop_token token) mutable {
                        return buildMountains(std::move(surface), config, shapes, occupied, token);
                    };
                    std::expected<MountainStage, SurfaceError> geometry = std::unexpected(SurfaceError::Cancelled);
                    if (repairWorld) {
                        geometry = MountainStage{std::move(*grid), std::move(repairWorld->caps)};
                    } else if (background) {
                        CoreTask<decltype(geometry)> task(std::move(work));
                        while (!task.done()) {
                            if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
                            co_await std::suspend_always{};
                        }
                        geometry = task.take();
                    } else {
                        geometry = work(stop);
                    }
                    if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
                    if (g_version.GetAssetRevision() != input.assetRevision) co_return fail(PreviewError::StaleSnapshot);
                    if (!geometry) co_return fail(fromSurface(geometry.error()));
                    *grid = std::move(geometry->surface);
                    levelCaps = std::move(geometry->floors);
                    auto& floors = levelCaps;
                    PreparedTerrain* lowerUnit = nullptr;
                    // Storeys share z - 1 with the first cap; without mountains they get that floor to themselves.
                    PlateauGrid noCap;
                    noCap.region = input.config.region;
                    --noCap.region.z;
                    noCap.cells.assign(cellCount, PlateauKind::None);
                    noCap.landings.assign(cellCount, 0);
                    std::vector<uint8_t> storeyCell(cellCount); // 1 interior, 2 outline
                    for (const auto& s : levelStoreys) for (uint32_t y = s.y0; y <= s.y1; ++y) for (uint32_t x = s.x0; x <= s.x1; ++x)
                        storeyCell[size_t(y) * grid->region.width + x] = x == s.x0 || x == s.x1 || y == s.y0 || y == s.y1 ? 2 : 1;
                    const size_t upperLevels = std::max(floors.size(), levelStoreys.empty() ? size_t(0) : size_t(1));
                    for (size_t level = 0; level < upperLevels; ++level) {
                        auto* generated = level < floors.size() ? &floors[level] : &noCap;
                        const bool withStoreys = level == 0 && !levelStoreys.empty();
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
                            const bool storey = withStoreys && storeyCell[i];
                            clear[i] = generated->cells[i] == PlateauKind::None && !storey;
                            if (!clear[i]) {
                                upperGrid.cells[i] = storey ? SurfaceKind::Floor : SurfaceKind::Grass;
                            }
                        }
                        std::optional<DecorGrid> upperDecor;
                        if (input.config.vegetation && !mountains) {
                            upperDecor = DecorGrid{upperConfig.region, std::vector<DecorKind>(cellCount, DecorKind::None)};
                        } else if (input.config.vegetation) {
                            auto d = generatePlateauDecoration(
                                *generated, input.config.seed ^ (uint64_t(level) << 48), input.config.decor, stop
                            );
                            if (!d) {
                                co_return fail(fromSurface(d.error()));
                            }
                            upperDecor = repairWorld ? std::move(repairWorld->decor[(cave ? 2 : 1) + level]) : std::move(*d);
                        }
                        auto upperProfile = profile;
                        if (mountains) {
                            upperProfile.brushes[0] = profile.plateauGround;
                            upperProfile.brushes[2] = profile.plateauCliff;
                        }
                        // Storey outlines keep the building's wall brush; caps have none.
                        if (!withStoreys) upperProfile.wall.clear();
                        upperProfile.door.clear();
                        upperProfile.campfire.clear();
                        // Caps and storeys use their own ground; the learned mixes belong to the surface below.
                        upperProfile.grassMix = {}; upperProfile.grassMixPermille = {};
                        upperProfile.grassMixDecorKeep = GroundProfile {}.grassMixDecorKeep; upperProfile.grassMixDecor = {}; upperProfile.treeMix = {}; upperProfile.treeMixPermille = {}; upperProfile.bushMix = {}; upperProfile.bushMixPermille = {};
                        auto upperPlan = resolveSurfacePlan(
                            upperSnapshot->input, upperGrid, upperProfile, remaining, stop, upperDecor ? &*upperDecor : nullptr, clear
                        );
                        if (!upperPlan) {
                            co_return fail(upperPlan.error());
                        }
                        upperPlan->profile = profile;
                        upperPlan->plateauFloor.resize(cellCount);
                        if (!withStoreys) upperPlan->walls.clear();
                        upperPlan->doors.clear();
                        upperPlan->fires.clear();
                        for (size_t i = 0; i < cellCount; ++i) {
                            upperPlan->plateauFloor[i] = generated->cells[i] == PlateauKind::Top ? 1 : withStoreys && storeyCell[i] ? 4 : 0;
                        }
                        for (const auto& s : withStoreys ? std::span<const BuildingStorey>(levelStoreys) : std::span<const BuildingStorey>{}) {
                            if (!upperPlan->items[s.stairs] || !upperPlan->items[s.arrival]) co_return fail(PreparationInputError::InvalidParameters);
                            upperPlan->items[s.stairs] = stairBrush.items[s.shape].opening;
                            upperPlan->plateauFloor[s.stairs] = 5;
                        }
                        if (mountains) {
                            // Resolve cliff variants with the existing ground resolver, then use the normal border/undo path.
                            auto cliffGrid = upperGrid;
                            cliffGrid.region = lowerPlan.config.region;
                            auto cliffClear = clear;
                            for (size_t i = 0; i < cellCount; ++i) {
                                if (cliffGrid.cells[i] == SurfaceKind::Grass) cliffGrid.cells[i] = SurfaceKind::Rock;
                                else if (cliffGrid.cells[i] == SurfaceKind::Floor) { cliffGrid.cells[i] = SurfaceKind::Preserve; cliffClear[i] = 1; }
                            }
                            auto cliffProfile = upperProfile;
                            cliffProfile.brushes[2] = profile.plateauCliff;
                            cliffProfile.wall.clear();
                            auto cliffInput = upperSnapshot->input;
                            cliffInput.config.region = lowerPlan.config.region;
                            cliffInput.config.vegetation = false;
                            cliffProfile.decorCount = 0;
                            auto cliffs = resolveSurfacePlan(cliffInput, cliffGrid, cliffProfile, remaining, stop, nullptr, cliffClear);
                            if (!cliffs) co_return fail(cliffs.error());
                            for (size_t i = 0; i < cellCount; ++i) if (generated->cells[i] == PlateauKind::Top) {
                                lowerPlan.items[i] = cliffs->items[i];
                                if (lowerUnit) lowerPlan.plateauFloor[i] = 2;
                                if (!lowerPlan.decor.empty()) lowerPlan.decor[i] = 0;
                            }
                            lowerPlan.plateauRamps.assign(cellCount, 0);
                        }
                        for (const auto& ramp : generated->ramps) {
                            const auto& ids = brushes.items[ramp.shape];
                            const auto& shape = brushes.shapes[ramp.shape];
                            if (!lowerPlan.items[ramp.cell] || !upperPlan->items[ramp.destination]) {
                                co_return fail(PreparationInputError::InvalidParameters);
                            }
                            upperPlan->items[ramp.cell] = ids.opening;
                            upperPlan->plateauFloor[ramp.cell] = 3;
                            if (!upperPlan->decor.empty()) upperPlan->decor[ramp.cell] = 0;
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
                // Read resolved items only on the GUI thread. The semantic graph remains
                // independent of client assets and map state.
                const uint64_t levelCells = cellCount * (1 + (cave ? 1 : 0) + std::max(levelCaps.size(), levelStoreys.empty() ? size_t(0) : size_t(1)));
                if (!reserve(levelCells * 96 + 65536)) co_return fail(PreparationInputError::MemoryLimit);
                bool rockBlocks = false;
                for (size_t i = 0; i < cellCount; ++i) if (grid->cells[i] == SurfaceKind::Rock && plan.items[i]) {
                    // The final plan already substitutes mountain below caps.
                    if (levelCaps.empty() || levelCaps.front().cells[i] != PlateauKind::Top)
                        rockBlocks |= g_item_definitions.get(plan.items[i]).hasFlag(ItemFlag::Unpassable);
                }
                auto semantic = levelPlanOf(*grid, !profile.wall.empty(), rockBlocks,
                    levelCave ? &*levelCave : nullptr, levelExits, levelShapes, levelCaps, {}, stop, levelStoreys, levelDungeons);
                if (!semantic) co_return fail(semantic.error() == ReachabilityError::Cancelled ?
                    PreparationInputError::Cancelled : PreparationInputError::InvalidParameters);
                // A hole with walkable ground below has no ramp up: only its own edge hole -> landing remains (iteration 48c).
                std::erase_if(semantic->edges, [&](const LevelEdge& edge) {
                    return std::ranges::any_of(holeLandings, [&](const LevelEdge& landing) { return edge.a == landing.a && edge.b == landing.b; });
                });
                levels = std::make_unique<PreparedTerrain::Levels>();
                levels->repair = repairReport;
                levels->passes = std::move(passes);
                levels->plan = std::move(*semantic);
                levels->blockingMask.resize(levelCells);
                if (budget.measureCandidate || budget.copyIndex) {
                    auto work = [surface = std::move(*grid), &copyAssessment, budget](std::stop_token token)
                        -> std::expected<CandidateMetrics, CandidateScoreError> {
                        if (budget.copyIndex) {
                            const auto assessed = assessCopy(surface, *budget.copyIndex, token);
                            if (!assessed) return std::unexpected(token.stop_requested() ? CandidateScoreError::Cancelled : CandidateScoreError::MeasurementFailed);
                            copyAssessment = *assessed;
                        }
                        if (!budget.measureCandidate) return CandidateMetrics{};
                        return candidateMetricsOf(surface, token);
                    };
                    std::expected<CandidateMetrics, CandidateScoreError> measured = std::unexpected(CandidateScoreError::Cancelled);
                    if (background) {
                        CoreTask<decltype(measured)> task(std::move(work));
                        while (!task.done()) {
                            if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
                            co_await std::suspend_always{};
                        }
                        measured = task.take();
                    } else measured = work(stop);
                    if (!measured) co_return fail(stop.stop_requested() ? PreparationInputError::Cancelled : PreparationInputError::InvalidParameters);
                    if (budget.measureCandidate) candidateMetrics = std::move(*measured);
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
		if (levels) {
            // Rebuild from the concrete client items after all brush/border passes.
            // Only vegetation and borders on semantically open cells are removable;
            // ground, walls, transition items and absent ground stay forbidden.
            std::vector<TerrainPreview*> previews {&*preview};
            std::vector<const GroundPlan*> plans {&plan};
            if (cave) { previews.push_back(&cave->preview); plans.push_back(&cave->plan); }
            for (auto* cap = plateau.get(); cap; cap = cap->plateau.get()) {
                previews.push_back(&cap->preview); plans.push_back(&cap->plan);
            }
            const bool rockFaceEntrances = input.config.surface.rockBlocks && input.config.surface.caveEntranceStyle == CaveEntranceStyle::RockFace &&
                !plan.entrances.empty() && std::ranges::any_of(plan.entrances, [](uint16_t id) { return id != 0; });
            if (!levels->passes.empty() || rockFaceEntrances) {
                std::vector<uint8_t> clear(cellCount);
                for (const auto& pass : levels->passes) for (const auto i : pass.cells) clear[i] = 1;
                // Rock borders on a rock-face entrance and the cell before it would block the walkable ground. A dungeon's
                // stairs down (its ground is the entrance item) are not cleared: the cell south of them is the building
                // wall, and clearing would remove it (iteration 45c).
                if (rockFaceEntrances) for (size_t i = 0; i + input.config.region.width < cellCount; ++i)
                    if (plan.entrances[i] && plan.entrances[i] != plan.items[i]) clear[i] = clear[i + input.config.region.width] = 1;
                auto cleared = DrawOperations::clearTerrainPreviewBlockers(*previews.front(), clear, stop);
                if (!cleared) co_return fail(cleared.error());
            }
            for (;;) {
                std::vector<uint8_t> repairMask(levels->blockingMask.size());
                for (size_t f = 0; f < previews.size(); ++f) for (size_t i = 0; i < cellCount; ++i) {
                    if (i % 4096 == 0 && stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);
                    const Position position(input.config.region.x + int(i % input.config.region.width),
                        input.config.region.y + int(i / input.config.region.width), levels->plan.floors[f].z);
                    const auto* tile = previews[f]->getBuffer().getTile(position);
                    levels->plan.floors[f].walkable[i] &= tile && tile->ground;
                    uint8_t blocks = tile && tile->ground && tile->ground->isBlocking() ? 2 : 0;
                    if (tile) for (const auto& item : tile->items) if (item->isBlocking() &&
                        !(!plans[f]->doors.empty() && plans[f]->doors[i] && item->isBrushDoor() && !item->isLocked())) {
                        const bool decor = !plans[f]->decor.empty() && plans[f]->decor[i] == item->getID();
                        blocks = std::max<uint8_t>(blocks, item->isBorder() || decor ? 1 : 2);
                    }
                    levels->blockingMask[f * cellCount + i] = blocks != 0;
                    repairMask[f * cellCount + i] = blocks;
                }
                auto reach = levelReachabilityOf(levels->plan, {}, levels->blockingMask, stop);
                if (!reach) co_return fail(reach.error() == ReachabilityError::Cancelled ?
                    PreparationInputError::Cancelled : PreparationInputError::AllocationFailed);
                levels->report = std::move(*reach);
                for (const auto& pass : levels->passes) for (const auto i : pass.cells)
                    if (!levels->report.walkable[i]) co_return fail(PreviewError::InvalidPlan);
                const bool complete = levels->report.allReached() && levels->report.walkableEdges == levels->report.edges;
                if (!input.config.surface.repairReach) break;
                levels->repair.unrepaired = !complete;
                if (complete || levels->repair.repairs == 64) break;
                co_await std::suspend_always{};
                auto repaired = repairLevelReach(levels->plan, repairMask, 64 - levels->repair.repairs, stop);
                if (!repaired) co_return fail(repaired.error() == ReachabilityError::Cancelled ?
                    PreparationInputError::Cancelled : PreparationInputError::AllocationFailed);
                if (!repaired->repairs) break; // stable: no legal clearance remains
                levels->repair.repairs += repaired->repairs;
                levels->repair.removedDecor += repaired->removedDecor;
                for (size_t f = 0; f < previews.size(); ++f) {
                    auto clear = std::span(repairMask).subspan(f * cellCount, cellCount);
                    for (size_t i = 0; i < cellCount; ++i)
                        clear[i] = levels->blockingMask[f * cellCount + i] && !clear[i];
                    auto cleared = DrawOperations::clearTerrainPreviewBlockers(*previews[f], clear, stop);
                    if (!cleared) co_return fail(cleared.error());
                }
                // Re-evaluate the actual payload, never accept a predicted mask.
            }
		}

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
        co_return PreparedTerrain {std::move(*snapshot), std::move(plan), std::move(*preview), memory, std::move(cave), std::move(plateau), std::move(levels), std::move(candidateMetrics), copyAssessment};
    } catch (const std::bad_alloc&) {
		co_return fail(PreparationInputError::AllocationFailed);
	} catch (const std::system_error&) {
		co_return fail(PreparationInputError::AllocationFailed);
	}
}

} // namespace

std::expected<TerrainPreparationSteps, PreparationError> prepareTerrainSteps(
	const Map& map, const GenerationConfig& config, const GroundProfile& profile,
	PreparationBudget budget, std::span<const uint8_t> mask, std::stop_token stop) {
	try {
		const auto effective = withRockPassability(config, profile);
		const auto limits = preflight(map, effective, profile, budget, mask, stop, true);
		if (!limits) return fail(limits.error());
		return runPreparation(map, effective, profile, budget, mask, stop, *limits, true);
	} catch (const std::bad_alloc&) {
		return fail(PreparationInputError::AllocationFailed);
	}
}

std::expected<PreparedTerrain, PreparationError> prepareTerrain(
	const Map& map, const GenerationConfig& config, const GroundProfile& profile,
	PreparationBudget budget, std::span<const uint8_t> mask, std::stop_token stop) {
	try {
		const auto effective = withRockPassability(config, profile);
		const auto limits = preflight(map, effective, profile, budget, mask, stop, false);
		if (!limits) return fail(limits.error());
		auto steps = runPreparation(map, effective, profile, budget, mask, stop, *limits, false);
		return steps.finish();
	} catch (const std::bad_alloc&) {
		return fail(PreparationInputError::AllocationFailed);
	}
}

} // namespace rme::procedural
