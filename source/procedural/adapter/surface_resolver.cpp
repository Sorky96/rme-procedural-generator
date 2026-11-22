// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/surface_resolver.h"

#include "app/managers/version_manager.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "game/item.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "procedural/adapter/door_types.h"
#include "procedural/adapter/decor_source.h"
#include "procedural/core/grass_patches.h"
#include "item_definitions/core/item_definition_store.h"
#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <ranges>
#include <string_view>
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
constexpr uint64_t treeMixStream = 0x73325f6d69787472ULL, bushMixStream = 0x73325f6d69786275ULL, mixDecorStream = 0x73325f6d69786465ULL;
constexpr uint64_t overlayStream = 0x73325f6f7665726cULL; // "s2_overl"
constexpr uint64_t wallStackStream = 0x73325f77616c6c73ULL; // "s2_walls"
constexpr uint64_t structureStream = 0x73325f7772656b73ULL; // "s2_wreks"
constexpr uint64_t carpetStream = 0x73325f6361727065ULL; // "s2_carpe"
constexpr uint64_t variantStream = 0x73325f7661726e74ULL; // "s2_varnt"
constexpr uint32_t overlayPatchCut = 39000; // grassPatchValue at or above this: 32% of the cells (measured on 1000 x 1000)

uint64_t mixHash(uint64_t value) {
	value += 0x9e3779b97f4a7c15ULL;
	value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
	value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
	return value ^ (value >> 31);
}

constexpr std::string_view fallbackHoleBrush = "cave hole";
constexpr std::string_view holeLandingBrush = "cave ropespot";

bool plainCaveItem(const Item* item, bool allowGround = false) {
	const auto def = item ? g_item_definitions.get(item->getID()) : ItemDefinitionView{};
	return item && typeid(*item) == typeid(Item) && !item->getInvalidOTBMData() &&
		(!item->getAttributesView() || item->getAttributesView()->empty()) && def && def.clientId() &&
		!def.isMetaItem() && (!def.isGroundTile() || (allowGround && def.hasFlag(ItemFlag::FloorChangeDown))) && !def.attribute(ItemAttributeKey::BorderBaseGroundId);
}
}

bool hasPlateau(const GroundProfile& p) noexcept
{
    return !p.plateauGround.empty() || !p.plateauRamp.empty();
}
bool validPlateauProfile(const GroundProfile& p, const GenerationConfig& c) noexcept
{
    return !hasPlateau(p)
        || (c.algorithm == GenerationAlgorithm::Surface && c.surface.plateauMinTop >= 16 && c.surface.plateauMinTop <= 4096
            && c.surface.plateauLevels >= 1 && c.surface.plateauLevels <= 3 && c.surface.plateauMaxPercent <= 100
            && !p.plateauGround.empty() && p.plateauGround.size() <= 256 && !p.plateauRamp.empty() && p.plateauRamp.size() <= 256
            && !p.plateauCliff.empty() && p.plateauCliff.size() <= 256);
}
std::expected<CaveExitBrush, ResolveError> resolvePlateauBrushes(const GroundProfile& p, const GenerationConfig& c)
{
    if (!wxIsMainThread()) {
        return std::unexpected(ResolveError::WrongThread);
    }
    if (!g_version.IsVersionLoaded()) {
        return std::unexpected(ResolveError::NoClient);
    }
    if (!hasPlateau(p) || !validPlateauProfile(p, c)) {
        return std::unexpected(ResolveError::InvalidProfile);
    }
    for (const auto* name : {&p.plateauGround, &p.plateauCliff}) {
        const auto* b = g_brushes.getBrush(*name);
        const auto* g = b ? b->as<GroundBrush>() : nullptr;
        if (!g || g->getTotalChance() <= 0 || !g->getPlacementItemId() || (name == &p.plateauCliff && g->getBorders().empty())) {
            return std::unexpected(ResolveError::InvalidProfile);
        }
    }
    const auto* b = g_brushes.getBrush(p.plateauRamp);
    const auto* d = b ? b->as<DoodadBrush>() : nullptr;
    if (!d) {
        return std::unexpected(ResolveError::InvalidProfile);
    }
    return resolveCaveExitBrush(*d);
}

bool validStoreyProfile(const GroundProfile& p, const GenerationConfig& c) noexcept {
    if (!c.surface.buildingStoreys) return p.buildingStairs.empty();
    return c.algorithm == GenerationAlgorithm::Surface && c.surface.buildingDensity > 0 && c.surface.upperFloorPercent <= 100 &&
        p.count == GroundProfile::surfaceWithFloorCount && !p.wall.empty() && !p.buildingStairs.empty() && p.buildingStairs.size() <= 256;
}

std::expected<CaveExitBrush, ResolveError> resolveBuildingStairsBrush(std::string_view name) {
    if (!wxIsMainThread()) return std::unexpected(ResolveError::WrongThread);
    if (!g_version.IsVersionLoaded()) return std::unexpected(ResolveError::NoClient);
    if (name.empty() || name.size() > 256) return std::unexpected(ResolveError::InvalidProfile);
    const auto* brush = g_brushes.getBrush(std::string(name));
    const auto* doodad = brush ? brush->as<DoodadBrush>() : nullptr;
    if (!doodad) return std::unexpected(ResolveError::InvalidProfile);
    return resolveCaveExitBrush(*doodad);
}

bool validDungeonProfile(const GroundProfile& p, const GenerationConfig& c) noexcept {
    if (!c.surface.buildingDungeons) return p.dungeonFloor.empty() && p.dungeonWall.empty() && p.dungeonStairs.empty() && p.dungeonDescent.empty();
    return c.algorithm == GenerationAlgorithm::Surface && c.surface.buildingDensity > 0 && c.surface.dungeonPercent <= 100 &&
        c.surface.caveEntrances > 0 && hasCave(p) && p.count == GroundProfile::surfaceWithFloorCount && !p.wall.empty() && !p.door.empty() &&
        std::ranges::all_of(std::array{&p.dungeonFloor, &p.dungeonWall, &p.dungeonStairs, &p.dungeonDescent},
            [](const std::string* name) { return !name->empty() && name->size() <= 256; });
}

