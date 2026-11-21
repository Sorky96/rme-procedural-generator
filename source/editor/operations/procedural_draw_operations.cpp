// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "editor/operations/draw_operations.h"

#include "app/managers/version_manager.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/ground/ground_border_calculator.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/terrain_placement.h"
#include "brushes/door/door_brush.h"
#include "game/complexitem.h"
#include "brushes/wall/wall_brush.h"
#include "procedural/adapter/door_types.h"
#include "procedural/adapter/decor_source.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/map.h"
#include "map/tile_operations.h"
#include "procedural/adapter/generation_snapshot.h"
#include "procedural/adapter/ground_resolver.h"
#include "procedural/adapter/surface_resolver.h"
#include "procedural/adapter/snapshot_equal.h"
#include "procedural/adapter/snapshot_memory.h"
#include <algorithm>
#include <chrono>
#include <new>
#include <string_view>
#include <typeinfo>
#include <wx/thread.h>

namespace {
using namespace rme::procedural;
constexpr uint64_t maxBorderItems = 128;
constexpr uint64_t borderScratchBytes = 4096;

bool sameConfig(const GenerationConfig& a, const GenerationConfig& b) noexcept { return a == b; }

bool emptyContent(const Tile* tile) noexcept {
	return emptyTerrainContent(tile);
}

bool protectedCell(const SnapshotCell& cell) noexcept {
	if (cell.spawnCount || cell.waypointCount || cell.townCount || !cell.houseExits.empty()) return true;
	const auto* tile = cell.original.get();
	return tile && (tile->ground || !tile->items.empty() || tile->creature || tile->spawn ||
		tile->house_id || tile->mapflags || tile->extra ||
		(tile->statflags & ~uint16_t(TILESTATE_SELECTED | TILESTATE_MODIFIED)));
}

// Explicit fields only: no addresses, object padding or platform-sized values.
void hashField(uint64_t& hash, uint64_t value, unsigned bytes = 8) noexcept {
	for (unsigned i = 0; i < bytes; ++i) {
		hash = (hash ^ uint8_t(value)) * 1099511628211ULL;
		value >>= 8;
	}
}

uint64_t hashPreviewSource(const GenerationSnapshot& snapshot, const GroundPlan& plan) noexcept {
	uint64_t hash = 14695981039346656037ULL;
	hashField(hash, 0x736f757263655f31ULL);
	hashField(hash, generationAlgorithmVersion, 4);
	const auto region = [&](const GenerationRegion& r) {
		hashField(hash, uint32_t(r.x), 4);
		hashField(hash, uint32_t(r.y), 4);
		hashField(hash, uint32_t(r.z), 4);
		hashField(hash, r.width, 4);
		hashField(hash, r.height, 4);
	};
	const auto config = [&](const GenerationConfig& c) {
		region(c.region);
		hashField(hash, c.seed);
		hashField(hash, c.grassWeight, 4);
		hashField(hash, c.dirtWeight, 4);
		hashField(hash, c.waterWeight, 4);
		// GroundRoll keeps its original wire layout; only Surface adds its algorithm and tuning.
		if (c.algorithm != GenerationAlgorithm::GroundRoll) {
			hashField(hash, uint8_t(c.algorithm), 1);
			hashField(hash, c.vegetation, 1);
			for (const auto value : {c.surface.featureSize, c.surface.lakePermille, c.surface.minLakeCells, c.surface.riverCount,
				c.surface.minRiverLength, c.surface.shoreWidth, c.surface.rockPermille, c.surface.minRockCells,
				c.surface.dirtPermille, c.surface.transitionWidth}) hashField(hash, value, 4);
			// Appended only when set, so plans with the original river routes keep their fingerprint.
			if (c.surface.meander) hashField(hash, c.surface.meander, 4);
			if (c.surface.islandPermille) hashField(hash, c.surface.islandPermille, 4);
			if (c.surface.dirtSize) hashField(hash, c.surface.dirtSize, 4);
			if (c.surface.pathLength) hashField(hash, c.surface.pathLength, 4);
			if (c.surface.buildingDensity) hashField(hash, c.surface.buildingDensity, 4);
			if (c.surface.fordLength) hashField(hash, c.surface.fordLength, 4);
			if (c.surface.rockPasses) hashField(hash, c.surface.rockPasses, 4);
			if (c.surface.shoreSmoothing) hashField(hash, c.surface.shoreSmoothing, 4);
			if (c.surface.shoreRadius) hashField(hash, c.surface.shoreRadius, 4);
			if (c.surface.repairReach) hashField(hash, c.surface.repairReach, 4);
			if (c.surface.ruinPercent) hashField(hash, c.surface.ruinPercent, 4);
			if (c.surface.campCount) hashField(hash, c.surface.campCount, 4);
			// Appended only when it differs from the built-in rules, so other plans keep their original fingerprint.
			if (c.vegetation && !(c.decor == DecorTuning {})) {
				for (const auto value : {c.decor.treeScalePermille, c.decor.waterTreePermille, c.decor.bushPermille,
					c.decor.waterBushPermille, c.decor.dirtStonePermille, c.decor.rockStonePermille}) hashField(hash, value, 4);
				if (c.decor.clumpPermille) hashField(hash, c.decor.clumpPermille, 4);
				if (c.decor.shoreTreePermille) hashField(hash, c.decor.shoreTreePermille, 4);
			}
		}
	};
	const auto string = [&](std::string_view value) {
		hashField(hash, value.size());
		for (const unsigned char byte : value) hashField(hash, byte, 1);
	};
	const auto& input = snapshot.input;
	config(input.config);
	hashField(hash, input.extent.width, 4);
	hashField(hash, input.extent.height, 4);
	hashField(hash, input.extent.layers, 4);
	hashField(hash, input.mapGeneration);
	hashField(hash, input.mapRevision);
	hashField(hash, input.assetRevision);
	hashField(hash, input.mask.size());
	for (const auto byte : input.mask) hashField(hash, byte, 1);
	hashField(hash, input.protection.size());
	for (const auto byte : input.protection) hashField(hash, byte, 1);
	region(snapshot.context);
	hashField(hash, snapshot.mapWidth, 4);
	hashField(hash, snapshot.mapHeight, 4);
	hashField(hash, snapshot.mapFormat, 4);
	hashField(hash, snapshot.mapClientVersion, 4);
	hashField(hash, snapshot.cells.size());
	config(plan.config);
	hashField(hash, plan.mapGeneration);
	hashField(hash, plan.mapRevision);
	hashField(hash, plan.assetRevision);
	hashField(hash, plan.semanticFingerprint);
	string(plan.client);
	for (const auto& name : plan.profile.active()) string(name);
	for (const auto& name : plan.profile.activeDecor()) string(name);
	for (size_t i = 0; i < GroundProfile::grassMixCount; ++i) {
		string(plan.profile.grassMix[i]);
		hashField(hash, plan.profile.grassMixPermille[i], 2);
		for (size_t k = 0; k < GroundProfile::decorationCount; ++k) {
			hashField(hash, plan.profile.grassMixDecorKeep[i][k], 2);
			string(plan.profile.grassMixDecor[i][k]);
		}
	}
	for (size_t i = 0; i < GroundProfile::treeMixCount; ++i) {
		string(plan.profile.treeMix[i]);
		hashField(hash, plan.profile.treeMixPermille[i], 2);
		string(plan.profile.bushMix[i]);
		hashField(hash, plan.profile.bushMixPermille[i], 2);
	}
	if (!plan.profile.waterTree.empty()) string(plan.profile.waterTree); // only when set, so other plans keep their hash
	if (plan.profile.grassPatchStep != 10) hashField(hash, plan.profile.grassPatchStep, 2); // likewise only when learned
	hashField(hash, plan.items.size());
	for (const auto id : plan.items) hashField(hash, id, 2);
	hashField(hash, plan.decor.size());
	for (const auto id : plan.decor) hashField(hash, id, 2);
	if (!plan.walls.empty()) {
		hashField(hash, plan.walls.size());
		for (const auto id : plan.walls) hashField(hash, id, 2);
	}
	if (!plan.fires.empty()) {
		hashField(hash, plan.fires.size());
		for (const auto id : plan.fires) hashField(hash, id, 2);
	}
	// Only with overlays, so plans without them keep their hash.
	if (!plan.overlays.empty()) {
		hashField(hash, 0x76, 1);
		for (size_t i = 0; i < GroundProfile::overlayCount; ++i) {
			string(plan.profile.overlay[i]);
			hashField(hash, plan.profile.overlayTreePpm[i], 4);
			hashField(hash, plan.profile.overlayGroundPpm[i], 4);
		}
		hashField(hash, plan.overlays.size());
		for (const auto id : plan.overlays) hashField(hash, id, 2);
	}
	for (const auto layer : {std::span(plan.entrances), std::span(plan.exits)}) {
		if (layer.empty()) continue;
		hashField(hash, layer.size());
		for (const auto id : layer) hashField(hash, id, 2);
	}
	if (!plan.caveFloor.empty()) {
		hashField(hash, 0x62, 1);
		for (const auto cell : plan.caveFloor) hashField(hash, cell, 1);
	}
	if (hasCave(plan.profile)) {
		string(plan.profile.caveEntrance); string(plan.profile.caveExit); string(plan.profile.caveGround);
		hashField(hash, plan.config.surface.caveEntrances, 4);
		hashField(hash, uint8_t(plan.config.surface.caveEntranceStyle), 1);
		if (!plan.profile.caveWall.empty()) string(plan.profile.caveWall);
        if (plan.config.surface.caveFarExits || plan.config.surface.caveDoorCorridors) {
            hashField(hash, plan.config.surface.caveFarExits, 1);
            hashField(hash, plan.config.surface.caveDoorCorridors, 1);
            if (!plan.profile.caveDoor.empty()) string(plan.profile.caveDoor);
        }
	}
    if (plan.config.surface.plateauMinTop) {
        hashField(hash, plan.config.surface.plateauMinTop, 4);
        if (plan.config.surface.plateauLevels > 1) {
            hashField(hash, plan.config.surface.plateauLevels, 4);
        }
        if (plan.config.surface.plateauMaxPercent) {
            hashField(hash, plan.config.surface.plateauMaxPercent, 4);
        }
        string(plan.profile.plateauGround);
        string(plan.profile.plateauCliff);
        string(plan.profile.plateauRamp);
        for (auto c : plan.plateauFloor) {
            hashField(hash, c, 1);
        }
        for (auto id : plan.plateauRamps) {
            hashField(hash, id, 2);
        }
    }
    if (plan.config.surface.buildingStoreys) {
        hashField(hash, plan.config.surface.upperFloorPercent, 4);
        string(plan.profile.buildingStairs);
        hashField(hash, plan.plateauFloor.size());
        for (auto c : plan.plateauFloor) hashField(hash, c, 1);
        hashField(hash, plan.stairs.size());
        for (auto id : plan.stairs) hashField(hash, id, 2);
    }
    if (plan.config.surface.buildingDungeons) {
        hashField(hash, plan.config.surface.dungeonPercent, 4);
        string(plan.profile.dungeonFloor); string(plan.profile.dungeonWall); string(plan.profile.dungeonStairs); string(plan.profile.dungeonDescent);
        hashField(hash, plan.doors.size());
        for (const auto door : plan.doors) hashField(hash, door, 1);
    }
    return hash;
}

bool hashItem(uint64_t& hash, const Item* item, bool ground, std::span<const uint16_t> decorIds) noexcept {
	// RME turns a door piece into a Door (a plain Item subclass with a house door id); only an unassigned one is generated.
	const bool plain = item && (typeid(*item) == typeid(Item) || (typeid(*item) == typeid(Door) && static_cast<const Door*>(item)->getDoorID() == 0));
	if (!plain || item->getInvalidOTBMData() ||
		(item->getAttributesView() && !item->getAttributesView()->empty())) return false;
	const auto definition = g_item_definitions.get(item->getID());
	if (!definition || !definition.clientId() || definition.isMetaItem() ||
		(ground ? !definition.isGroundTile() : !(item->isBorder() || std::ranges::binary_search(decorIds, item->getID())))) return false;
	hashField(hash, item->getID(), 2);
	hashField(hash, item->getSubtype(), 2);
	hashField(hash, item->isSelected(), 1);
	return true;
}
} // namespace

