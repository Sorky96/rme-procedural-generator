// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/ground_resolver.h"

#include "app/managers/version_manager.h"
#include "brushes/ground/ground_brush.h"
#include "procedural/adapter/surface_resolver.h"
#include "item_definitions/core/item_definition_store.h"
#include <limits>
#include <new>
#include <wx/thread.h>

namespace rme::procedural {

uint64_t GroundPlan::fingerprint() const {
	// Domain + semantic result + explicit little-endian placement IDs.
	uint64_t hash = semanticFingerprint ^ 0x67726f756e645f31ULL;
	for (const auto id : items) {
		hash = (hash ^ uint8_t(id)) * 1099511628211ULL;
		hash = (hash ^ uint8_t(id >> 8)) * 1099511628211ULL;
	}
	// Appended only with vegetation, so plans without it keep their original fingerprint.
	if (!decor.empty()) {
		hash = (hash ^ 0x64) * 1099511628211ULL;
		for (const auto id : decor) {
			hash = (hash ^ uint8_t(id)) * 1099511628211ULL;
			hash = (hash ^ uint8_t(id >> 8)) * 1099511628211ULL;
		}
	}
	// Appended only with walls, so plans without them keep their fingerprint.
	if (!walls.empty()) {
		hash = (hash ^ 0x77) * 1099511628211ULL;
		for (const auto id : walls) {
			hash = (hash ^ uint8_t(id)) * 1099511628211ULL;
			hash = (hash ^ uint8_t(id >> 8)) * 1099511628211ULL;
		}
	}
	// Appended only with doors, so plans without them keep their fingerprint.
	if (!doors.empty()) {
		hash = (hash ^ 0x6f) * 1099511628211ULL;
		for (const auto flag : doors) hash = (hash ^ flag) * 1099511628211ULL;
	}
	// Appended only with camp fires, so plans without them keep their fingerprint.
	if (!fires.empty()) {
		hash = (hash ^ 0x63) * 1099511628211ULL;
		for (const auto id : fires) {
			hash = (hash ^ uint8_t(id)) * 1099511628211ULL;
			hash = (hash ^ uint8_t(id >> 8)) * 1099511628211ULL;
		}
	}
	for (const auto [tag, layer] : {std::pair{0x65, std::span(entrances)}, std::pair{0x78, std::span(exits)}}) {
		if (layer.empty()) continue;
		hash = (hash ^ tag) * 1099511628211ULL;
		for (const auto id : layer) {
			hash = (hash ^ uint8_t(id)) * 1099511628211ULL;
			hash = (hash ^ uint8_t(id >> 8)) * 1099511628211ULL;
		}
	}
	if (!caveFloor.empty()) {
		hash = (hash ^ 0x62) * 1099511628211ULL;
		for (const auto cell : caveFloor) hash = (hash ^ cell) * 1099511628211ULL;
	}
	if (config.surface.caveEntrances > 0) hash = (hash ^ uint8_t(config.surface.caveEntranceStyle)) * 1099511628211ULL;
	if (!profile.caveWall.empty()) {
		for (const unsigned char c : profile.caveWall) hash = (hash ^ c) * 1099511628211ULL;
		hash = (hash ^ 0) * 1099511628211ULL;
	}
    if (config.surface.plateauMinTop) {
        const auto add = [&](uint64_t v) {
            for (unsigned i = 0; i < 8; ++i) {
                hash = (hash ^ uint8_t(v)) * 1099511628211ULL;
                v >>= 8;
            }
        };
        add(config.surface.plateauMinTop);
        if (config.surface.plateauLevels > 1) {
            add(config.surface.plateauLevels);
        }
        if (config.surface.plateauMaxPercent) {
            add(config.surface.plateauMaxPercent);
        }
        for (const auto* name : {&profile.plateauCliff, &profile.plateauGround, &profile.plateauRamp}) {
            for (unsigned char c : *name) {
                add(c);
            }
            add(0);
        }
        for (auto c : plateauFloor) {
            add(c);
        }
        for (auto id : plateauRamps) {
            add(id);
        }
    }
    return hash;
}

bool profileNeedsEdgeClearance(const GroundProfile& profile) {
    const auto needsClearance = [](const std::string& name) {
		const auto* brush = name.empty() ? nullptr : g_brushes.getBrush(name);
		const auto* ground = brush ? brush->as<GroundBrush>() : nullptr;
        return ground && ground->hasOuterZilchBorder();
    };
    if (std::ranges::any_of(profile.active(), needsClearance)) return true;
    // Whole caps can reach the surface rock boundary. Reserve space for their
    // actual cliff and grass autoborders before generating that surface.
    return hasPlateau(profile) && (needsClearance(profile.plateauCliff) || needsClearance(profile.plateauGround));
}

std::expected<GroundPlan, ResolveError> resolveGroundPlan(
	const GenerationInput& input, const SemanticGrid& grid, const GroundProfile& profile,
	uint64_t maxPlanBytes, std::stop_token stop) {
	if (!wxIsMainThread()) return std::unexpected(ResolveError::WrongThread);
	if (stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
	if (!g_version.IsVersionLoaded() || !g_version.getLoadedVersion()) return std::unexpected(ResolveError::NoClient);
	if (input.assetRevision != g_version.GetAssetRevision()) return std::unexpected(ResolveError::StaleAssets);
	if (grid.region != input.config.region || !grid.valid()) return std::unexpected(ResolveError::InvalidInput);
	const auto layout = validateGeneration(input.config, input.extent,
		{grid.cells.size(), UINT64_MAX}, input.mask, input.protection, stop);
	if (!layout) return std::unexpected(layout.error() == GenerationError::Cancelled ?
		ResolveError::Cancelled : ResolveError::InvalidInput);
	// Captured inputs always own both full masks. Reject malformed caller data.
	if (input.mask.size() != grid.cells.size() || input.protection.size() != grid.cells.size()) {
		return std::unexpected(ResolveError::InvalidInput);
	}
	if (grid.cells.size() > (std::numeric_limits<size_t>::max() - sizeof(GroundPlan)) / sizeof(uint16_t) ||
		grid.cells.size() > std::vector<uint16_t>().max_size()) return std::unexpected(ResolveError::PlanLimit);
	uint64_t payloadBytes = sizeof(GroundPlan) + uint64_t(grid.cells.size()) * sizeof(uint16_t);
	if (payloadBytes > maxPlanBytes) return std::unexpected(ResolveError::PlanLimit);
	if (hasCave(profile)) return std::unexpected(ResolveError::InvalidProfile);
	if (profile.count != GroundProfile::groundRollCount) return std::unexpected(ResolveError::InvalidProfile);
	for (const auto& name : profile.active()) {
		if (name.empty() || name.size() > 256) return std::unexpected(ResolveError::InvalidProfile);
		if (name.size() + 1 > maxPlanBytes - payloadBytes) return std::unexpected(ResolveError::PlanLimit);
		payloadBytes += name.size() + 1;
	}
	try {
		const auto client = g_version.GetCurrentVersionID();
		if (client.size() > maxPlanBytes - payloadBytes || maxPlanBytes - payloadBytes - client.size() < 1) {
			return std::unexpected(ResolveError::PlanLimit);
		}
		std::array<const GroundBrush*, 3> brushes {};
		std::array<uint64_t, 3> totals {};
		for (size_t index = 0; index < brushes.size(); ++index) {
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
		GroundPlan result {input.config, profile, client, input.mapGeneration, input.mapRevision, input.assetRevision,
			grid.fingerprint(), std::vector<uint16_t>(grid.cells.size(), 0)};
		// Advance all terrain-specific streams at every position, even exclusions.
		// Changing only a mask/protection therefore cannot shift another variant.
		std::array<StableRandom, 3> streams {
			StableRandom(input.config.seed, 0x67726173735f7631ULL),
			StableRandom(input.config.seed, 0x646972745f7631ULL),
			StableRandom(input.config.seed, 0x77617465725f7631ULL)
		};
		for (size_t i = 0; i < grid.cells.size(); ++i) {
			if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
			std::array<int, 3> rolls {};
			for (size_t terrain = 0; terrain < rolls.size(); ++terrain) rolls[terrain] = int(streams[terrain].bounded(totals[terrain])) + 1;
			const bool writable = input.mask[i] && !input.protection[i];
			if ((grid.cells[i] != Terrain::Preserve) != writable) return std::unexpected(ResolveError::InvalidInput);
			if (!writable) continue;
			const auto terrain = size_t(grid.cells[i]) - 1;
			const auto id = brushes[terrain]->getPlacementItemIdForRoll(rolls[terrain]);
			if (!id || !*id) return std::unexpected(ResolveError::InvalidItem);
			result.items[i] = *id;
		}
		if (stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
		return result;
	} catch (const std::bad_alloc&) {
		return std::unexpected(ResolveError::AllocationFailed);
	}
}

} // namespace rme::procedural