std::expected<DungeonBrushes, ResolveError> resolveDungeonBrushes(const GroundProfile& p) {
    if (!wxIsMainThread()) return std::unexpected(ResolveError::WrongThread);
    if (!g_version.IsVersionLoaded()) return std::unexpected(ResolveError::NoClient);
    if (p.dungeonFloor.empty() || p.dungeonFloor.size() > 256) return std::unexpected(ResolveError::InvalidProfile);
    const auto* brush = g_brushes.getBrush(p.dungeonFloor);
    const auto* floor = brush ? brush->as<GroundBrush>() : nullptr;
    if (!floor || floor->getTotalChance() <= 0) return std::unexpected(ResolveError::InvalidProfile);
    DungeonBrushes result;
    result.floor = floor->getPlacementItemId();
    const auto def = g_item_definitions.get(result.floor);
    if (!def || !def.clientId() || def.isMetaItem() || !def.isGroundTile() || def.hasFlag(ItemFlag::Unpassable) ||
        std::ranges::none_of(floor->getBorderItems(), [&](const auto& variant) { return variant.id == result.floor; }))
        return std::unexpected(ResolveError::InvalidProfile);
    auto wall = resolveCaveDoorBrush(p.dungeonWall);
    if (!wall) return std::unexpected(wall.error());
    result.wall = *wall;
    auto stairs = resolveBuildingStairsBrush(p.dungeonStairs);
    if (!stairs) return std::unexpected(stairs.error());
    result.stairs = std::move(*stairs);
    // Iteration 45b: a ground brush (its placement ground) or a doodad brush (its first single item) naming a
    // full, walkable floor-change-down ground; the ramp's upper part shows as a black hole inside a building.
    if (p.dungeonDescent.empty() || p.dungeonDescent.size() > 256) return std::unexpected(ResolveError::InvalidProfile);
    const auto* descentBrush = g_brushes.getBrush(p.dungeonDescent);
    if (const auto* ground = descentBrush ? descentBrush->as<GroundBrush>() : nullptr) result.descent = ground->getPlacementItemId();
    else if (const auto* doodad = descentBrush ? descentBrush->as<DoodadBrush>() : nullptr;
             doodad && doodad->hasSingleObjects(0) && doodad->getSingleChance(0) > 0)
        if (const auto* item = doodad->getRandomSingleItem(0, 1)) result.descent = item->getID();
    const auto down = g_item_definitions.get(result.descent);
    if (!down || !down.clientId() || down.isMetaItem() || !down.isGroundTile() || !down.hasFlag(ItemFlag::FloorChangeDown) ||
        down.hasFlag(ItemFlag::Unpassable)) return std::unexpected(ResolveError::InvalidProfile);
    return result;
}

bool hasCave(const GroundProfile& profile) noexcept {
	return !profile.caveEntrance.empty() || !profile.caveExit.empty() || !profile.caveGround.empty() || !profile.caveWall.empty();
}

const WallBrush* caveWallBrushOf(const GroundProfile& profile) {
	if (profile.caveWall.empty() || profile.caveWall.size() > 256) return nullptr;
	const auto* brush = g_brushes.getBrush(profile.caveWall);
	const auto* wall = brush ? brush->as<WallBrush>() : nullptr;
	return wall && !wall->is<WallDecorationBrush>() ? wall : nullptr;
}