namespace rme::procedural {
std::expected<bool, AssetFingerprintError> TerrainPreview::matchesAssets(std::stop_token stop) const noexcept {
	const auto current = fingerprintGenerationAssets(stop);
	if (!current) return std::unexpected(current.error());
	return buffer_ && *current == assetsFingerprint_;
}

bool TerrainPreview::matchesSource(const GenerationSnapshot& snapshot, const GroundPlan& plan) const noexcept {
	return wxIsMainThread() && buffer_ && sameConfig(config_, snapshot.input.config) &&
		mapGeneration_ == snapshot.input.mapGeneration && mapRevision_ == snapshot.input.mapRevision &&
		assetRevision_ == snapshot.input.assetRevision && planFingerprint_ == plan.fingerprint() &&
		sourceFingerprint_ == hashPreviewSource(snapshot,plan);
}

std::optional<uint64_t> TerrainPreview::currentFingerprint() const noexcept {
	if (!wxIsMainThread() || !buffer_ || buffer_->getTileCount() != changes_.size() || !g_version.IsVersionLoaded() ||
		g_version.GetAssetRevision() != assetRevision_) return std::nullopt;
	uint64_t hash = 14695981039346656037ULL;
	hashField(hash, 0x7072657669657731ULL);
	hashField(hash, generationAlgorithmVersion, 4);
	hashField(hash, planFingerprint_);
	hashField(hash, changes_.size());
	Position previous;
	bool first = true;
	for (const auto& position : changes_) {
		const auto& r = config_.region;
		if (position.z != r.z || position.x < r.x || position.y < r.y ||
			int64_t(position.x) >= int64_t(r.x) + r.width || int64_t(position.y) >= int64_t(r.y) + r.height ||
			(!first && (position.y < previous.y || (position.y == previous.y && position.x <= previous.x)))) return std::nullopt;
		first = false;
		previous = position;
		const auto* tile = buffer_->getTile(position);
		if (!tile || tile->getPosition() != position || tile->creature || tile->spawn || tile->house_id ||
			tile->mapflags || tile->extra || tile->items.size() > maxBorderItems) return std::nullopt;
		const auto* location = tile->getLocation();
		if (!location || location->getSpawnCount() || location->getWaypointCount() || location->getTownCount() ||
			(location->getHouseExits() && !location->getHouseExits()->empty())) return std::nullopt;
		hashField(hash, uint32_t(position.x), 4);
		hashField(hash, uint32_t(position.y), 4);
		hashField(hash, uint32_t(position.z), 4);
		hashField(hash, tile->statflags, 2);
		hashField(hash, tile->minimapColor, 1);
		// A tile kept clear beside the area's edge (surfaceKeepClear) carries only the outer border of its neighbour:
		// no ground, at least one item, and every item is checked below. Any other tile must have a valid ground.
		if (tile->ground) {
			if (!hashItem(hash, tile->ground.get(), true, decorIds_)) return std::nullopt;
		} else {
			if (tile->items.empty()) return std::nullopt;
			hashField(hash, 0, 2);
		}
		hashField(hash, tile->items.size(), 4);
		for (const auto& item : tile->items) if (!hashItem(hash, item.get(), false, decorIds_)) return std::nullopt;
	}
	return hash;
}
} // namespace rme::procedural

