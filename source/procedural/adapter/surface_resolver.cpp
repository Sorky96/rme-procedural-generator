// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/surface_resolver.h"

#include "app/managers/version_manager.h"
#include "brushes/doodad/doodad_brush.h"
#include "game/item.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "procedural/adapter/door_types.h"
#include "item_definitions/core/item_definition_store.h"
#include <array>
#include <limits>
#include <new>
#include <ranges>
#include <typeinfo>
#include <wx/thread.h>

namespace rme::procedural {

namespace {
constexpr size_t kindCount = 8; // Grass, Dirt, Rock, Shore, Lake, River, Path, Floor; a profile uses the first `count` of them
// One independent stream per kind, advanced at every cell, so changing a mask,
// protection or neighbouring kind never shifts another cell's variant.
constexpr std::array<uint64_t, kindCount> variantStreams {
	0x73325f6772617373ULL, 0x73325f6469727420ULL, 0x73325f726f636b20ULL,
	0x73325f73686f7265ULL, 0x73325f6c616b6520ULL, 0x73325f7269766572ULL, 0x73325f7061746820ULL, 0x73325f666c6f6f72ULL
};
constexpr std::array<uint64_t, decorKindCount> decorStreams {
	0x73325f7472656573ULL, 0x73325f6275736865ULL, 0x73325f73746f6e65ULL
};
constexpr uint64_t maxDecorChance = 100000;

bool plainCaveItem(const Item* item, bool allowGround = false) {
	const auto def = item ? g_item_definitions.get(item->getID()) : ItemDefinitionView{};
	return item && typeid(*item) == typeid(Item) && !item->getInvalidOTBMData() &&
		(!item->getAttributesView() || item->getAttributesView()->empty()) && def && def.clientId() &&
		!def.isMetaItem() && (!def.isGroundTile() || (allowGround && def.hasFlag(ItemFlag::FloorChangeDown))) && !def.attribute(ItemAttributeKey::BorderBaseGroundId);
}
}

bool hasCave(const GroundProfile& profile) noexcept {
	return !profile.caveEntrance.empty() || !profile.caveExit.empty() || !profile.caveGround.empty() || !profile.caveWall.empty();
}

bool validCaveProfile(const GroundProfile& profile, const GenerationConfig& config) noexcept {
	return !hasCave(profile) || (config.algorithm == GenerationAlgorithm::Surface && config.surface.caveEntrances > 0 &&
		profile.count == GroundProfile::surfaceWithFloorCount && !profile.caveEntrance.empty() && profile.caveEntrance.size() <= 256 &&
		!profile.caveExit.empty() && profile.caveExit.size() <= 256 && !profile.caveGround.empty() && profile.caveGround.size() <= 256 && !profile.caveWall.empty() && profile.caveWall.size() <= 256);
}

std::expected<CaveBrushes, ResolveError> resolveCaveBrushes(const GroundProfile& profile, const GenerationConfig& config) try {
	if (!wxIsMainThread()) return std::unexpected(ResolveError::WrongThread);
	if (!g_version.IsVersionLoaded()) return std::unexpected(ResolveError::NoClient);
	if (!hasCave(profile) || !validCaveProfile(profile, config)) return std::unexpected(ResolveError::InvalidProfile);
	const auto doodad = [](const std::string& name) {
		const auto* brush = g_brushes.getBrush(name);
		return brush ? brush->as<DoodadBrush>() : nullptr;
	};
	const auto* entrance = doodad(profile.caveEntrance);
	const auto* exit = doodad(profile.caveExit);
	const auto* brush = g_brushes.getBrush(profile.caveGround);
	const auto* ground = brush ? brush->as<GroundBrush>() : nullptr;
	const auto* wallBrush = g_brushes.getBrush(profile.caveWall);
	const auto* wall = wallBrush ? wallBrush->as<GroundBrush>() : nullptr;
	const auto* entranceBrush = g_brushes.getBrush(profile.caveEntrance);
	const auto* hole = entranceBrush ? entranceBrush->as<GroundBrush>() : nullptr;
	const bool floorHole = config.surface.caveEntranceStyle == CaveEntranceStyle::FloorHole;
	if (!exit || !ground || !wall || (floorHole ? !hole : !entrance)) return std::unexpected(ResolveError::InvalidProfile);
	CaveBrushes result;
	const auto validGround = [](const GroundBrush* ground) {
		if (!ground || ground->getTotalChance() <= 0 || ground->getBorderItems().empty()) return false;
		int previous = 0;
		for (const auto& variant : ground->getBorderItems()) {
			const auto def = g_item_definitions.get(variant.id);
			const auto base = def.attribute(ItemAttributeKey::BorderBaseGroundId);
			const auto actual = def.isGroundTile() ? def : (base > 0 && base <= UINT16_MAX ? g_item_definitions.get(uint16_t(base)) : ItemDefinitionView{});
			if (variant.chance < previous || variant.chance > ground->getTotalChance() || !actual || !actual.isGroundTile() ||
				!def.clientId() || def.isMetaItem() || !actual.clientId() || actual.isMetaItem() || actual.editorData().brush != ground) return false;
			previous = variant.chance;
		}
		if (previous != ground->getTotalChance()) return false;
		return true;
	};
	if (!validGround(ground) || !validGround(wall) || (floorHole && !validGround(hole))) return std::unexpected(ResolveError::InvalidProfile);
	result.ground = ground->getPlacementItemId();
	result.wall = wall->getPlacementItemId();
	if (!floorHole && entrance->getAlternativeCount() > 256) return std::unexpected(ResolveError::InvalidProfile);
	auto exitBrush = resolveCaveExitBrush(*exit);
	if (!exitBrush) return std::unexpected(exitBrush.error());
	static_cast<CaveExitBrush&>(result) = std::move(*exitBrush);
	if (floorHole) {
		const auto id = hole->getPlacementItemId();
		const auto def = g_item_definitions.get(id);
		if (!def || !def.isGroundTile() || !def.hasFlag(ItemFlag::FloorChangeDown)) return std::unexpected(ResolveError::InvalidProfile);
		result.entrances.push_back(id);
	} else {
		int priorAlternative = -1;
		for (int i = 0; i < entrance->getTotalVariants(); ++i) {
			const auto variant = entrance->getVariant(i);
			if (variant.composite || !plainCaveItem(variant.single_item)) return std::unexpected(ResolveError::InvalidProfile);
			if (variant.alternative_index != priorAlternative) {
				const auto* item = entrance->getRandomSingleItem(variant.alternative_index, 1);
				if (!plainCaveItem(item)) return std::unexpected(ResolveError::InvalidProfile);
				result.entrances.push_back(item->getID());
				priorAlternative = variant.alternative_index;
			}
		}
		if (result.entrances.size() != entrance->getAlternativeCount()) return std::unexpected(ResolveError::InvalidProfile);
	}
	if (result.entrances.empty() || !result.ground || !result.wall) return std::unexpected(ResolveError::InvalidProfile);
	return result;
} catch (const std::bad_alloc&) {
	return std::unexpected(ResolveError::AllocationFailed);
}

std::expected<CaveExitBrush, ResolveError> resolveCaveExitBrush(const DoodadBrush& exit) try {
	if (!wxIsMainThread()) return std::unexpected(ResolveError::WrongThread);
	if (!g_version.IsVersionLoaded()) return std::unexpected(ResolveError::NoClient);
	if (exit.getAlternativeCount() > 256 || size_t(exit.getTotalVariants()) != exit.getAlternativeCount()) return std::unexpected(ResolveError::InvalidProfile);
	CaveExitBrush result;
	for (int i = 0; i < exit.getTotalVariants(); ++i) {
		const auto variant = exit.getVariant(i);
		if (!variant.composite || variant.single_item || variant.alternative_index != i || result.shapes.size() >= 256) return std::unexpected(ResolveError::InvalidProfile);
		ExitShape shape;
		CaveExitBrush::Items ids;
		bool hasUpperPart = false;
		for (const auto& [pos, items] : *variant.composite) {
			if (items.size() != 1 || !plainCaveItem(items.front().get(), pos.x == 0 && pos.y == 0 && pos.z == -1)) return std::unexpected(ResolveError::InvalidProfile);
			const auto id = items.front()->getID();
			if (pos.x == 0 && pos.y == 0 && pos.z == -1 && !hasUpperPart) hasUpperPart = true;
			else if (pos.x == 0 && pos.y == 0 && pos.z == 0 && !ids.ramp) {
				ids.ramp = id;
				const auto def = g_item_definitions.get(id);
				unsigned directions = 0;
				for (const auto [flag, direction] : {std::pair{ItemFlag::FloorChangeNorth, ExitDirection::North},
					std::pair{ItemFlag::FloorChangeSouth, ExitDirection::South}, std::pair{ItemFlag::FloorChangeEast, ExitDirection::East},
					std::pair{ItemFlag::FloorChangeWest, ExitDirection::West}}) {
					if (def.hasFlag(flag)) { shape.direction = direction; ++directions; }
				}
				if (directions != 1) return std::unexpected(ResolveError::InvalidProfile);
			} else if (pos.z == 0 && std::abs(pos.x) + std::abs(pos.y) == 1 && !ids.tail) {
				ids.tail = id; shape.hasTail = true; shape.tailDx = int8_t(pos.x); shape.tailDy = int8_t(pos.y);
			} else return std::unexpected(ResolveError::InvalidProfile);
		}
		if (!hasUpperPart || !ids.ramp) return std::unexpected(ResolveError::InvalidProfile);
		result.shapes.push_back(shape);
		result.items.push_back(ids);
	}
	if (result.shapes.empty()) return std::unexpected(ResolveError::InvalidProfile);
	return result;
} catch (const std::bad_alloc&) {
	return std::unexpected(ResolveError::AllocationFailed);
}

std::expected<GroundPlan, ResolveError> resolveSurfacePlan(
	const GenerationInput& input, const SurfaceGrid& grid, const GroundProfile& profile,
	uint64_t maxPlanBytes, std::stop_token stop, const DecorGrid* decor, std::span<const uint8_t> keepClear) {
	if (!wxIsMainThread()) return std::unexpected(ResolveError::WrongThread);
	if (stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
	if (!g_version.IsVersionLoaded() || !g_version.getLoadedVersion()) return std::unexpected(ResolveError::NoClient);
	if (input.assetRevision != g_version.GetAssetRevision()) return std::unexpected(ResolveError::StaleAssets);
	if (grid.region != input.config.region || !grid.valid()) return std::unexpected(ResolveError::InvalidInput);
	if (!validCaveProfile(profile, input.config)) return std::unexpected(ResolveError::InvalidProfile);
	if (hasCave(profile)) {
		const auto cave = resolveCaveBrushes(profile, input.config);
		if (!cave) return std::unexpected(cave.error());
	}
	const auto layout = validateGeneration(input.config, input.extent,
		{grid.cells.size(), UINT64_MAX}, input.mask, input.protection, stop);
	if (!layout) return std::unexpected(layout.error() == GenerationError::Cancelled ?
		ResolveError::Cancelled : ResolveError::InvalidInput);
	if (input.mask.size() != grid.cells.size() || input.protection.size() != grid.cells.size() ||
		(!keepClear.empty() && keepClear.size() != grid.cells.size())) {
		return std::unexpected(ResolveError::InvalidInput);
	}
	if (profile.count != GroundProfile::surfaceCount && profile.count != GroundProfile::surfaceWithPathCount &&
		profile.count != GroundProfile::surfaceWithFloorCount) return std::unexpected(ResolveError::InvalidProfile);
	const size_t kinds = profile.count;
	const bool walled = !profile.wall.empty();
	if (walled && (profile.count != GroundProfile::surfaceWithFloorCount || profile.wall.size() > 256)) return std::unexpected(ResolveError::InvalidProfile);
	const bool fired = !profile.campfire.empty();
	if (fired && (profile.count != GroundProfile::surfaceWithFloorCount || profile.campfire.size() > 256)) return std::unexpected(ResolveError::InvalidProfile);
	const bool doored = !profile.door.empty();
	if (doored && (!walled || !doorTypeOf(profile.door))) return std::unexpected(ResolveError::InvalidProfile);
	const bool vegetation = input.config.vegetation;
	if (vegetation != (decor != nullptr) ||
		profile.decorCount != (vegetation ? GroundProfile::decorationCount : 0)) {
		return std::unexpected(ResolveError::InvalidProfile);
	}
	if (vegetation && (decor->region != grid.region || !decor->valid())) return std::unexpected(ResolveError::InvalidInput);
	if (grid.cells.size() > (std::numeric_limits<size_t>::max() - sizeof(GroundPlan)) / sizeof(uint16_t) ||
		grid.cells.size() > std::vector<uint16_t>().max_size()) return std::unexpected(ResolveError::PlanLimit);
	uint64_t payloadBytes = sizeof(GroundPlan) + uint64_t(grid.cells.size()) * (sizeof(uint16_t) * ((vegetation ? 2 : 1) + (fired ? 1 : 0)) + (doored ? 1 : 0));
	if (payloadBytes > maxPlanBytes) return std::unexpected(ResolveError::PlanLimit);
	for (const auto& name : std::views::join(std::array<std::span<const std::string>, 2> {profile.active(), profile.activeDecor()})) {
		if (name.empty() || name.size() > 256) return std::unexpected(ResolveError::InvalidProfile);
		if (name.size() + 1 > maxPlanBytes - payloadBytes) return std::unexpected(ResolveError::PlanLimit);
		payloadBytes += name.size() + 1;
	}
	for (const auto* name : {&profile.caveEntrance, &profile.caveExit, &profile.caveGround, &profile.caveWall}) {
		if (name->empty()) continue;
		if (name->size() + 1 > maxPlanBytes - payloadBytes) return std::unexpected(ResolveError::PlanLimit);
		payloadBytes += name->size() + 1;
	}
	try {
		const auto client = g_version.GetCurrentVersionID();
		if (client.size() > maxPlanBytes - payloadBytes || maxPlanBytes - payloadBytes - client.size() < 1) {
			return std::unexpected(ResolveError::PlanLimit);
		}
		std::array<const GroundBrush*, kindCount> brushes {};
		std::array<uint64_t, kindCount> totals {};
		for (size_t index = 0; index < kinds; ++index) {
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
		uint16_t wallItem = 0;
		if (walled) {
			const auto* brush = g_brushes.getBrush(profile.wall);
			const auto* wall = brush ? brush->as<WallBrush>() : nullptr;
			if (!wall || wall->is<WallDecorationBrush>()) return std::unexpected(ResolveError::InvalidProfile);
			wallItem = wall->getPlacementItemId();
			const auto definition = wallItem ? g_item_definitions.get(wallItem) : ItemDefinitionView{};
			if (!definition || !definition.clientId() || definition.isMetaItem() || definition.isGroundTile()) return std::unexpected(ResolveError::InvalidItem);
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
		if (walled) result.walls.assign(grid.cells.size(), 0);
		// Camp fires (iteration 26): one item of the campfire doodad brush per fire cell; the variation is picked from the cell so the fires differ.
		if (fired) {
			const auto* brush = g_brushes.getBrush(profile.campfire);
			const auto* fireBrush = brush ? brush->as<DoodadBrush>() : nullptr;
			if (!fireBrush) return std::unexpected(ResolveError::InvalidProfile);
			std::vector<int> variations;
			for (int variation = 0; variation <= fireBrush->getMaxVariation(); ++variation) if (fireBrush->hasSingleObjects(variation) && fireBrush->getSingleChance(variation) > 0) variations.push_back(variation);
			if (variations.empty()) return std::unexpected(ResolveError::InvalidProfile);
			result.fires.assign(grid.cells.size(), 0);
			for (const uint32_t fire : grid.fires) {
				const int variation = variations[(uint64_t(fire) + input.config.seed) % variations.size()];
				const auto* item = fireBrush->getRandomSingleItem(variation, 1);
				const auto definition = item && item->getID() ? g_item_definitions.get(item->getID()) : ItemDefinitionView{};
				if (!definition || !definition.clientId() || definition.isMetaItem() || definition.isGroundTile() ||
					definition.attribute(ItemAttributeKey::BorderBaseGroundId) > 0) return std::unexpected(ResolveError::InvalidItem);
				result.fires[fire] = item->getID();
			}
		}
		std::vector<uint8_t> gapCell;  // outline cells of ruins and camps that get no wall piece (G5 iteration 25)
		if (walled && !grid.gaps.empty()) {
			gapCell.assign(grid.cells.size(), 0);
			for (const uint32_t gap : grid.gaps) gapCell[gap] = 1;  // valid() already checked the range
		}
		std::vector<uint8_t> doorCell;
		if (doored) {
			result.doors.assign(grid.cells.size(), 0);
			doorCell.assign(grid.cells.size(), 0);
			for (const uint32_t door : grid.doors) doorCell[door] = 1;  // valid() already checked the range
		}
		std::vector<StableRandom> streams;
		streams.reserve(kinds);
		for (size_t index = 0; index < kinds; ++index) streams.emplace_back(input.config.seed, variantStreams[index]);
		std::vector<StableRandom> decorRng;
		decorRng.reserve(decorKindCount);
		if (vegetation) for (const auto stream : decorStreams) decorRng.emplace_back(input.config.seed, stream);
		for (size_t i = 0; i < grid.cells.size(); ++i) {
			if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
			std::array<int, kindCount> rolls {};
			for (size_t kind = 0; kind < kinds; ++kind) rolls[kind] = int(streams[kind].bounded(totals[kind])) + 1;
			std::array<int, decorKindCount> decorRolls {};
			if (vegetation) for (size_t kind = 0; kind < decorKindCount; ++kind) {
				decorRolls[kind] = int(decorRng[kind].bounded(decorTotals[kind])) + 1;
			}
			const bool writable = input.mask[i] && !input.protection[i] && (keepClear.empty() || !keepClear[i]);
			if ((grid.cells[i] != SurfaceKind::Preserve) != writable) return std::unexpected(ResolveError::InvalidInput);
			if (!writable) continue;
			const auto kind = size_t(grid.cells[i]) - 1;
			if (kind >= kinds) return std::unexpected(ResolveError::InvalidInput); // a path or floor cell needs its brush
			const auto id = brushes[kind]->getPlacementItemIdForRoll(rolls[kind]);
			if (!id || !*id) return std::unexpected(ResolveError::InvalidItem);
			result.items[i] = *id;
			if (walled && grid.cells[i] == SurfaceKind::Floor) {
				// Outline of a foundation: a Floor cell with a 4-neighbour of another kind (or outside the area).
				const size_t width = grid.region.width, height = grid.region.height, x = i % width, y = i / width;
				const bool outline = x == 0 || y == 0 || x + 1 == width || y + 1 == height || grid.cells[i - 1] != SurfaceKind::Floor ||
					grid.cells[i + 1] != SurfaceKind::Floor || grid.cells[i - width] != SurfaceKind::Floor || grid.cells[i + width] != SurfaceKind::Floor;
				if (outline && (gapCell.empty() || !gapCell[i])) result.walls[i] = wallItem;
				if (outline && doored && doorCell[i]) result.doors[i] = 1;
			}
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