std::expected<CaveDoorBrush, ResolveError> resolveCaveDoorBrush(std::string_view name) {
    if (!wxIsMainThread()) return std::unexpected(ResolveError::WrongThread);
    if (!g_version.IsVersionLoaded()) return std::unexpected(ResolveError::NoClient);
    if (name.empty() || name.size() > 256) return std::unexpected(ResolveError::InvalidProfile);
    const auto* b = g_brushes.getBrush(std::string(name));
    const auto* wall = b ? b->as<WallBrush>() : nullptr;
    if (!wall || wall->is<WallDecorationBrush>()) return std::unexpected(ResolveError::InvalidProfile);
    CaveDoorBrush result {wall->getPlacementItemId()};
    for (const auto [alignment, target] : {std::pair{WALL_HORIZONTAL, &result.horizontal}, std::pair{WALL_VERTICAL, &result.vertical}})
        for (const auto& door : wall->items.getDoorItems(alignment)) {
            const auto def = g_item_definitions.get(door.id);
            // The client item must be a door: some brushes list plain wall pieces in their door slots (iteration 45c).
            if (door.type == WALL_DOOR_NORMAL && !door.locked && def && def.clientId() && !def.isMetaItem() && def.isDoor() &&
                !def.hasFlag(ItemFlag::IsOpen) && !def.isGroundTile()) { *target = door.id; break; }
        }
    if (!result.wall || !result.horizontal || !result.vertical) return std::unexpected(ResolveError::InvalidProfile);
    return result;
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
    if (config.surface.caveDoorCorridors && !resolveCaveDoorBrush(profile.caveDoor)) return std::unexpected(ResolveError::InvalidProfile);
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
	const auto* wallPieces = caveWallBrushOf(profile);
	const auto* entranceBrush = g_brushes.getBrush(profile.caveEntrance);
	const auto* hole = entranceBrush ? entranceBrush->as<GroundBrush>() : nullptr;
	const bool floorHole = config.surface.caveEntranceStyle == CaveEntranceStyle::FloorHole;
	if (!exit || !ground || !(wall || wallPieces) || (floorHole ? !hole : !entrance)) return std::unexpected(ResolveError::InvalidProfile);
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
	if (!validGround(ground) || (wall && !validGround(wall)) || (floorHole && !validGround(hole))) return std::unexpected(ResolveError::InvalidProfile);
	result.ground = ground->getPlacementItemId();
	result.wall = wall ? wall->getPlacementItemId() : result.ground;
	if (wallPieces) {
		// Iteration 50: every piece the wall logic may align to must be a real, impassable non-ground client item, so the
		// wall band blocks exactly like the cave's semantic plan expects and nothing walkable stands in for a wall.
		size_t pieces = 0;
		for (int alignment = 0; alignment < WallBrushItems::WALL_ALIGNMENT_COUNT; ++alignment)
			for (const auto& piece : wallPieces->items.getWallNode(alignment).items) {
				const auto def = g_item_definitions.get(piece.id);
				if (!def || !def.clientId() || def.isMetaItem() || def.isGroundTile() || !def.hasFlag(ItemFlag::Unpassable))
					return std::unexpected(ResolveError::InvalidProfile);
				++pieces;
			}
		result.wallPiece = wallPieces->getPlacementItemId();
		if (!pieces || !result.wallPiece) return std::unexpected(ResolveError::InvalidProfile);
	}
	if (!floorHole && entrance->getAlternativeCount() > 256) return std::unexpected(ResolveError::InvalidProfile);
	auto exitBrush = resolveCaveExitBrush(*exit);
	if (!exitBrush) return std::unexpected(exitBrush.error());
	static_cast<CaveExitBrush&>(result) = std::move(*exitBrush);
	const auto holeItem = [](const GroundBrush& brush) -> uint16_t {
		const auto id = brush.getPlacementItemId();
		const auto def = g_item_definitions.get(id);
		return def && def.isGroundTile() && def.hasFlag(ItemFlag::FloorChangeDown) ? id : 0;
	};
	// Iteration 48c: a plain walkable ground without any floor change; data without the brush keeps the ramp under holes.
	const auto* landingBrush = g_brushes.getBrush(std::string(holeLandingBrush));
	if (const auto* landing = landingBrush ? landingBrush->as<GroundBrush>() : nullptr; landing && validGround(landing)) {
		const auto id = landing->getPlacementItemId();
		const auto def = g_item_definitions.get(id);
		if (def && def.isGroundTile() && !def.hasFlag(ItemFlag::Unpassable) &&
			std::ranges::none_of(std::array{ItemFlag::FloorChange, ItemFlag::FloorChangeDown, ItemFlag::FloorChangeNorth,
				ItemFlag::FloorChangeSouth, ItemFlag::FloorChangeEast, ItemFlag::FloorChangeWest},
				[&](ItemFlag flag) { return def.hasFlag(flag); }))
			result.landing = id;
	}
	if (floorHole) {
		result.entrance = holeItem(*hole);
		if (!result.entrance) return std::unexpected(ResolveError::InvalidProfile);
	} else {
		// The ordinary entrances a rock-face style falls back to off the rock (iteration 48) look like the floor-hole style
		// (iteration 48b): the dialog's floor-hole brush. Data without it keeps the rock-face doodad there.
		const auto* fallback = g_brushes.getBrush(std::string(fallbackHoleBrush));
		if (const auto* ground = fallback ? fallback->as<GroundBrush>() : nullptr; ground && validGround(ground)) result.hole = holeItem(*ground);
		int priorAlternative = -1;
		size_t alternatives = 0;
		for (int i = 0; i < entrance->getTotalVariants(); ++i) {
			const auto variant = entrance->getVariant(i);
			if (variant.composite || !plainCaveItem(variant.single_item)) return std::unexpected(ResolveError::InvalidProfile);
			if (variant.alternative_index != priorAlternative) {
				const auto* item = entrance->getRandomSingleItem(variant.alternative_index, 1);
				if (!plainCaveItem(item)) return std::unexpected(ResolveError::InvalidProfile);
				if (alternatives++ == 0) result.entrance = item->getID();
				priorAlternative = variant.alternative_index;
			}
		}
		if (alternatives != entrance->getAlternativeCount()) return std::unexpected(ResolveError::InvalidProfile);
	}
	if (!result.entrance || !result.ground || !result.wall) return std::unexpected(ResolveError::InvalidProfile);
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
			if (pos.x == 0 && pos.y == 0 && pos.z == -1 && !hasUpperPart) { hasUpperPart = true; ids.opening = id; }
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

std::expected<BridgeBrush, ResolveError> resolveBridgeBrush(std::string_view name) {
    if (!wxIsMainThread()) return std::unexpected(ResolveError::WrongThread);
    if (!g_version.IsVersionLoaded()) return std::unexpected(ResolveError::NoClient);
    if (name.empty()) return BridgeBrush{};
    if (name.size() > 256) return std::unexpected(ResolveError::InvalidProfile);
    const auto* brush = g_brushes.getBrush(std::string(name));
    const auto* wall = brush ? brush->as<WallBrush>() : nullptr;
    if (!wall || wall->is<WallDecorationBrush>()) return std::unexpected(ResolveError::InvalidProfile);
    BridgeBrush result;
    for (const auto [alignment, output] : {std::pair{WALL_HORIZONTAL, &result.horizontal}, std::pair{WALL_VERTICAL, &result.vertical}}) {
        // First positive-chance variant, in XML order. No global editor RNG.
        for (const auto& item : wall->items.getWallNode(alignment).items) {
            if (item.chance <= 0) continue;
            const auto def = g_item_definitions.get(item.id);
            if (!def || !def.clientId() || !def.isGroundTile() || def.isMetaItem() || def.hasFlag(ItemFlag::Unpassable))
                return std::unexpected(ResolveError::InvalidItem);
            if (!*output) *output = item.id;
        }
        if (!*output) return std::unexpected(ResolveError::InvalidProfile);
    }
    return result;
}

std::expected<GroundPlan, ResolveError> resolveSurfacePlan(
	const GenerationInput& input, const SurfaceGrid& grid, const GroundProfile& profile,
	uint64_t maxPlanBytes, std::stop_token stop, const DecorGrid* decor, std::span<const uint8_t> keepClear) {
	if (!wxIsMainThread()) return std::unexpected(ResolveError::WrongThread);
	if (stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
	if (!g_version.IsVersionLoaded() || !g_version.getLoadedVersion()) return std::unexpected(ResolveError::NoClient);
	if (input.assetRevision != g_version.GetAssetRevision()) return std::unexpected(ResolveError::StaleAssets);
	if (grid.region != input.config.region || !grid.valid()) return std::unexpected(ResolveError::InvalidInput);
    if (!validPlateauProfile(profile, input.config) || !validCaveProfile(profile, input.config)) {
        return std::unexpected(ResolveError::InvalidProfile);
    }
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
	uint32_t grassMixShare = 0;
	for (size_t i = 0; i < GroundProfile::grassMixCount; ++i) {
		if (profile.grassMix[i].empty() != (profile.grassMixPermille[i] == 0) || profile.grassMix[i].size() > 256)
			return std::unexpected(ResolveError::InvalidProfile);
		grassMixShare += profile.grassMixPermille[i];
	}
	if (grassMixShare > 1000 || profile.grassPatchStep < 1 || profile.grassPatchStep > 64) return std::unexpected(ResolveError::InvalidProfile);
	// Tree and bush mixes, in that order (DecorKind Tree, Bush).
	const std::array<const std::array<std::string, GroundProfile::treeMixCount>*, 2> kindMixNames {&profile.treeMix, &profile.bushMix};
	const std::array<const std::array<uint16_t, GroundProfile::treeMixCount>*, 2> kindMixPermille {&profile.treeMixPermille, &profile.bushMixPermille};
	std::array<uint32_t, 2> kindMixShare {};
	for (size_t kind = 0; kind < 2; ++kind) {
		for (size_t i = 0; i < GroundProfile::treeMixCount; ++i) {
			const auto& name = (*kindMixNames[kind])[i];
			if (name.empty() != ((*kindMixPermille[kind])[i] == 0) || name.size() > 256 || (!vegetation && !name.empty()))
				return std::unexpected(ResolveError::InvalidProfile);
			kindMixShare[kind] += (*kindMixPermille[kind])[i];
		}
		if (kindMixShare[kind] > 1000) return std::unexpected(ResolveError::InvalidProfile);
	}
	bool overlaid = false;
	for (size_t i = 0; i < GroundProfile::overlayCount; ++i) {
		const auto& name = profile.overlay[i];
		const bool off = profile.overlayTreePpm[i] == 0 && profile.overlayGroundPpm[i] == 0;
		if (name.empty() != off || name.size() > 256 ||
			profile.overlayTreePpm[i] > 1000000 || profile.overlayGroundPpm[i] > 1000000) return std::unexpected(ResolveError::InvalidProfile);
		overlaid = overlaid || (vegetation && !name.empty()); // overlays come only with vegetation
	}
	for (size_t i = 0; i < GroundProfile::overlayCount; ++i) {
		const auto& name = profile.interior[i];
		if (name.empty() != (profile.interiorPpm[i] == 0) || name.size() > 256 || profile.interiorPpm[i] > 1000000)
			return std::unexpected(ResolveError::InvalidProfile);
		overlaid = overlaid || (vegetation && !name.empty());
	}
	// Wall stacks: real items, at most 1000000 ppm per side mask; they come with the walls.
	std::array<uint64_t, 16> stackShare {};
	bool stacked = false;
	for (const auto& stack : profile.wallStacks) {
		if (!stack.ppm) { if (stack != GroundProfile::WallStack {}) return std::unexpected(ResolveError::InvalidProfile); continue; }
		if (stack.sides > 15 || stack.ppm > 1000000 || (stackShare[stack.sides] += stack.ppm) > 1000000 || !stack.items[0])
			return std::unexpected(ResolveError::InvalidProfile);
		for (size_t k = 1; k < stack.items.size(); ++k) if (stack.items[k] && !stack.items[k - 1]) return std::unexpected(ResolveError::InvalidProfile);
		for (const uint16_t id : stack.items) {
			if (!id) break;
			const auto definition = g_item_definitions.get(id);
			if (!definition || !definition.clientId() || definition.isMetaItem() || definition.isGroundTile()) return std::unexpected(ResolveError::InvalidItem);
		}
		stacked = stacked || walled;
	}
	// Structures: boxes inside the limits, each cell inside its box once, real grounds and items.
	if (profile.structures.size() > GroundProfile::structureCount) return std::unexpected(ResolveError::InvalidProfile);
	for (const auto& structure : profile.structures) {
		if (!structure.ppm || structure.ppm > 1000000 || !structure.width || !structure.height || structure.width > GroundProfile::structureSide ||
			structure.height > GroundProfile::structureSide || structure.cells.empty() || structure.cells.size() > GroundProfile::structureCells)
			return std::unexpected(ResolveError::InvalidProfile);
		if (!structure.water.empty() && (structure.water.size() != size_t(structure.width) * structure.height ||
			std::ranges::any_of(structure.water, [](uint8_t w) { return w > 1; }))) return std::unexpected(ResolveError::InvalidProfile);
		std::vector<uint8_t> used(size_t(structure.width) * structure.height, 0);
		for (const auto& cell : structure.cells) {
			if (cell.dx >= structure.width || cell.dy >= structure.height || used[size_t(cell.dy) * structure.width + cell.dx]++ ||
				(!cell.items[0] && !cell.ground)) return std::unexpected(ResolveError::InvalidProfile);
			if (cell.ground) {
				const auto definition = g_item_definitions.get(cell.ground);
				if (!definition || !definition.clientId() || definition.isMetaItem() || !definition.isGroundTile()) return std::unexpected(ResolveError::InvalidItem);
			}
			for (size_t k = 1; k < cell.items.size(); ++k) if (cell.items[k] && !cell.items[k - 1]) return std::unexpected(ResolveError::InvalidProfile);
			for (const uint16_t id : cell.items) {
				if (!id) break;
				const auto definition = g_item_definitions.get(id);
				if (!definition || !definition.clientId() || definition.isMetaItem() || definition.isGroundTile()) return std::unexpected(ResolveError::InvalidItem);
			}
		}
	}
	if (profile.carpet.empty() != (profile.carpetPpm == 0) || profile.carpet.size() > 256 || profile.carpetPpm > 1000000)
		return std::unexpected(ResolveError::InvalidProfile);
	overlaid = overlaid || (vegetation && !profile.carpet.empty());
	for (size_t m = 0; m < GroundProfile::grassMixCount; ++m) {
		const bool defaults = profile.grassMixDecorKeep[m] == GroundProfile::keepAllDecor &&
			std::ranges::all_of(profile.grassMixDecor[m], &std::string::empty);
		if ((!defaults && (profile.grassMix[m].empty() || !vegetation)) ||
			std::ranges::any_of(profile.grassMixDecorKeep[m], [](uint16_t keep) { return keep > 1000; }) ||
			std::ranges::any_of(profile.grassMixDecor[m], [](const std::string& name) { return name.size() > 256; }))
			return std::unexpected(ResolveError::InvalidProfile);
	}
	if (grid.cells.size() > (std::numeric_limits<size_t>::max() - sizeof(GroundPlan)) / sizeof(uint16_t) ||
		grid.cells.size() > std::vector<uint16_t>().max_size()) return std::unexpected(ResolveError::PlanLimit);
	uint64_t payloadBytes = sizeof(GroundPlan) + uint64_t(grid.cells.size()) * (sizeof(uint16_t) * ((vegetation ? 2 : 1) + (fired ? 1 : 0) + (overlaid ? 1 : 0)) + (doored ? 1 : 0) + (stacked ? 1 : 0) + (profile.structures.empty() ? 0 : sizeof(uint16_t)));
	if (payloadBytes > maxPlanBytes) return std::unexpected(ResolveError::PlanLimit);
	for (const auto& name : std::views::join(std::array<std::span<const std::string>, 2> {profile.active(), profile.activeDecor()})) {
		if (name.empty() || name.size() > 256) return std::unexpected(ResolveError::InvalidProfile);
		if (name.size() + 1 > maxPlanBytes - payloadBytes) return std::unexpected(ResolveError::PlanLimit);
		payloadBytes += name.size() + 1;
	}
	for (const auto* name : {&profile.caveEntrance, &profile.caveExit, &profile.caveGround, &profile.caveWall, &profile.caveDoor,
		&profile.grassMix[0], &profile.grassMix[1], &profile.grassMix[2], &profile.treeMix[0], &profile.treeMix[1], &profile.treeMix[2], &profile.bushMix[0], &profile.bushMix[1], &profile.bushMix[2]}) {
		if (name->empty()) continue;
		if (name->size() + 1 > maxPlanBytes - payloadBytes) return std::unexpected(ResolveError::PlanLimit);
		payloadBytes += name->size() + 1;
	}
	for (const auto& names : profile.grassMixDecor) for (const auto& name : names) {
		if (name.empty()) continue;
		if (name.size() + 1 > maxPlanBytes - payloadBytes) return std::unexpected(ResolveError::PlanLimit);
		payloadBytes += name.size() + 1;
	}
	for (const auto* name : {&profile.waterTree, &profile.carpet}) {
		if (name->empty()) continue;
		if (name->size() + 1 > maxPlanBytes - payloadBytes) return std::unexpected(ResolveError::PlanLimit);
		payloadBytes += name->size() + 1;
	}
	for (const auto& name : std::views::join(std::array<std::span<const std::string>, 2> {profile.overlay, profile.interior})) {
		if (name.empty()) continue;
		if (name.size() + 1 > maxPlanBytes - payloadBytes) return std::unexpected(ResolveError::PlanLimit);
		payloadBytes += name.size() + 1;
	}
	try {
		const auto bridge = resolveBridgeBrush(profile.bridgeBrush);
		if (!bridge) return std::unexpected(bridge.error());
		const auto client = g_version.GetCurrentVersionID();
		if (client.size() > maxPlanBytes - payloadBytes || maxPlanBytes - payloadBytes - client.size() < 1) {
			return std::unexpected(ResolveError::PlanLimit);
		}
		// A ground brush and the sum of its placement chances, after checking every item it can place.
		const auto checkGround = [&](const std::string& name) -> std::expected<std::pair<const GroundBrush*, uint64_t>, ResolveError> {
			const auto* brush = g_brushes.getBrush(name);
			const auto* ground = brush ? brush->as<GroundBrush>() : nullptr;
			if (!ground || ground->getTotalChance() <= 0 || ground->getBorderItems().empty()) {
				return std::unexpected(ResolveError::InvalidProfile);
			}
			int previous = 0;
			for (const auto& variant : ground->getBorderItems()) {
				if (stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
				if (variant.chance < previous || variant.chance > ground->getTotalChance()) {
					return std::unexpected(ResolveError::InvalidProfile);
				}
				previous = variant.chance;
				const auto definition = g_item_definitions.get(variant.id);
				if (!variant.id || !definition || !definition.clientId() || definition.isMetaItem()) {
					return std::unexpected(ResolveError::InvalidItem);
				}
				const auto baseId = definition.attribute(ItemAttributeKey::BorderBaseGroundId);
				const auto base = definition.isGroundTile() ? definition :
					(baseId > 0 && baseId <= UINT16_MAX ? g_item_definitions.get(uint16_t(baseId)) : ItemDefinitionView{});
				if (!base || !base.isGroundTile() || !base.clientId() || base.isMetaItem() ||
					base.editorData().brush != ground) return std::unexpected(ResolveError::InvalidItem);
			}
			if (previous != ground->getTotalChance()) return std::unexpected(ResolveError::InvalidProfile);
			return std::pair{ground, uint64_t(previous)};
		};
		std::array<const GroundBrush*, kindCount> brushes {};
		std::array<uint64_t, kindCount> totals {};
		for (size_t index = 0; index < kinds; ++index) {
			const auto checked = checkGround(profile.brushes[index]);
			if (!checked) return std::unexpected(checked.error());
			std::tie(brushes[index], totals[index]) = *checked;
		}
		std::array<const GroundBrush*, GroundProfile::grassMixCount> grassMix {};
		std::array<uint64_t, GroundProfile::grassMixCount> grassMixTotals {};
		for (size_t m = 0; m < GroundProfile::grassMixCount; ++m) {
			if (profile.grassMix[m].empty()) continue;
			const auto checked = checkGround(profile.grassMix[m]);
			if (!checked) return std::unexpected(checked.error());
			std::tie(grassMix[m], grassMixTotals[m]) = *checked;
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
		// A doodad brush with single-tile items and the sum of their chances, or one RAW item; every reachable
		// item must be a real, plain, non-ground, non-border item.
		const auto checkDoodad = [&](const std::string& name) -> std::expected<DecorSource, ResolveError> {
			const DecorSource source = findDecorSource(name, maxDecorChance);
			if (!source) return std::unexpected(ResolveError::InvalidProfile);
			for (uint64_t roll = 1; roll <= (source.raw ? 1 : source.total); ++roll) {
				if (roll % 1024 == 0 && stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
				if (!decorItemPlaceable(source.item(roll))) return std::unexpected(ResolveError::InvalidItem);
			}
			return source;
		};
		std::array<DecorSource, decorKindCount> doodads {};
		for (size_t index = 0; vegetation && index < decorKindCount; ++index) {
			const auto checked = checkDoodad(profile.decor[index]);
			if (!checked) return std::unexpected(checked.error());
			doodads[index] = *checked;
		}
		std::array<std::array<DecorSource, GroundProfile::treeMixCount>, 2> kindMix {};
		for (size_t kind = 0; kind < 2; ++kind) {
			for (size_t index = 0; index < GroundProfile::treeMixCount; ++index) {
				if ((*kindMixNames[kind])[index].empty()) continue;
				const auto checked = checkDoodad((*kindMixNames[kind])[index]);
				if (!checked) return std::unexpected(checked.error());
				kindMix[kind][index] = *checked;
			}
		}
		std::array<std::array<DecorSource, decorKindCount>, GroundProfile::grassMixCount> mixDoodads {};
		for (size_t m = 0; m < GroundProfile::grassMixCount; ++m) {
			for (size_t k = 0; k < decorKindCount; ++k) {
				if (profile.grassMixDecor[m][k].empty()) continue;
				const auto checked = checkDoodad(profile.grassMixDecor[m][k]);
				if (!checked) return std::unexpected(checked.error());
				mixDoodads[m][k] = *checked;
			}
		}
		// Overlays must not block the way: paths and reachability were planned without them.
		std::array<DecorSource, GroundProfile::overlayCount> overlays {};
		for (size_t index = 0; index < GroundProfile::overlayCount; ++index) {
			if (!overlaid || profile.overlay[index].empty()) continue;
			const auto checked = checkDoodad(profile.overlay[index]);
			if (!checked) return std::unexpected(checked.error());
			for (uint64_t roll = 1; roll <= (checked->raw ? 1 : checked->total); ++roll)
				if (g_item_definitions.get(checked->item(roll)).hasFlag(ItemFlag::Unpassable)) return std::unexpected(ResolveError::InvalidItem);
			overlays[index] = *checked;
		}
		std::array<DecorSource, GroundProfile::overlayCount> interiors {};
		for (size_t index = 0; index < GroundProfile::overlayCount; ++index) {
			if (!overlaid || profile.interior[index].empty()) continue;
			const auto checked = checkDoodad(profile.interior[index]);
			if (!checked) return std::unexpected(checked.error());
			for (uint64_t roll = 1; roll <= (checked->raw ? 1 : checked->total); ++roll)
				if (g_item_definitions.get(checked->item(roll)).hasFlag(ItemFlag::Unpassable)) return std::unexpected(ResolveError::InvalidItem);
			interiors[index] = *checked;
		}
		// The carpet of the buildings: its brush, and the Floor cells it covers (carpetPpm of the inner Floor cells, those
		// with Floor on all four sides so no wall stands on them, the ones with the highest patch noise).
		const CarpetBrush* carpet = nullptr;
		std::vector<uint8_t> carpetCell;
		if (overlaid && !profile.carpet.empty()) {
			const auto* brush = g_brushes.getBrush(profile.carpet);
			carpet = brush ? brush->as<CarpetBrush>() : nullptr;
			if (!carpet) return std::unexpected(ResolveError::InvalidProfile);
			const size_t width = grid.region.width, height = grid.region.height;
			const auto floorAt = [&](size_t j) { return grid.cells[j] == SurfaceKind::Floor; };
			std::vector<std::pair<uint32_t, uint32_t>> ranked; // patch value, cell
			for (size_t i = 0; i < grid.cells.size(); ++i) {
				const size_t x = i % width, y = i / width;
				if (!floorAt(i) || !x || !y || x + 1 == width || y + 1 == height ||
					!floorAt(i - 1) || !floorAt(i + 1) || !floorAt(i - width) || !floorAt(i + width)) continue;
				ranked.emplace_back(grassPatchValue(input.config.seed ^ carpetStream, grid.region.x + int64_t(x), grid.region.y + int64_t(y)), uint32_t(i));
			}
			std::ranges::sort(ranked, std::greater {});
			carpetCell.assign(grid.cells.size(), 0);
			const size_t covered = size_t((uint64_t(ranked.size()) * profile.carpetPpm + 500000) / 1000000);
			for (size_t k = 0; k < covered; ++k) carpetCell[ranked[k].second] = 1;
		}
		DecorSource waterTree;
		if (vegetation && !profile.waterTree.empty()) {
			const auto checked = checkDoodad(profile.waterTree);
			if (!checked) return std::unexpected(checked.error());
			waterTree = *checked;
		}
		// Grass mix per cell (0 = the Grass brush, m + 1 = grassMix[m]), see assignGrassPatches.
		std::vector<uint8_t> grassSlot;
		if (grassMixShare) {
			std::array<uint16_t, grassPatchKinds> shares {};
			for (size_t m = 0; m < GroundProfile::grassMixCount; ++m) shares[m] = grassMix[m] ? profile.grassMixPermille[m] : 0;
			grassSlot = assignGrassPatches(grid, input.config.seed, shares, profile.grassPatchStep, stop);
			if (stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
		}
		// Tree and bush brush per decor cell (0 = the main brush, m + 1 = the kind's mix m), chosen by smooth
		// noise so a species grows in groves rather than one tile here, one there. The cells of a kind are
		// ranked by their noise and split by the shares, which keeps the learned shares in any region.
		std::vector<uint8_t> kindSlot;
		if (vegetation && (kindMixShare[0] || kindMixShare[1])) {
			kindSlot.assign(grid.cells.size(), 0);
			std::vector<std::pair<uint32_t, uint32_t>> ranked; // grove value, cell
			for (size_t kind = 0; kind < 2; ++kind) {
				if (!kindMixShare[kind]) continue;
				const auto placed = kind == 0 ? DecorKind::Tree : DecorKind::Bush;
				ranked.clear();
				for (size_t i = 0; i < grid.cells.size(); ++i) {
					if (decor->cells[i] != placed) continue;
					const size_t patch = grassSlot.empty() ? 0 : grassSlot[i];
					if (patch && !profile.grassMixDecor[patch - 1][size_t(placed) - 1].empty()) continue; // the patch brush
					ranked.emplace_back(grassPatchValue(input.config.seed ^ (kind == 0 ? treeMixStream : bushMixStream),
						grid.region.x + int64_t(i % grid.region.width), grid.region.y + int64_t(i / grid.region.width)), uint32_t(i));
				}
				std::ranges::sort(ranked);
				size_t from = ranked.size() * (1000 - kindMixShare[kind]) / 1000; // the main brush takes the lowest values
				for (size_t m = 0; m < GroundProfile::treeMixCount; ++m) {
					const size_t count = ranked.size() * (*kindMixPermille[kind])[m] / 1000;
					for (size_t k = from; k < std::min(ranked.size(), from + count); ++k) kindSlot[ranked[k].second] = uint8_t(m + 1);
					from += count;
				}
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
		// Ruin tiles without their floor take the land (Grass) brush; they stay Floor for the walls.
		std::vector<uint8_t> bareCell;
		if (!grid.bare.empty()) {
			bareCell.assign(grid.cells.size(), 0);
			for (const uint32_t cell : grid.bare) bareCell[cell] = 1;  // valid() already checked the range
		}
		// Loose floor tiles around ruins take the floor brush while they are still Grass or Dirt.
		std::vector<uint8_t> strewnCell;
		if (!grid.strewn.empty() && kinds > size_t(SurfaceKind::Floor) - 1) {
			strewnCell.assign(grid.cells.size(), 0);
			for (const uint32_t cell : grid.strewn) strewnCell[cell] = 1;  // valid() already checked the range
		}
		// The entrance of every building: a door with doors, an open gap in the wall without them.
		std::vector<uint8_t> doorCell;
		if (walled) {
			doorCell.assign(grid.cells.size(), 0);
			for (const uint32_t door : grid.doors) doorCell[door] = 1;  // valid() already checked the range
		}
		if (doored) result.doors.assign(grid.cells.size(), 0);
		std::vector<StableRandom> streams;
		streams.reserve(kinds);
		for (size_t index = 0; index < kinds; ++index) streams.emplace_back(input.config.seed, variantStreams[index]);
		std::vector<StableRandom> decorRng;
		decorRng.reserve(decorKindCount);
		if (vegetation) for (const auto stream : decorStreams) decorRng.emplace_back(input.config.seed, stream);
		// Own streams, advanced at every cell like the others, so a mix never shifts the plain variants.
		std::vector<StableRandom> grassMixRng;
		for (const auto stream : grassPatchStreams) grassMixRng.emplace_back(input.config.seed, stream);
		StableRandom mixDecorRng(input.config.seed, mixDecorStream);
		StableRandom overlayRng(input.config.seed, overlayStream);
		if (overlaid) result.overlays.assign(grid.cells.size(), 0);
		for (size_t i = 0; i < grid.cells.size(); ++i) {
			if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
			std::array<int, kindCount> rolls {};
			for (size_t kind = 0; kind < kinds; ++kind) rolls[kind] = int(streams[kind].bounded(totals[kind])) + 1;
			std::array<int, decorKindCount> decorRolls {};
			if (vegetation) for (size_t kind = 0; kind < decorKindCount; ++kind) {
				decorRolls[kind] = int(decorRng[kind].bounded(doodads[kind].total)) + 1;
			}
			std::array<int, GroundProfile::grassMixCount> grassMixRolls {};
			for (size_t m = 0; m < GroundProfile::grassMixCount; ++m)
				if (grassMix[m]) grassMixRolls[m] = int(grassMixRng[m].bounded(grassMixTotals[m])) + 1;
			const uint64_t mixDecorRoll = grassMixShare && vegetation ? mixDecorRng.bounded(1000) : 0;
			const uint64_t overlayRoll = overlaid ? overlayRng.bounded(1000000) : 0;
			const uint64_t overlayPick = overlaid ? overlayRng.bounded(DecorSource::rawTotal * 100) : 0;
			const bool writable = input.mask[i] && !input.protection[i] && (keepClear.empty() || !keepClear[i]);
			if ((grid.cells[i] != SurfaceKind::Preserve) != writable) return std::unexpected(ResolveError::InvalidInput);
			if (!writable) continue;
			const bool isBridge = grid.cells[i] == SurfaceKind::Bridge;
			const bool strewnFloor = !strewnCell.empty() && strewnCell[i] && (grid.cells[i] == SurfaceKind::Grass || grid.cells[i] == SurfaceKind::Dirt);
			const auto kind = isBridge ? size_t(SurfaceKind::Shore) - 1 : strewnFloor ? size_t(SurfaceKind::Floor) - 1 : !bareCell.empty() && bareCell[i] && grid.cells[i] == SurfaceKind::Floor ? size_t(SurfaceKind::Grass) - 1 :
                input.config.surface.passes && grid.cells[i] == SurfaceKind::Path && kinds == GroundProfile::surfaceCount
                    ? size_t(SurfaceKind::Dirt) - 1 : size_t(grid.cells[i]) - 1;
			if (kind >= kinds) return std::unexpected(ResolveError::InvalidInput); // a path or floor cell needs its brush
			const size_t mixSlot = grassSlot.empty() || strewnFloor ? 0 : grassSlot[i];
			const auto id = mixSlot ? grassMix[mixSlot - 1]->getPlacementItemIdForRoll(grassMixRolls[mixSlot - 1])
				: brushes[kind]->getPlacementItemIdForRoll(rolls[kind]);
			if (!id || !*id) return std::unexpected(ResolveError::InvalidItem);
			result.items[i] = *id;
            if (isBridge && bridge->horizontal) {
                const auto w = grid.region.width;
                const auto x = i % w, y = i / w;
                const auto connects = [&](size_t j) { return grid.cells[j] == SurfaceKind::Bridge; };
                const bool horizontal = (x && connects(i - 1)) || (x + 1 < w && connects(i + 1));
                const bool vertical = (y && connects(i - w)) || (y + 1 < grid.region.height && connects(i + w));
                const auto land = [&](size_t j) { const auto k = grid.cells[j]; return k != SurfaceKind::Lake && k != SurfaceKind::River && k != SurfaceKind::Preserve; };
                const bool horizontalBanks = x && x + 1 < w && land(i - 1) && land(i + 1);
                const bool useHorizontal = grid.bridgeAxes.empty() ? horizontal || (!vertical && horizontalBanks) :
                    grid.bridgeAxes[i] == BridgeAxis::Horizontal;
                result.items[i] = useHorizontal ? bridge->horizontal : bridge->vertical;
            }
			if (walled && grid.cells[i] == SurfaceKind::Floor) {
				// Outline of a foundation: a Floor cell with a 4-neighbour of another kind (or outside the area).
				const size_t width = grid.region.width, height = grid.region.height, x = i % width, y = i / width;
				const bool outline = x == 0 || y == 0 || x + 1 == width || y + 1 == height || grid.cells[i - 1] != SurfaceKind::Floor ||
					grid.cells[i + 1] != SurfaceKind::Floor || grid.cells[i - width] != SurfaceKind::Floor || grid.cells[i + width] != SurfaceKind::Floor;
				if (outline && (gapCell.empty() || !gapCell[i]) && (doored || !doorCell[i])) result.walls[i] = wallItem;
				if (outline && doored && doorCell[i]) result.doors[i] = 1;
			}
			if (vegetation) {
				const auto placed = decor->cells[i];
				// On a grass mix patch the decoration thins to the patch's own rate and takes its own brush.
				const size_t patch = grassSlot.empty() ? 0 : grassSlot[i];
				const bool kept = placed != DecorKind::None &&
					(!patch || mixDecorRoll < profile.grassMixDecorKeep[patch - 1][size_t(placed) - 1]);
				const bool inWater = grid.cells[i] == SurfaceKind::Lake || grid.cells[i] == SurfaceKind::River;
				// A brush other than the main one draws its variant from its own full chance range, by a hash of the
				// map position so the other streams are not shifted.
				const auto variantRoll = [&](uint64_t total) {
					const uint64_t ax = uint64_t(grid.region.x + int64_t(i % grid.region.width)), ay = uint64_t(grid.region.y + int64_t(i / grid.region.width));
					return mixHash(input.config.seed ^ variantStream ^ mixHash(ax * 0x100000001b3ULL ^ ay)) % total + 1;
				};
				if (kept && inWater) {
					// A tree standing in the water: its own brush, or none without one.
					if (placed == DecorKind::Tree && waterTree) {
						const uint64_t roll = variantRoll(waterTree.total);
						if (!(result.decor[i] = waterTree.item(roll))) return std::unexpected(ResolveError::InvalidItem);
					}
				} else if (kept) {
					const size_t slot = size_t(placed) - 1;
					const DecorSource* doodad = &doodads[slot];
					uint64_t roll = uint64_t(decorRolls[slot]);
					if (patch && mixDoodads[patch - 1][slot]) {
						doodad = &mixDoodads[patch - 1][slot];
						roll = variantRoll(doodad->total);
					} else if (const size_t mix = kindSlot.empty() ? 0 : kindSlot[i]; mix) {
						// A grove of a mix brush.
						const size_t kind = placed == DecorKind::Tree ? 0 : 1;
						doodad = &kindMix[kind][mix - 1];
						roll = variantRoll(doodad->total);
					}
					const uint16_t item = doodad->item(roll);
					if (!item) return std::unexpected(ResolveError::InvalidItem);
					result.decor[i] = item;
				}
				// One overlay per cell: on a tree by the tree rates, on other Grass or Dirt by the ground rates. The rates
				// stack up in order, so overlay k hits on its own share of the cells.
				const SurfaceKind cell = grid.cells[i];
				const bool tree = result.decor[i] && placed == DecorKind::Tree && !inWater;
				if (overlaid && (tree || cell == SurfaceKind::Grass || cell == SurfaceKind::Dirt)) {
					uint64_t upTo = 0;
					for (size_t k = 0; k < GroundProfile::overlayCount; ++k) {
						if (!overlays[k]) continue;
						// Each overlay grows in its own patches (cobwebs, shells and debris lie in heaps): only on about a
						// third of the land, the cells with the highest patch noise, at three times its rate (about the same overall).
						const uint64_t rate = tree ? profile.overlayTreePpm[k] : profile.overlayGroundPpm[k];
						const uint64_t patchSeed = input.config.seed ^ mixHash(overlayStream + k);
						const bool patch = grassPatchValue(patchSeed, grid.region.x + int64_t(i % grid.region.width),
							grid.region.y + int64_t(i / grid.region.width)) >= overlayPatchCut;
						upTo += patch ? std::min<uint64_t>(1000000, rate * 3) : 0;
						if (overlayRoll >= upTo) continue;
						result.overlays[i] = overlays[k].item(overlayPick % overlays[k].total + 1);
						break;
					}
				}
				// Inside a building: a Floor cell without a wall piece (its open floor, a gap, or a ruin's bare patch).
				if (overlaid && cell == SurfaceKind::Floor && (result.walls.empty() || !result.walls[i]) && (carpetCell.empty() || !carpetCell[i])) {
					uint64_t upTo = 0;
					for (size_t k = 0; k < GroundProfile::overlayCount; ++k) {
						if (!interiors[k]) continue;
						upTo += profile.interiorPpm[k];
						if (overlayRoll >= upTo) continue;
						result.overlays[i] = interiors[k].item(overlayPick % interiors[k].total + 1);
						break;
					}
				}
			}
		}
		// Carpet pieces by their eight neighbours, as the carpet brush aligns them in the editor; the piece of an alignment
		// is drawn by its chance from a hash of the map position.
		if (carpet) {
			const size_t width = grid.region.width, height = grid.region.height;
			constexpr std::array<std::pair<int, int>, 8> around {{{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};
			const auto& groups = carpet->getItems().getGroups();
			for (size_t i = 0; i < grid.cells.size(); ++i) {
				if (!carpetCell[i]) continue;
				const int x = int(i % width), y = int(i / width);
				uint32_t mask = 0;
				for (size_t b = 0; b < around.size(); ++b) {
					const int nx = x + around[b].first, ny = y + around[b].second;
					if (nx >= 0 && ny >= 0 && nx < int(width) && ny < int(height) && carpetCell[size_t(ny) * width + nx]) mask |= 1u << b;
				}
				const size_t alignment = CarpetBrush::alignmentFor(uint8_t(mask));
				const auto* group = alignment < groups.size() && groups[alignment].total_chance > 0 ? &groups[alignment] : &groups[CARPET_CENTER];
				if (group->total_chance <= 0) return std::unexpected(ResolveError::InvalidItem);
				const uint64_t ax = uint64_t(grid.region.x + int64_t(x)), ay = uint64_t(grid.region.y + int64_t(y));
				int64_t roll = int64_t(mixHash(input.config.seed ^ carpetStream ^ mixHash(ax * 0x100000001b3ULL ^ ay)) % uint64_t(group->total_chance));
				for (const auto& piece : group->items) {
					if (roll < piece.chance) { result.overlays[i] = piece.id; break; }
					roll -= piece.chance;
				}
				if (!result.overlays[i]) return std::unexpected(ResolveError::InvalidItem);
			}
		}
		// Wall stacks, once every wall piece is known: a wall cell (doors keep their piece) may take a stack learned for the
		// sides its neighbouring pieces are on, in place of its piece. The masks are read before any piece gives way; each
		// cell rolls by a hash of its map position, so the other streams are not shifted.
		if (stacked && !result.walls.empty()) {
			result.wallStack.assign(grid.cells.size(), 0);
			const size_t width = grid.region.width, height = grid.region.height;
			std::vector<std::pair<size_t, uint8_t>> pieces; // cell, sides
			for (size_t i = 0; i < grid.cells.size(); ++i) {
				if (!result.walls[i] || (!result.doors.empty() && result.doors[i])) continue;
				const size_t x = i % width, y = i / width;
				const uint8_t sides = uint8_t((y && result.walls[i - width] ? 1 : 0) | (x + 1 < width && result.walls[i + 1] ? 2 : 0) |
					(y + 1 < height && result.walls[i + width] ? 4 : 0) | (x && result.walls[i - 1] ? 8 : 0));
				pieces.emplace_back(i, sides);
			}
			for (const auto [i, sides] : pieces) {
				const uint64_t ax = uint64_t(grid.region.x + int64_t(i % width)), ay = uint64_t(grid.region.y + int64_t(i / width));
				const uint64_t chance = mixHash(input.config.seed ^ wallStackStream ^ mixHash(ax * 0x100000001b3ULL ^ ay)) % 1000000;
				uint64_t upTo = 0;
				for (size_t k = 0; k < profile.wallStacks.size(); ++k) {
					if (profile.wallStacks[k].sides != sides || !profile.wallStacks[k].ppm) continue;
					upTo += profile.wallStacks[k].ppm;
					if (chance >= upTo) continue;
					result.wallStack[i] = uint8_t(k + 1);
					result.walls[i] = 0;
					break;
				}
			}
		}
		// Structures, last. A structure's box and a ring of one cell around it lie on writable land (Grass, Dirt, Shore)
		// or water (Lake, River) clear of walls, fires and strewn floor, so it never touches a path, a building or another
		// structure. Its tiles without a ground of their own keep the land or water they had in the sample: at most a tenth of
		// them may differ. Each structure is placed about ppm times per cell of the area (the fraction by a hash), at the
		// first fitting positions of a scan that starts at a hash of the seed.
		if (!profile.structures.empty()) {
			const size_t width = grid.region.width, height = grid.region.height;
			// 0 = taken, 1 = land, 2 = water
			std::vector<uint8_t> ground(grid.cells.size(), 0);
			for (size_t i = 0; i < grid.cells.size(); ++i) {
				const bool writable = input.mask[i] && !input.protection[i] && (keepClear.empty() || !keepClear[i]);
				const auto kind = grid.cells[i];
				if (!writable || !result.items[i] || (!strewnCell.empty() && strewnCell[i]) || (!result.walls.empty() && result.walls[i]) ||
					(!result.wallStack.empty() && result.wallStack[i]) || (!result.fires.empty() && result.fires[i])) continue;
				if (kind == SurfaceKind::Grass || kind == SurfaceKind::Dirt || kind == SurfaceKind::Shore) ground[i] = 1;
				if (kind == SurfaceKind::Lake || kind == SurfaceKind::River) ground[i] = 2;
			}
			std::vector<uint8_t> own; // tiles of the box with a ground of the structure
			for (size_t k = 0; k < profile.structures.size(); ++k) {
				const auto& structure = profile.structures[k];
				const size_t sw = structure.width, sh = structure.height;
				if (width < sw + 2 || height < sh + 2) continue;
				own.assign(sw * sh, 0);
				for (const auto& cell : structure.cells) own[size_t(cell.dy) * sw + cell.dx] = cell.ground != 0;
				size_t constrained = 0;
				for (const uint8_t o : own) constrained += !o;
				const size_t tolerance = constrained / 10;
				const uint64_t expected = uint64_t(structure.ppm) * grid.cells.size();
				const uint64_t base = input.config.seed ^ structureStream ^ (uint64_t(k) << 40);
				uint64_t copies = expected / 1000000 + (mixHash(base) % 1000000 < expected % 1000000 ? 1 : 0);
				const size_t spanX = width - sw - 1, spanY = height - sh - 1, positions = spanX * spanY;
				const size_t first = size_t(mixHash(base ^ 0x9e3779b97f4a7c15ULL) % positions);
				for (size_t step = 0; copies && step < positions; ++step) {
					if (step % 4096 == 0 && stop.stop_requested()) return std::unexpected(ResolveError::Cancelled);
					const size_t position = (first + step) % positions;
					const size_t x0 = 1 + position % spanX, y0 = 1 + position / spanX;
					size_t misses = 0;
					bool fits = true;
					for (size_t y = y0 - 1; fits && y <= y0 + sh; ++y) for (size_t x = x0 - 1; fits && x <= x0 + sw; ++x) {
						const uint8_t here = ground[y * width + x];
						if (!here) { fits = false; break; }
						if (x < x0 || y < y0 || x >= x0 + sw || y >= y0 + sh) continue;
						const size_t b = (y - y0) * sw + (x - x0);
						if (own[b]) continue;
						const bool wet = !structure.water.empty() && structure.water[b];
						if ((here == 2) != wet && ++misses > tolerance) fits = false;
					}
					if (!fits) continue;
					if (result.structure.empty()) result.structure.assign(grid.cells.size(), 0);
					for (size_t c = 0; c < structure.cells.size(); ++c) {
						const size_t i = (y0 + structure.cells[c].dy) * width + x0 + structure.cells[c].dx;
						result.structure[i] = uint16_t(k * 256 + c + 1);
						if (!result.decor.empty()) result.decor[i] = 0;
						if (!result.overlays.empty()) result.overlays[i] = 0;
					}
					for (size_t y = y0 - 1; y <= y0 + sh; ++y) for (size_t x = x0 - 1; x <= x0 + sw; ++x) ground[y * width + x] = 0;
					--copies;
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