std::expected<void, rme::procedural::PreviewError> DrawOperations::clearTerrainPreviewBlockers(
    TerrainPreview& preview, std::span<const uint8_t> clear, std::stop_token stop) {
    if (!wxIsMainThread()) return std::unexpected(PreviewError::WrongThread);
    const auto& r = preview.config_.region;
    if (clear.size() != uint64_t(r.width) * r.height || !preview.currentFingerprint())
        return std::unexpected(PreviewError::InvalidPlan);
    for (size_t i = 0; i < clear.size(); ++i) {
        if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(PreviewError::Cancelled);
        if (!clear[i]) continue;
        auto* tile = preview.buffer_->getTile(Position(r.x + int(i % r.width), r.y + int(i / r.width), r.z));
        if (!tile || !tile->ground || tile->ground->isBlocking()) return std::unexpected(PreviewError::InvalidPlan);
        std::erase_if(tile->items, [&](const auto& item) {
            return item->isBlocking() && (item->isBorder() || std::ranges::binary_search(preview.decorIds_, item->getID()));
        });
        TileOperations::update(tile);
    }
    const auto fingerprint = preview.currentFingerprint();
    if (!fingerprint) return std::unexpected(PreviewError::InvalidItem);
    preview.fingerprint_ = *fingerprint;
    return {};
}

std::expected<rme::procedural::TerrainPreview, rme::procedural::PreviewError> DrawOperations::prepareTerrainPreview(
	const Map& map, const GenerationSnapshot& snapshot, const GroundPlan& plan, PreviewBudget budget, std::stop_token stop) {
	try {
		return prepareTerrainPreviewSteps(map, snapshot, plan, budget, stop).finish();
	} catch (const std::bad_alloc&) {
		return std::unexpected(PreviewError::AllocationFailed);
	}
}

