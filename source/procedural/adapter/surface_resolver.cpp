// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/surface_resolver.h"

#include "app/managers/version_manager.h"
#include "brushes/doodad/doodad_brush.h"
#include "game/item.h"
#include "brushes/ground/ground_brush.h"
#include "item_definitions/core/item_definition_store.h"
#include <array>
#include <limits>
#include <new>
#include <ranges>
#include <wx/thread.h>

namespace rme::procedural {

namespace {
constexpr size_t kindCount = 6;
// One independent stream per kind, advanced at every cell, so changing a mask,
// protection or neighbouring kind never shifts another cell's variant.
constexpr std::array<uint64_t, kindCount> variantStreams {
	0x73325f6772617373ULL, 0x73325f6469727420ULL, 0x73325f726f636b20ULL,
	0x73325f73686f7265ULL, 0x73325f6c616b6520ULL, 0x73325f7269766572ULL
};
constexpr std::array<uint64_t, decorKindCount> decorStreams {
	0x73325f7472656573ULL, 0x73325f6275736865ULL, 0x73325f73746f6e65ULL
};
constexpr uint64_t maxDecorChance = 100000;
}

std::expected<GroundPlan, ResolveError> resolveSurfacePlan(
	const GenerationInput& input, const SurfaceGrid& grid, const GroundProfile& profile,
	uint64_t maxPlanBytes, std::stop_token stop, const DecorGrid* decor) {
	if (!wxIsMainThread()) return std::unexpected(ResolveError::WrongThread);
	if (stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
	if (!g_version.IsVersionLoaded() || !g_version.getLoadedVersion()) return std::unexpected(ResolveError::NoClient);
	if (input.assetRevision != g_version.GetAssetRevision()) return std::unexpected(ResolveError::StaleAssets);
	if (grid.region != input.config.region || !grid.valid()) return std::unexpected(ResolveError::InvalidInput);
	const auto layout = validateGeneration(input.config, input.extent,
		{grid.cells.size(), UINT64_MAX}, input.mask, input.protection, stop);
	if (!layout) return std::unexpected(layout.error() == GenerationError::Cancelled ?
		ResolveError::Cancelled : ResolveError::InvalidInput);
	if (input.mask.size() != grid.cells.size() || input.protection.size() != grid.cells.size()) {
		return std::unexpected(ResolveError::InvalidInput);
	}
	if (profile.count != GroundProfile::surfaceCount) return std::unexpected(ResolveError::InvalidProfile);
	const bool vegetation = input.config.vegetation;
	if (vegetation != (decor != nullptr) ||
		profile.decorCount != (vegetation ? GroundProfile::decorationCount : 0)) {
		return std::unexpected(ResolveError::InvalidProfile);
	}
	if (vegetation && (decor->region != grid.region || !decor->valid())) return std::unexpected(ResolveError::InvalidInput);
	if (grid.cells.size() > (std::numeric_limits<size_t>::max() - sizeof(GroundPlan)) / sizeof(uint16_t) ||
		grid.cells.size() > std::vector<uint16_t>().max_size()) return std::unexpected(ResolveError::PlanLimit);
	uint64_t payloadBytes = sizeof(GroundPlan) + uint64_t(grid.cells.size()) * sizeof(uint16_t) * (vegetation ? 2 : 1);
	if (payloadBytes > maxPlanBytes) return std::unexpected(ResolveError::PlanLimit);
	for (const auto& name : std::views::join(std::array<std::span<const std::string>, 2> {profile.active(), profile.activeDecor()})) {
		if (name.empty() || name.size() > 256) return std::unexpected(ResolveError::InvalidProfile);
		if (name.size() + 1 > maxPlanBytes - payloadBytes) return std::unexpected(ResolveError::PlanLimit);
		payloadBytes += name.size() + 1;
	}
	try {
		const auto client = g_version.GetCurrentVersionID();
		if (client.size() > maxPlanBytes - payloadBytes || maxPlanBytes - payloadBytes - client.size() < 1) {
			return std::unexpected(ResolveError::PlanLimit);
		}
		std::array<const GroundBrush*, kindCount> brushes {};
		std::array<uint64_t, kindCount> totals {};
		for (size_t index = 0; index < kindCount; ++index) {
			const auto* brush = g_brushes.getBrush(profile.brushes[index]);
			brushes[index] = brush ? brush->as<GroundBrush>() : nullptr;
			if (!brushes[index] || brushes[index]->getTotalChance() <= 0 || brushes[index]->getBorderItems().empty()) {
				return std::unexpected(ResolveError::InvalidProfile);
			}
			int previous = 0;
			for (const auto& variant : brushes[index]->getBorderItems()) {
				if (stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
				if (variant.chance < previous || variant.chance > brushes[index]->getTotalChance()) {
					return std::unexpected(ResolveError::InvalidProfile);
				}
				previous = variant.chance;
				const auto definition = g_item_definitions.get(variant.id);
				if (!variant.id || !definition || !definition.clientId() || definition.isMetaItem()) {
					return std::unexpected(ResolveError::InvalidItem);
				}
				const auto baseId = definition.attribute(ItemAttributeKey::BorderBaseGroundId);
				const auto ground = definition.isGroundTile() ? definition :
					(baseId > 0 && baseId <= UINT16_MAX ? g_item_definitions.get(uint16_t(baseId)) : ItemDefinitionView{});
				if (!ground || !ground.isGroundTile() || !ground.clientId() || ground.isMetaItem() ||
					ground.editorData().brush != brushes[index]) return std::unexpected(ResolveError::InvalidItem);
			}
			if (previous != brushes[index]->getTotalChance()) return std::unexpected(ResolveError::InvalidProfile);
			totals[index] = uint64_t(previous);
		}
		std::array<const DoodadBrush*, decorKindCount> doodads {};
		std::array<uint64_t, decorKindCount> decorTotals {};
		for (size_t index = 0; vegetation && index < decorKindCount; ++index) {
			const auto* brush = g_brushes.getBrush(profile.decor[index]);
			doodads[index] = brush ? brush->as<DoodadBrush>() : nullptr;
			if (!doodads[index] || !doodads[index]->hasSingleObjects(0) || doodads[index]->getSingleChance(0) <= 0 ||
				uint64_t(doodads[index]->getSingleChance(0)) > maxDecorChance) return std::unexpected(ResolveError::InvalidProfile);
			decorTotals[index] = uint64_t(doodads[index]->getSingleChance(0));
			// Every reachable single item must be a real, plain, non-ground, non-border item.
			for (uint64_t roll = 1; roll <= decorTotals[index]; ++roll) {
				if (roll % 1024 == 0 && stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
				const auto* item = doodads[index]->getRandomSingleItem(0, int(roll));
				const auto definition = item && item->getID() ? g_item_definitions.get(item->getID()) : ItemDefinitionView{};
				if (!definition || !definition.clientId() || definition.isMetaItem() || definition.isGroundTile() ||
					definition.attribute(ItemAttributeKey::BorderBaseGroundId) > 0) return std::unexpected(ResolveError::InvalidItem);
			}
		}
		GroundPlan result {input.config, profile, client, input.mapGeneration, input.mapRevision,
			input.assetRevision, grid.fingerprint(), std::vector<uint16_t>(grid.cells.size(), 0)};
		if (vegetation) result.decor.assign(grid.cells.size(), 0);
		std::vector<StableRandom> streams;
		streams.reserve(kindCount);
		for (const auto stream : variantStreams) streams.emplace_back(input.config.seed, stream);
		std::vector<StableRandom> decorRng;
		decorRng.reserve(decorKindCount);
		if (vegetation) for (const auto stream : decorStreams) decorRng.emplace_back(input.config.seed, stream);
		for (size_t i = 0; i < grid.cells.size(); ++i) {
			if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
			std::array<int, kindCount> rolls {};
			for (size_t kind = 0; kind < kindCount; ++kind) rolls[kind] = int(streams[kind].bounded(totals[kind])) + 1;
			std::array<int, decorKindCount> decorRolls {};
			if (vegetation) for (size_t kind = 0; kind < decorKindCount; ++kind) {
				decorRolls[kind] = int(decorRng[kind].bounded(decorTotals[kind])) + 1;
			}
			const bool writable = input.mask[i] && !input.protection[i];
			if ((grid.cells[i] != SurfaceKind::Preserve) != writable) return std::unexpected(ResolveError::InvalidInput);
			if (!writable) continue;
			const auto kind = size_t(grid.cells[i]) - 1;
			const auto id = brushes[kind]->getPlacementItemIdForRoll(rolls[kind]);
			if (!id || !*id) return std::unexpected(ResolveError::InvalidItem);
			result.items[i] = *id;
			if (vegetation) {
				const auto placed = decor->cells[i];
				if (placed != DecorKind::None) {
					const size_t slot = size_t(placed) - 1;
					const auto* item = doodads[slot]->getRandomSingleItem(0, decorRolls[slot]);
					if (!item || !item->getID()) return std::unexpected(ResolveError::InvalidItem);
					result.decor[i] = item->getID();
				}
			}
		}
		if (stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
		return result;
	} catch (const std::bad_alloc&) {
		return std::unexpected(ResolveError::AllocationFailed);
	}
}

} // namespace rme::procedural
