// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/terrain_preparation.h"

#include "app/managers/version_manager.h"
#include "editor/operations/draw_operations.h"
#include "map/map.h"
#include "procedural/adapter/snapshot_memory.h"

#include <algorithm>
#include <limits>
#include <new>
#include <wx/thread.h>

namespace rme::procedural {
namespace {

constexpr uint64_t maxNameBytes = 256;

std::unexpected<PreparationError> fail(PreparationError error) {
	return std::unexpected<PreparationError>(std::move(error));
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

	uint64_t profileBytes = 0;
	for (const auto& name : profile.brushes) {
		if (!validName(name.size())) return fail(PreparationInputError::InvalidProfile);
		profileBytes += stringCopyBytes(name.size());
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
	const uint64_t planBytes = fixedPlanBytes + cellCount * sizeof(uint16_t);

	const uint64_t outputBytes = layout->outputBytes;
	if (outputBytes > std::numeric_limits<uint64_t>::max() - kSnapshotAllocationOverhead) {
		return fail(PreparationInputError::MemoryLimit);
	}
	const uint64_t semanticBytes = outputBytes + kSnapshotAllocationOverhead;

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

		// The semantic grid lives only in this scope, so it is released before the sandbox exists.
		GroundPlan plan;
		{
			auto grid = generateTerrain(input.config, input.extent, { budget.maxCells, semanticBytes },
				input.mask, input.protection, stop);
			if (!grid) co_return fail(grid.error());
			auto resolved = resolveGroundPlan(input, *grid, profile, planBytes, stop);
			if (!resolved) co_return fail(resolved.error());
			plan = std::move(*resolved);
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

		// Check the live source again before returning a result to either caller.
		if (stop.stop_requested()) co_return fail(PreparationInputError::Cancelled);

		if (!snapshotMatchesMap(map, *snapshot)) co_return fail(PreviewError::StaleSnapshot);

		const PreparationMemory memory {
			infrastructureBytes, snapshotBytes, semanticBytes, planBytes, previewBytes, budget.maxBytes - remaining
		};
		co_return PreparedTerrain { std::move(*snapshot), std::move(plan), std::move(*preview), memory };
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