PreparationSteps<std::expected<TerrainPreview, PreviewError>> DrawOperations::prepareTerrainPreviewSteps(
	const Map& map, const GenerationSnapshot& snapshot, const GroundPlan& plan, PreviewBudget budget, std::stop_token stop) {
	if (!wxIsMainThread()) co_return std::unexpected(PreviewError::WrongThread);
	if (stop.stop_requested()) co_return std::unexpected(PreviewError::Cancelled);
	if (!g_version.IsVersionLoaded() || !g_version.getLoadedVersion()) co_return std::unexpected(PreviewError::NoClient);
	try {
	const auto assets = fingerprintGenerationAssets(stop);
	if (!assets) co_return std::unexpected(assets.error() == AssetFingerprintError::Cancelled ? PreviewError::Cancelled : PreviewError::InvalidItem);
	if (!snapshotMatchesMap(map, snapshot)) co_return std::unexpected(PreviewError::StaleSnapshot);
	const auto& input = snapshot.input;
	const auto& r = input.config.region;
	const auto& c = snapshot.context;
	const auto layout = validateGeneration(input.config, input.extent, {plan.items.size(), UINT64_MAX},
		input.mask, input.protection, stop);
	if (!layout) co_return std::unexpected(layout.error() == GenerationError::Cancelled ? PreviewError::Cancelled : PreviewError::InvalidPlan);
	if (!sameConfig(input.config, plan.config) || input.mapGeneration != plan.mapGeneration ||
		input.mapRevision != plan.mapRevision || input.assetRevision != plan.assetRevision ||
		plan.client != g_version.GetCurrentVersionID() || plan.items.size() != layout->cellCount ||
		input.mask.size() != plan.items.size() || input.protection.size() != plan.items.size() ||
		input.extent.width != uint32_t(std::min(map.getWidth(), MAP_MAX_WIDTH + 1)) ||
		input.extent.height != uint32_t(std::min(map.getHeight(), MAP_MAX_HEIGHT + 1)) || input.extent.layers != MAP_LAYERS) {
		co_return std::unexpected(PreviewError::InvalidPlan);
	}
	const int left = std::max(0, r.x - 2), top = std::max(0, r.y - 2);
	const int64_t right = std::min<int64_t>(int64_t(MAP_MAX_WIDTH) + 1, int64_t(r.x) + r.width + 2);
	const int64_t bottom = std::min<int64_t>(int64_t(MAP_MAX_HEIGHT) + 1, int64_t(r.y) + r.height + 2);
	if (c != GenerationRegion{left, top, r.z, uint32_t(right-left), uint32_t(bottom-top)}) co_return std::unexpected(PreviewError::InvalidPlan);
	const auto contextIndex = [&](int x, int y) { return size_t(y-c.y) * c.width + size_t(x-c.x); };
	if (plan.profile.count != GroundProfile::groundRollCount && plan.profile.count != GroundProfile::surfaceCount &&
		plan.profile.count != GroundProfile::surfaceWithPathCount && plan.profile.count != GroundProfile::surfaceWithFloorCount) {
		co_return std::unexpected(PreviewError::InvalidPlan);
	}
	const bool cave = !plan.caveFloor.empty();
    const auto bridge = resolveBridgeBrush(plan.profile.bridgeBrush);
    if (!bridge) co_return std::unexpected(PreviewError::InvalidItem);
    const bool upper = !plan.plateauFloor.empty();
    const bool storeys = plan.config.surface.buildingStoreys;
    const bool mountains = hasPlateau(plan.profile);
    if (!validPlateauProfile(plan.profile, plan.config) || !validStoreyProfile(plan.profile, plan.config)
        || (upper && (cave || !(mountains || storeys) || plan.plateauFloor.size() != plan.items.size()))
        || ((upper || cave || !storeys) ? !plan.stairs.empty() : plan.stairs.size() != plan.items.size())) {
        co_return std::unexpected(PreviewError::InvalidPlan);
    }
    // Storeys (iteration 43): floor brush on z - 1 and the stairs items on both floors.
    const GroundBrush* storeyFloorBrush = nullptr;
    std::vector<uint16_t> stairIds, stairOpenings;
    if (storeys) {
        const auto resolved = resolveBuildingStairsBrush(plan.profile.buildingStairs);
        const auto* floor = g_brushes.getBrush(plan.profile.brushes[GroundProfile::surfaceWithFloorCount - 1]);
        storeyFloorBrush = floor ? floor->as<GroundBrush>() : nullptr;
        if (!resolved || !storeyFloorBrush) co_return std::unexpected(PreviewError::InvalidPlan);
        for (const auto& ids : resolved->items) {
            stairIds.push_back(ids.ramp);
            if (ids.tail) stairIds.push_back(ids.tail);
            stairOpenings.push_back(ids.opening);
        }
    }
    if (!validCaveProfile(plan.profile, plan.config) || (cave && (!hasCave(plan.profile) || plan.caveFloor.size() != plan.items.size())))
		co_return std::unexpected(PreviewError::InvalidPlan);
    // Dungeons (iteration 45): floor, walls and doors on the cave floor, stairs up there and the stairs down on the surface.
    const bool dungeons = plan.config.surface.buildingDungeons;
    if (!validDungeonProfile(plan.profile, plan.config)) co_return std::unexpected(PreviewError::InvalidPlan);
    const GroundBrush* dungeonFloorBrush = nullptr;
    std::vector<uint16_t> dungeonRamps, dungeonOpenings;
    if (dungeons && !upper) {
        const auto resolved = resolveDungeonBrushes(plan.profile);
        const auto* floor = g_brushes.getBrush(plan.profile.dungeonFloor);
        dungeonFloorBrush = floor ? floor->as<GroundBrush>() : nullptr;
        if (!resolved || !dungeonFloorBrush) co_return std::unexpected(PreviewError::InvalidPlan);
        for (const auto& ids : resolved->stairs.items) {
            dungeonRamps.push_back(ids.ramp);
            if (ids.tail) dungeonRamps.push_back(ids.tail);
        }
        dungeonOpenings.push_back(resolved->descent);
    }
	// Iteration 50: a wall-brush cave wall puts its pieces on cave ground over the wall band.
	const WallBrush* caveWallPieces = cave ? caveWallBrushOf(plan.profile) : nullptr;
	if (cave && (!plan.decor.empty() || !plan.fires.empty() ||
        (!plan.config.surface.caveDoorCorridors && !dungeons && ((!plan.walls.empty() && !caveWallPieces) ||
            std::ranges::any_of(plan.doors, [](uint8_t door) { return door != 0; }))))) co_return std::unexpected(PreviewError::InvalidPlan);
	const auto groundBrush = [](const std::string& name) {
		const auto* brush = g_brushes.getBrush(name);
		return brush ? brush->as<GroundBrush>() : nullptr;
	};
	// The cave floor's ground and wall come from their own profile fields, never from the surface brush slots.
	const GroundBrush* caveGroundBrush = cave ? groundBrush(plan.profile.caveGround) : nullptr;
	const GroundBrush* caveWallBrush = !cave ? nullptr : caveWallPieces ? caveGroundBrush : groundBrush(plan.profile.caveWall);
	if (cave && (!caveGroundBrush || !caveWallBrush)) co_return std::unexpected(PreviewError::InvalidPlan);
	std::array<const GroundBrush*, GroundProfile::surfaceWithFloorCount> brushes {};
	for (size_t i = 0; i < plan.profile.count; ++i) {
		if (plan.profile.brushes[i].empty() || plan.profile.brushes[i].size() > 256) co_return std::unexpected(PreviewError::InvalidPlan);
        brushes[i] = upper && mountains ? groundBrush(i == 1 ? plan.profile.plateauCliff : plan.profile.plateauGround)
            : cave ? caveGroundBrush
                   : groundBrush(plan.profile.brushes[i]);
		if (!brushes[i]) co_return std::unexpected(PreviewError::InvalidPlan);
	}
	// The learned grass mix is placed on Grass cells of the surface only.
	std::array<const GroundBrush*, GroundProfile::grassMixCount> grassMixBrushes {};
	for (size_t m = 0; m < GroundProfile::grassMixCount && !cave && !upper; ++m) {
		const auto& name = plan.profile.grassMix[m];
		if (name.empty()) continue;
		grassMixBrushes[m] = name.size() <= 256 ? groundBrush(name) : nullptr;
		if (!grassMixBrushes[m]) co_return std::unexpected(PreviewError::InvalidPlan);
	}
	const bool vegetation = plan.config.vegetation && !cave;
	if (vegetation != !plan.decor.empty() || (!cave && plan.profile.decorCount != (vegetation ? GroundProfile::decorationCount : 0)) ||
		(vegetation && (plan.decor.size() != plan.items.size() || plan.config.algorithm != GenerationAlgorithm::Surface))) {
		co_return std::unexpected(PreviewError::InvalidPlan);
	}
	std::vector<uint16_t> decorIds;
	// Required decor brushes first, then the optional ones: tree mix and the brushes of the grass mix patches.
	std::vector<const std::string*> decorNames;
	for (const auto& name : plan.profile.decor) decorNames.push_back(&name);
	for (const auto& name : plan.profile.treeMix) decorNames.push_back(&name);
	for (const auto& name : plan.profile.bushMix) decorNames.push_back(&name);
	for (const auto& names : plan.profile.grassMixDecor) for (const auto& name : names) decorNames.push_back(&name);
	for (const auto& name : plan.profile.overlay) decorNames.push_back(&name);
	decorNames.push_back(&plan.profile.waterTree);
	// Overlays come only with vegetation; a plan of a floor or stage without it carries none, whatever its profile says.
	const bool overlaid = std::ranges::any_of(plan.profile.overlay, [](const std::string& name) { return !name.empty(); });
	if (!plan.overlays.empty() && (!overlaid || !vegetation || plan.overlays.size() != plan.items.size()))
		co_return std::unexpected(PreviewError::InvalidPlan);
	for (size_t i = 0; vegetation && i < decorNames.size(); ++i) {
		const std::string& name = *decorNames[i];
		if (i >= GroundProfile::decorationCount && name.empty()) continue; // optional slots
		const DecorSource source = name.size() > 256 ? DecorSource{} : findDecorSource(name, 100000);
		if (!source) co_return std::unexpected(PreviewError::InvalidPlan);
		for (uint64_t roll = 1; roll <= (source.raw ? 1 : source.total); ++roll) {
			const uint16_t item = source.item(roll);
			if (!item) co_return std::unexpected(PreviewError::InvalidItem);
			decorIds.push_back(item);
		}
	}
	// Camp fire items (iteration 26): every single item of every variation of the campfire brush may be placed.
    if ((!cave && !upper && plan.profile.campfire.empty() != plan.fires.empty())
        || (!plan.fires.empty()
            && (plan.fires.size() != plan.items.size() || plan.profile.count != GroundProfile::surfaceWithFloorCount))) {
        co_return std::unexpected(PreviewError::InvalidPlan);
    }
    if (!cave && !upper && !plan.profile.campfire.empty()) {
        const auto* brush = plan.profile.campfire.size() <= 256 ? g_brushes.getBrush(plan.profile.campfire) : nullptr;
		const auto* fireBrush = brush ? brush->as<DoodadBrush>() : nullptr;
		if (!fireBrush) co_return std::unexpected(PreviewError::InvalidPlan);
		size_t found = 0;
		for (int variation = 0; variation <= fireBrush->getMaxVariation(); ++variation) {
			if (!fireBrush->hasSingleObjects(variation) || fireBrush->getSingleChance(variation) <= 0 || fireBrush->getSingleChance(variation) > 100000) continue;
			for (int roll = 1; roll <= fireBrush->getSingleChance(variation); ++roll) {
				const auto* item = fireBrush->getRandomSingleItem(variation, roll);
				if (!item || !item->getID()) co_return std::unexpected(PreviewError::InvalidItem);
				decorIds.push_back(item->getID());
				++found;
			}
		}
		if (!found) co_return std::unexpected(PreviewError::InvalidPlan);
    }
    std::vector<uint16_t> entranceIds, exitIds;
    uint16_t fallbackHole = 0, caveLanding = 0;
    if (!upper && hasCave(plan.profile)) {
        const auto resolved = resolveCaveBrushes(plan.profile, plan.config);
		if (!resolved) co_return std::unexpected(PreviewError::InvalidPlan);
		entranceIds = {resolved->entrance};
		fallbackHole = resolved->hole;
		if (cave) caveLanding = resolved->landing;
		if (fallbackHole) entranceIds.push_back(fallbackHole);
		for (const auto& ids : resolved->items) {
			exitIds.push_back(ids.ramp);
			if (ids.tail) exitIds.push_back(ids.tail);
		}
        entranceIds.insert(entranceIds.end(), dungeonOpenings.begin(), dungeonOpenings.end());
        exitIds.insert(exitIds.end(), dungeonRamps.begin(), dungeonRamps.end());
		if (cave ? (!plan.entrances.empty() || plan.exits.size() != plan.items.size()) :
			(!plan.exits.empty() || plan.entrances.size() != plan.items.size()))
			co_return std::unexpected(PreviewError::InvalidPlan);
		for (const auto* ids : {&entranceIds, &exitIds}) decorIds.insert(decorIds.end(), ids->begin(), ids->end());
    } else if (cave || !plan.entrances.empty() || !plan.exits.empty()) {
        co_return std::unexpected(PreviewError::InvalidPlan);
    }
    std::vector<uint16_t> plateauIds;
    if (hasPlateau(plan.profile)) {
        auto resolved = resolvePlateauBrushes(plan.profile, plan.config);
        if (!resolved) {
            co_return std::unexpected(PreviewError::InvalidPlan);
        }
        for (const auto& ids : resolved->items) {
            plateauIds.push_back(ids.opening);
            plateauIds.push_back(ids.ramp);
            if (ids.tail) {
                plateauIds.push_back(ids.tail);
            }
        }
        decorIds.insert(decorIds.end(), plateauIds.begin(), plateauIds.end());
        if (!plan.plateauRamps.empty() && (plan.plateauRamps.size() != plan.items.size())) {
            co_return std::unexpected(PreviewError::InvalidPlan);
        }
    } else if ((upper && !storeys) || !plan.plateauRamps.empty()) {
        co_return std::unexpected(PreviewError::InvalidPlan);
    }
    decorIds.insert(decorIds.end(), stairIds.begin(), stairIds.end());
    decorIds.insert(decorIds.end(), stairOpenings.begin(), stairOpenings.end());
    // Wall items: every piece of the profile's wall brush may end up on a tile once RME aligns them.
	std::vector<uint16_t> wallIds;
	const WallBrush* wallBrush = nullptr;
    const auto& wallName = cave ? plan.profile.caveDoor : plan.profile.wall;
    const bool caveDoors = cave && plan.config.surface.caveDoorCorridors;
    // Storey outlines on z - 1 carry the building's wall brush.
    const bool storeyWalls = upper && storeys && !plan.walls.empty();
    const bool dungeonWalls = cave && dungeons;
    // Collects every piece of a wall brush; the first brush is the plan's main one.
    const auto addWallBrush = [&](const std::string& name) {
        const auto* brush = name.size() <= 256 ? g_brushes.getBrush(name) : nullptr;
        const auto* walls = brush ? brush->as<WallBrush>() : nullptr;
        if (!walls) return false;
        if (!wallBrush) wallBrush = walls;
        const_cast<WallBrush*>(walls)->getRelatedItems(wallIds);
        return true;
    };
    if (((!upper && ((!cave && !wallName.empty()) || caveDoors)) || storeyWalls) && !addWallBrush(wallName))
        co_return std::unexpected(PreviewError::InvalidPlan);
    if (dungeonWalls && !addWallBrush(plan.profile.dungeonWall)) co_return std::unexpected(PreviewError::InvalidPlan);
    std::vector<uint16_t> caveWallIds;
    if (caveWallPieces) {
        if (!addWallBrush(plan.profile.caveWall)) co_return std::unexpected(PreviewError::InvalidPlan);
        const_cast<WallBrush*>(caveWallPieces)->getRelatedItems(caveWallIds);
        std::ranges::sort(caveWallIds);
    }
    if (wallBrush) {
		if (plan.walls.size() != plan.items.size() || plan.profile.count != GroundProfile::surfaceWithFloorCount) co_return std::unexpected(PreviewError::InvalidPlan);
		std::ranges::sort(wallIds);
		wallIds.erase(std::ranges::unique(wallIds).begin(), wallIds.end());
		decorIds.insert(decorIds.end(), wallIds.begin(), wallIds.end());
    } else if (!plan.walls.empty()) {
        co_return std::unexpected(PreviewError::InvalidPlan);
    }
    const auto doorType = caveDoors || dungeonWalls ? std::optional<DoorType>(WALL_DOOR_NORMAL) :
        cave || upper || plan.profile.door.empty() ? std::nullopt : doorTypeOf(plan.profile.door);
    if (caveDoors && (!wallBrush || plan.doors.size() != plan.items.size() || !resolveCaveDoorBrush(plan.profile.caveDoor)))
        co_return std::unexpected(PreviewError::InvalidPlan);
    if (dungeonWalls && plan.doors.size() != plan.items.size()) co_return std::unexpected(PreviewError::InvalidPlan);
    if ((!cave && !upper && plan.profile.door.empty() != plan.doors.empty())
        || (!cave && !upper && !plan.profile.door.empty() && (!doorType || !wallBrush || plan.doors.size() != plan.items.size()))) {
        co_return std::unexpected(PreviewError::InvalidPlan);
    }
    std::ranges::sort(decorIds);
	decorIds.erase(std::ranges::unique(decorIds).begin(), decorIds.end());
	uint64_t decorPlacements = 0;
	uint64_t placements = 0;
	// Surface leaves writable cells beside existing ground empty; every other cell must be placed.
    const auto keepClear = !cave && !upper && plan.config.algorithm == GenerationAlgorithm::Surface
        ? surfaceKeepClear(snapshot, profileNeedsEdgeClearance(plan.profile))
        : std::vector<uint8_t>();
    for (size_t i = 0; i < plan.items.size(); ++i) {
		if (i % 4096 == 0 && stop.stop_requested()) co_return std::unexpected(PreviewError::Cancelled);
		const int x = r.x + int(i % r.width), y = r.y + int(i / r.width);
        if (bool(input.protection[i]) != protectedCell(snapshot.cells[contextIndex(x, y)])
            || bool(plan.items[i])
                != (input.mask[i] && !input.protection[i] && (keepClear.empty() || !keepClear[i]) && (!cave || (plan.caveFloor[i] && plan.caveFloor[i] != 5))
                    && (!upper || plan.plateauFloor[i]))) {
            co_return std::unexpected(PreviewError::InvalidPlan);
        }
        if (cave && (plan.caveFloor[i] > (dungeons ? 5 : 2) || ((plan.caveFloor[i] == 1 || plan.caveFloor[i] == 3) &&
            (i % r.width < 2 || i / r.width < 2 || i % r.width + 2 >= r.width || i / r.width + 2 >= r.height)))) co_return std::unexpected(PreviewError::InvalidPlan);
        // Dungeon walls and doors stand exactly on the dungeon's wall cells.
        if (cave && plan.caveFloor[i] >= 3 && ((plan.caveFloor[i] >= 4) != (!plan.walls.empty() && plan.walls[i] != 0) ||
            (plan.caveFloor[i] == 5 && (plan.items[i] || plan.doors[i]))))
            co_return std::unexpected(PreviewError::InvalidPlan);
        if (upper) {
            const auto code = plan.plateauFloor[i];
            if (code > 5 || (code >= 4 && !storeys) || (code <= 3 && code && !mountains)) {
                co_return std::unexpected(PreviewError::InvalidPlan);
            }
            if (!plan.decor.empty() && plan.decor[i] && code != 1) {
                co_return std::unexpected(PreviewError::InvalidPlan);
            }
            if (!plan.walls.empty() && plan.walls[i] && code != 4) co_return std::unexpected(PreviewError::InvalidPlan);
            if (plan.items[i]) {
                auto def = g_item_definitions.get(plan.items[i]);
                const bool expected = code == 3 ? std::ranges::find(plateauIds, plan.items[i]) != plateauIds.end() :
                    code == 5 ? std::ranges::find(stairOpenings, plan.items[i]) != stairOpenings.end() :
                    code == 4 ? def.editorData().brush == storeyFloorBrush :
                    def.editorData().brush == (code == 2 ? brushes[1] : brushes[0]);
                if (!def || !expected) {
                    co_return std::unexpected(PreviewError::InvalidItem);
                }
            }
        }
        if (!plan.stairs.empty() && plan.stairs[i]) {
            if (!plan.items[i] || std::ranges::find(stairIds, plan.stairs[i]) == stairIds.end()) co_return std::unexpected(PreviewError::InvalidItem);
            ++decorPlacements;
        }
        if (!plan.plateauRamps.empty() && plan.plateauRamps[i]) {
            if (!plan.items[i] || std::ranges::find(plateauIds, plan.plateauRamps[i]) == plateauIds.end()) {
                co_return std::unexpected(PreviewError::InvalidItem);
            }
            ++decorPlacements;
        }
        if (cave) {
			bool adjacentFloor = false;
			const int cx = int(i % r.width), cy = int(i / r.width);
			for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
				if (cx + dx >= 0 && cy + dy >= 0 && cx + dx < int(r.width) && cy + dy < int(r.height))
					adjacentFloor |= plan.caveFloor[size_t(cy + dy) * r.width + size_t(cx + dx)] == 1;
			}
			if (plan.caveFloor[i] != 1 && (plan.caveFloor[i] == 2) != adjacentFloor) co_return std::unexpected(PreviewError::InvalidPlan);
			// A wall-brush cave wall: a piece on every placed wall band cell (a corridor door's wall brush may take a band
			// cell over), and none on a cave floor cell other than a corridor door.
			if (caveWallPieces) {
				const bool piece = !plan.walls.empty() && plan.walls[i] && std::ranges::binary_search(caveWallIds, plan.walls[i]);
				if ((plan.caveFloor[i] == 2 && plan.items[i] && !plan.walls[i]) ||
					(piece && plan.caveFloor[i] == 1 && (plan.doors.empty() || !plan.doors[i])))
					co_return std::unexpected(PreviewError::InvalidPlan);
			}
			// The walkable ground under a hole (iteration 48c) is a cave Floor cell without a ramp.
			const bool landing = caveLanding && plan.items[i] == caveLanding && plan.caveFloor[i] == 1 && !plan.exits[i];
			if (plan.items[i] && !landing) {
				const auto def = g_item_definitions.get(plan.items[i]);
				if (!def || def.editorData().brush != (plan.caveFloor[i] == 1 ? caveGroundBrush : plan.caveFloor[i] >= 3 ? dungeonFloorBrush : caveWallBrush))
					co_return std::unexpected(PreviewError::InvalidItem);
			}
		}
		for (const auto [layer, allowed] : {std::pair{std::span(plan.entrances), std::span(entranceIds)},
			std::pair{std::span(plan.exits), std::span(exitIds)}}) {
			if (layer.empty() || !layer[i]) continue;
			if (!plan.items[i] || std::ranges::find(allowed, layer[i]) == allowed.end()) co_return std::unexpected(PreviewError::InvalidItem);
			++decorPlacements;
		}
		if (plan.decor.empty() ? false : bool(plan.decor[i]) && !plan.items[i]) co_return std::unexpected(PreviewError::InvalidPlan);
		if (!plan.walls.empty() && plan.walls[i]) {
			// A dungeon ring corner (caveFloor 5) carries its wall piece without ground.
			if ((!plan.items[i] && !(cave && plan.caveFloor[i] == 5)) || !std::ranges::binary_search(wallIds, plan.walls[i])) co_return std::unexpected(PreviewError::InvalidPlan);
			const auto wallDefinition = g_item_definitions.get(plan.walls[i]);
			if (!wallDefinition || !wallDefinition.clientId() || wallDefinition.isMetaItem() || wallDefinition.isGroundTile()) co_return std::unexpected(PreviewError::InvalidItem);
		}
		if (!plan.doors.empty() && plan.doors[i] && (plan.walls.empty() || !plan.walls[i])) co_return std::unexpected(PreviewError::InvalidPlan);
		if (!plan.fires.empty() && plan.fires[i]) {
			if (!plan.items[i] || !std::ranges::binary_search(decorIds, plan.fires[i])) co_return std::unexpected(PreviewError::InvalidItem);
			const auto fireDefinition = g_item_definitions.get(plan.fires[i]);
			if (!fireDefinition || !fireDefinition.clientId() || fireDefinition.isMetaItem() || fireDefinition.isGroundTile()) co_return std::unexpected(PreviewError::InvalidItem);
			++decorPlacements;
		}
		if (!plan.overlays.empty() && plan.overlays[i]) {
			if (!plan.items[i] || !std::ranges::binary_search(decorIds, plan.overlays[i])) co_return std::unexpected(PreviewError::InvalidItem);
			const auto overlayDefinition = g_item_definitions.get(plan.overlays[i]);
			if (!overlayDefinition || !overlayDefinition.clientId() || overlayDefinition.isMetaItem() || overlayDefinition.isGroundTile() ||
				overlayDefinition.hasFlag(ItemFlag::Unpassable)) co_return std::unexpected(PreviewError::InvalidItem);
			++decorPlacements;
		}
		if (!plan.items[i]) continue;
		++placements;
		if (!plan.decor.empty() && plan.decor[i]) {
			if (!std::ranges::binary_search(decorIds, plan.decor[i])) co_return std::unexpected(PreviewError::InvalidItem);
			const auto decorDefinition = g_item_definitions.get(plan.decor[i]);
			if (!decorDefinition || !decorDefinition.clientId() || decorDefinition.isMetaItem() || decorDefinition.isGroundTile()) {
				co_return std::unexpected(PreviewError::InvalidItem);
			}
			++decorPlacements;
		}
		const auto definition = g_item_definitions.get(plan.items[i]);
		if (!definition || !definition.clientId() || definition.isMetaItem()) {
			co_return std::unexpected(PreviewError::InvalidItem);
		}
		const auto baseId = definition.attribute(ItemAttributeKey::BorderBaseGroundId);
		const auto ground = definition.isGroundTile() ? definition :
			(baseId > 0 && baseId <= UINT16_MAX ? g_item_definitions.get(uint16_t(baseId)) : ItemDefinitionView{});
		if (!ground || !ground.isGroundTile() || !ground.clientId() || ground.isMetaItem()) co_return std::unexpected(PreviewError::InvalidItem);
		bool found = false;
        if (!cave && !upper && bridge->horizontal &&
            (plan.items[i] == bridge->horizontal || plan.items[i] == bridge->vertical)) found = true;
		for (const auto* brush : std::span(brushes).first(plan.profile.count)) {
			if (ground.editorData().brush != brush) continue;
			for (const auto& variant : brush->getBorderItems()) if (variant.id == plan.items[i]) found = true;
		}
		for (const auto* mixBrush : grassMixBrushes) {
			if (!mixBrush || ground.editorData().brush != mixBrush) continue;
			for (const auto& variant : mixBrush->getBorderItems()) found |= variant.id == plan.items[i];
		}
		const bool groundHole = !cave && (plan.config.surface.caveEntranceStyle == CaveEntranceStyle::FloorHole ||
			(fallbackHole && plan.items[i] == fallbackHole) || std::ranges::find(dungeonOpenings, plan.items[i]) != dungeonOpenings.end()) &&
			!plan.entrances.empty() && plan.entrances[i] == plan.items[i] && definition.isGroundTile() &&
			definition.hasFlag(ItemFlag::FloorChangeDown) && std::ranges::find(entranceIds, plan.items[i]) != entranceIds.end();
        if (cave && plan.caveFloor[i] >= 3) for (const auto& variant : dungeonFloorBrush->getBorderItems()) found |= variant.id == plan.items[i];
        if (cave && caveLanding && plan.items[i] == caveLanding && plan.caveFloor[i] == 1 && !plan.exits[i]) found = true;
        if (cave && ground.editorData().brush == caveWallBrush) for (const auto& variant : caveWallBrush->getBorderItems()) found |= variant.id == plan.items[i];
		if (!upper && hasPlateau(plan.profile)) {
            const auto* cliff = g_brushes.getBrush(plan.profile.plateauCliff)->as<GroundBrush>();
            for (const auto& v : cliff->getBorderItems()) if (v.id == plan.items[i]) found = true;
        }
        if (upper && plan.plateauFloor[i] == 4) for (const auto& variant : storeyFloorBrush->getBorderItems()) found |= variant.id == plan.items[i];
        if (!found && !groundHole && !(upper && plan.plateauFloor[i] == 3 && std::ranges::find(plateauIds, plan.items[i]) != plateauIds.end()) &&
            !(upper && plan.plateauFloor[i] == 5 && std::ranges::find(stairOpenings, plan.items[i]) != stairOpenings.end()))
            co_return std::unexpected(PreviewError::InvalidItem);
	}
	if (placements > budget.maxChanges) co_return std::unexpected(PreviewError::Limit);
	const uint64_t count = snapshot.cells.size();
	uint64_t used = sizeof(TerrainPreview) + sizeof(BaseMap) + 8 * kSnapshotAllocationOverhead;
	uint64_t peak = used;
	const auto charge = [&](uint64_t n, uint64_t elementSize = 1) {
		if (used > budget.maxBytes || n > (budget.maxBytes-used) / elementSize) return false;
		used += n * elementSize;
		peak = std::max(peak, used);
		return true;
	};
	// Grid and vector growth allowances cover the entire clipped context. Tile
	// allowances deliberately include a plain ground even for absent halo tiles.
	// This is a conservative owned-buffer estimate, not an allocator/RSS bound.
	const uint64_t cells = uint64_t((right-1)/64-left/64+1) * uint64_t((bottom-1)/64-top/64+1);
	const uint64_t nodes = uint64_t((right-1)/4-left/4+1) * uint64_t((bottom-1)/4-top/4+1);
	const uint64_t emptyAllowance = sizeof(Tile) + 2*sizeof(TileLocation) + 3*kSnapshotAllocationOverhead + maximumSnapshotItemAllocation();
	if (!charge(count, sizeof(uint8_t)+sizeof(uint64_t)+emptyAllowance) || !charge(placements, sizeof(Position)) ||
		!charge(cells, sizeof(SpatialHashGrid::GridCell)+3*sizeof(SpatialHashGrid::CellEntry)+kSnapshotAllocationOverhead) ||
		!charge(nodes, sizeof(MapNode)+sizeof(Floor)+2*kSnapshotAllocationOverhead) || !charge(borderScratchBytes) ||
		!charge(maximumSnapshotItemAllocation()) ||
		!charge(decorPlacements, maximumSnapshotItemAllocation() + 3*sizeof(std::unique_ptr<Item>))) co_return std::unexpected(PreviewError::Limit);
	for (const auto& cell : snapshot.cells) {
		if (!cell.original) continue;
		const auto memory = estimateSnapshotTileMemory(*cell.original, budget.maxBytes-used, stop);
		if (!memory) co_return std::unexpected(memory.error() == SnapshotMemoryError::Cancelled ? PreviewError::Cancelled : PreviewError::Limit);
		if (!charge(memory->bytes)) co_return std::unexpected(PreviewError::Limit);
	}
		TerrainPreview result;
		result.config_ = input.config;
		result.mapGeneration_ = input.mapGeneration;
		result.mapRevision_ = input.mapRevision;
		result.assetRevision_ = input.assetRevision;
		result.planFingerprint_ = plan.fingerprint();
		result.sourceFingerprint_ = hashPreviewSource(snapshot,plan);
		result.assetsFingerprint_ = *assets;
		result.decorIds_ = std::move(decorIds);
		result.buffer_ = std::make_unique<BaseMap>();
		result.buffer_->getChangeTracker().markAllDirty();
		result.changes_.reserve(size_t(placements));
		std::vector<uint8_t> affected(size_t(count), 0);
		std::vector<uint64_t> tileBytes(size_t(count), emptyAllowance);
		using Clock = std::chrono::steady_clock;
		constexpr auto slice = std::chrono::milliseconds(8);
		auto deadline = Clock::now() + slice;
		for (uint32_t y = 0; y < c.height; ++y) {
			if (Clock::now() >= deadline) {
				co_await std::suspend_always{};
				deadline = Clock::now() + slice;
			}
			if (stop.stop_requested()) co_return std::unexpected(PreviewError::Cancelled);
			for (uint32_t x = 0; x < c.width; ++x) {
				const size_t i = size_t(y)*c.width+x;
				const auto* original = snapshot.cells[i].original.get();
				if (!original) continue;
				const auto memory = estimateSnapshotTileMemory(*original, UINT64_MAX);
				tileBytes[i] += memory->bytes;
				auto copy = TileOperations::deepCopy(original, *result.buffer_);
				if (!snapshotTilesEqual(original, copy.get())) co_return std::unexpected(PreviewError::InvalidPlan);
				(void)result.buffer_->setTile(std::move(copy));
			}
		}
		for (size_t i = 0; i < plan.items.size(); ++i) {
			if (i % 256 == 0 && Clock::now() >= deadline) {
				co_await std::suspend_always{};
				deadline = Clock::now() + slice;
			}
			if (i % 4096 == 0 && stop.stop_requested()) co_return std::unexpected(PreviewError::Cancelled);
			if (!plan.items[i]) continue;
			const int x = r.x + int(i % r.width), y = r.y + int(i / r.width);
			auto* tile = result.buffer_->getOrCreateTile(Position(x,y,r.z));
			TerrainPlacement::placeBrushItem(*tile, plan.items[i]);
			for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
				if (x+dx >= c.x && y+dy >= c.y && x+dx < right && y+dy < bottom) affected[contextIndex(x+dx,y+dy)] = 1;
			}
		}
		for (uint32_t y = 0; y < c.height; ++y) {
			if (Clock::now() >= deadline) {
				co_await std::suspend_always{};
				deadline = Clock::now() + slice;
			}
			if (stop.stop_requested()) co_return std::unexpected(PreviewError::Cancelled);
			for (uint32_t x = 0; x < c.width; ++x) {
				const size_t i = size_t(y)*c.width+x;
				if (!affected[i]) continue;
				auto* tile = result.buffer_->getOrCreateTile(Position(c.x+int(x),c.y+int(y),c.z));
				// At most two clusters per neighbour, four directions per cluster,
				// two components per diagonal. Specific cases replace/delete only.
				const uint64_t before = used;
				if (!charge(maxBorderItems, maximumSnapshotItemAllocation()) ||
					!charge(uint64_t(tile->items.size())+maxBorderItems, 3*sizeof(std::unique_ptr<Item>)) ||
					!charge(borderScratchBytes)) co_return std::unexpected(PreviewError::Limit);
				GroundBorderCalculator::calculate(result.buffer_.get(), tile, true);
				const auto memory = estimateSnapshotTileMemory(*tile, tileBytes[i]+used-before, stop);
				if (!memory) co_return std::unexpected(memory.error() == SnapshotMemoryError::Cancelled ? PreviewError::Cancelled : PreviewError::Limit);
				used = before;
				// Erasing borders can retain vector capacity. Never reclaim an
				// estimate merely because the logical stack became smaller.
				if (memory->bytes > tileBytes[i]) {
					if (!charge(memory->bytes-tileBytes[i])) co_return std::unexpected(PreviewError::Limit);
					tileBytes[i] = memory->bytes;
				}
			}
		}
		// Vegetation goes on after the borders: the border calculator never sees it.
		for (size_t i = 0; i < plan.decor.size(); ++i) {
			if (!plan.decor[i]) continue;
			auto* tile = result.buffer_->getTile(Position(r.x + int(i % r.width), r.y + int(i / r.width), r.z));
			if (!tile) co_return std::unexpected(PreviewError::InvalidPlan);
			tile->addItem(Item::Create(plan.decor[i]));
		}
		// Overlays go over the vegetation of their cell.
		for (size_t i = 0; i < plan.overlays.size(); ++i) {
			if (!plan.overlays[i]) continue;
			auto* tile = result.buffer_->getTile(Position(r.x + int(i % r.width), r.y + int(i / r.width), r.z));
			if (!tile) co_return std::unexpected(PreviewError::InvalidPlan);
			tile->addItem(Item::Create(plan.overlays[i]));
		}
		for (size_t i = 0; i < plan.fires.size(); ++i) {
			if (!plan.fires[i]) continue;
			auto* tile = result.buffer_->getTile(Position(r.x + int(i % r.width), r.y + int(i / r.width), r.z));
			if (!tile) co_return std::unexpected(PreviewError::InvalidPlan);
			tile->addItem(Item::Create(plan.fires[i]));
		}
        for (const auto layer : {std::span(plan.entrances), std::span(plan.exits), std::span(plan.plateauRamps), std::span(plan.stairs)}) {
            for (size_t i = 0; i < layer.size(); ++i) {
				if (!layer[i]) continue;
				auto* tile = result.buffer_->getTile(Position(r.x + int(i % r.width), r.y + int(i / r.width), r.z));
				if (!tile || !tile->ground) co_return std::unexpected(PreviewError::InvalidPlan);
				if (g_item_definitions.get(layer[i]).isGroundTile()) {
					if (plan.items[i] != layer[i] || (plan.entrances.empty() || plan.entrances[i] != layer[i])) co_return std::unexpected(PreviewError::InvalidPlan);
					continue; // already placed as ground before the normal border pass
				}
				tile->addItem(Item::Create(layer[i]));
			}
        }
        // Walls follow the vegetation: the pieces are placed with the brush's first item and RME's own wall logic then
		// picks the right piece (straight, corner) from the neighbouring walls.
		for (size_t i = 0; i < plan.walls.size(); ++i) {
			if (!plan.walls[i]) continue;
			const Position position(r.x + int(i % r.width), r.y + int(i / r.width), r.z);
			// Only a dungeon ring corner has no ground tile yet.
			auto* tile = cave && plan.caveFloor[i] == 5 ? result.buffer_->getOrCreateTile(position) : result.buffer_->getTile(position);
			if (!tile) co_return std::unexpected(PreviewError::InvalidPlan);
			tile->addItem(Item::Create(plan.walls[i]));
		}
		for (size_t i = 0; i < plan.walls.size(); ++i) {
			if (!plan.walls[i]) continue;
			auto* tile = result.buffer_->getTile(Position(r.x + int(i % r.width), r.y + int(i / r.width), r.z));
			WallBrush::doWalls(result.buffer_.get(), tile);
		}
		// Doors replace the aligned wall piece with RME's own door logic; a wall brush without this kind of door for the
		// piece that ended up there refuses the preview instead of leaving a closed wall.
		if (doorType) {
			DoorBrush doorBrush(*doorType);
			for (size_t i = 0; i < plan.doors.size(); ++i) {
				if (!plan.doors[i]) continue;
				const Position position(r.x + int(i % r.width), r.y + int(i / r.width), r.z);
				auto* tile = result.buffer_->getTile(position);
				if (!tile || !doorBrush.canDraw(result.buffer_.get(), position)) co_return std::unexpected(PreviewError::InvalidItem);
				doorBrush.draw(result.buffer_.get(), tile, nullptr);
                if (cave) {
                    // DoorBrush follows the editor's lock preference. Generated cave and dungeon
                    // doors always use the client's closed, unlocked normal variant of their wall brush.
                    const auto resolved = resolveCaveDoorBrush(plan.caveFloor[i] >= 3 ? plan.profile.dungeonWall : plan.profile.caveDoor);
                    auto* door = tile->getWall();
                    if (!resolved || !door || !door->isBrushDoor()) co_return std::unexpected(PreviewError::InvalidItem);
                    const auto alignment = door->getWallAlignment();
                    if (alignment != WALL_HORIZONTAL && alignment != WALL_VERTICAL) co_return std::unexpected(PreviewError::InvalidItem);
                    door->setID(alignment == WALL_HORIZONTAL ? resolved->horizontal : resolved->vertical);
                }
			}
		}
		// Classify actual values, ignoring only derived caches. Even a border on
		// an unselected neighbour rejects the whole candidate; never clip it away.
		for (uint32_t y = 0; y < c.height; ++y) {
			if (Clock::now() >= deadline) {
				co_await std::suspend_always{};
				deadline = Clock::now() + slice;
			}
			if (stop.stop_requested()) co_return std::unexpected(PreviewError::Cancelled);
			for (uint32_t x = 0; x < c.width; ++x) {
				const Position position(c.x+int(x),c.y+int(y),c.z);
				const auto* original = snapshot.cells[size_t(y)*c.width+x].original.get();
				const auto* tile = result.buffer_->getTile(position);
				const bool equal = snapshotTileContentsEqual(original,tile) || (!original && emptyContent(tile));
				if (equal) { (void)result.buffer_->setTile(position,nullptr); continue; }
				if (position.x < r.x || position.y < r.y || int64_t(position.x) >= int64_t(r.x)+r.width ||
					int64_t(position.y) >= int64_t(r.y)+r.height) co_return std::unexpected(PreviewError::OutOfMask);
				const size_t index = size_t(position.y-r.y)*r.width+size_t(position.x-r.x);
				if (cave) {
					if (bool(tile && tile->ground) != bool(plan.items[index])) co_return std::unexpected(PreviewError::InvalidPlan);
					if (!plan.caveFloor[index]) {
						bool adjacent = false;
						const int cx = int(index % r.width), cy = int(index / r.width);
						for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
							if (cx + dx >= 0 && cy + dy >= 0 && cx + dx < int(r.width) && cy + dy < int(r.height))
								adjacent |= bool(plan.items[size_t(cy + dy) * r.width + size_t(cx + dx)]);
						}
						if (!adjacent || !tile || std::ranges::any_of(tile->items, [](const auto& item) { return !item->isBorder(); }))
							co_return std::unexpected(PreviewError::InvalidPlan);
					}
				}
				if (!input.mask[index] || input.protection[index]) co_return std::unexpected(PreviewError::OutOfMask);
				if (result.changes_.size() >= budget.maxChanges) co_return std::unexpected(PreviewError::Limit);
				result.changes_.push_back(position);
			}
		}
		const auto fingerprint = result.currentFingerprint();
		if (!fingerprint) co_return std::unexpected(PreviewError::InvalidItem);
		result.fingerprint_ = *fingerprint;
		result.estimatedPeakBytes_ = peak;
		if (stop.stop_requested()) co_return std::unexpected(PreviewError::Cancelled);
		co_return result;
	} catch (const std::bad_alloc&) {
		co_return std::unexpected(PreviewError::AllocationFailed);
	}
}
