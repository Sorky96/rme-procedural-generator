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
#include "item_definitions/core/item_definition_store.h"
#include "map/map.h"
#include "map/tile_operations.h"
#include "procedural/adapter/generation_snapshot.h"
#include "procedural/adapter/ground_resolver.h"
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
	return !tile || (!tile->ground && tile->items.empty() && !tile->creature && !tile->spawn &&
		!tile->house_id && !tile->mapflags && !tile->extra &&
		!(tile->statflags & ~uint16_t(TILESTATE_BLOCKING)));
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
			if (c.surface.pathLength) hashField(hash, c.surface.pathLength, 4);
			if (c.surface.buildingDensity) hashField(hash, c.surface.buildingDensity, 4);
			if (c.surface.fordLength) hashField(hash, c.surface.fordLength, 4);
			if (c.surface.rockPasses) hashField(hash, c.surface.rockPasses, 4);
			if (c.surface.shoreSmoothing) hashField(hash, c.surface.shoreSmoothing, 4);
			if (c.surface.shoreRadius) hashField(hash, c.surface.shoreRadius, 4);
			if (c.surface.repairReach) hashField(hash, c.surface.repairReach, 4);
			// Appended only when it differs from the built-in rules, so other plans keep their original fingerprint.
			if (c.vegetation && !(c.decor == DecorTuning {})) {
				for (const auto value : {c.decor.treeScalePermille, c.decor.waterTreePermille, c.decor.bushPermille,
					c.decor.waterBushPermille, c.decor.dirtStonePermille, c.decor.rockStonePermille}) hashField(hash, value, 4);
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
	hashField(hash, plan.items.size());
	for (const auto id : plan.items) hashField(hash, id, 2);
	hashField(hash, plan.decor.size());
	for (const auto id : plan.decor) hashField(hash, id, 2);
	if (!plan.walls.empty()) {
		hashField(hash, plan.walls.size());
		for (const auto id : plan.walls) hashField(hash, id, 2);
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
	std::array<const GroundBrush*, GroundProfile::surfaceWithFloorCount> brushes {};
	for (size_t i = 0; i < plan.profile.count; ++i) {
		if (plan.profile.brushes[i].empty() || plan.profile.brushes[i].size() > 256) co_return std::unexpected(PreviewError::InvalidPlan);
		const auto* brush = g_brushes.getBrush(plan.profile.brushes[i]);
		brushes[i] = brush ? brush->as<GroundBrush>() : nullptr;
		if (!brushes[i]) co_return std::unexpected(PreviewError::InvalidPlan);
	}
	const bool vegetation = plan.config.vegetation;
	if (vegetation != !plan.decor.empty() || plan.profile.decorCount != (vegetation ? GroundProfile::decorationCount : 0) ||
		(vegetation && (plan.decor.size() != plan.items.size() || plan.config.algorithm != GenerationAlgorithm::Surface))) {
		co_return std::unexpected(PreviewError::InvalidPlan);
	}
	std::vector<uint16_t> decorIds;
	for (size_t i = 0; vegetation && i < GroundProfile::decorationCount; ++i) {
		const auto* brush = g_brushes.getBrush(plan.profile.decor[i]);
		const auto* doodad = brush ? brush->as<DoodadBrush>() : nullptr;
		if (plan.profile.decor[i].empty() || plan.profile.decor[i].size() > 256 || !doodad || !doodad->hasSingleObjects(0) ||
			doodad->getSingleChance(0) <= 0 || doodad->getSingleChance(0) > 100000) co_return std::unexpected(PreviewError::InvalidPlan);
		for (int roll = 1; roll <= doodad->getSingleChance(0); ++roll) {
			const auto* item = doodad->getRandomSingleItem(0, roll);
			if (!item || !item->getID()) co_return std::unexpected(PreviewError::InvalidItem);
			decorIds.push_back(item->getID());
		}
	}
	// Wall items: every piece of the profile's wall brush may end up on a tile once RME aligns them.
	std::vector<uint16_t> wallIds;
	const WallBrush* wallBrush = nullptr;
	if (!plan.profile.wall.empty()) {
		const auto* brush = plan.profile.wall.size() <= 256 ? g_brushes.getBrush(plan.profile.wall) : nullptr;
		wallBrush = brush ? brush->as<WallBrush>() : nullptr;
		if (!wallBrush || plan.walls.size() != plan.items.size() || plan.profile.count != GroundProfile::surfaceWithFloorCount) co_return std::unexpected(PreviewError::InvalidPlan);
		const_cast<WallBrush*>(wallBrush)->getRelatedItems(wallIds);
		std::ranges::sort(wallIds);
		wallIds.erase(std::ranges::unique(wallIds).begin(), wallIds.end());
		decorIds.insert(decorIds.end(), wallIds.begin(), wallIds.end());
	} else if (!plan.walls.empty()) {
		co_return std::unexpected(PreviewError::InvalidPlan);
	}
	const auto doorType = plan.profile.door.empty() ? std::nullopt : doorTypeOf(plan.profile.door);
	if (plan.profile.door.empty() != plan.doors.empty() || (!plan.profile.door.empty() && (!doorType || !wallBrush || plan.doors.size() != plan.items.size()))) {
		co_return std::unexpected(PreviewError::InvalidPlan);
	}
	std::ranges::sort(decorIds);
	decorIds.erase(std::ranges::unique(decorIds).begin(), decorIds.end());
	uint64_t decorPlacements = 0;
	uint64_t placements = 0;
	// Surface leaves writable cells beside existing ground empty; every other cell must be placed.
	const auto keepClear = plan.config.algorithm == GenerationAlgorithm::Surface ? surfaceKeepClear(snapshot, profileNeedsEdgeClearance(plan.profile)) : std::vector<uint8_t>();
	for (size_t i = 0; i < plan.items.size(); ++i) {
		if (i % 4096 == 0 && stop.stop_requested()) co_return std::unexpected(PreviewError::Cancelled);
		const int x = r.x + int(i % r.width), y = r.y + int(i / r.width);
		if (bool(input.protection[i]) != protectedCell(snapshot.cells[contextIndex(x,y)]) ||
			bool(plan.items[i]) != (input.mask[i] && !input.protection[i] && (keepClear.empty() || !keepClear[i]))) co_return std::unexpected(PreviewError::InvalidPlan);
		if (plan.decor.empty() ? false : bool(plan.decor[i]) && !plan.items[i]) co_return std::unexpected(PreviewError::InvalidPlan);
		if (!plan.walls.empty() && plan.walls[i]) {
			if (!plan.items[i] || !std::ranges::binary_search(wallIds, plan.walls[i])) co_return std::unexpected(PreviewError::InvalidPlan);
			const auto wallDefinition = g_item_definitions.get(plan.walls[i]);
			if (!wallDefinition || !wallDefinition.clientId() || wallDefinition.isMetaItem() || wallDefinition.isGroundTile()) co_return std::unexpected(PreviewError::InvalidItem);
		}
		if (!plan.doors.empty() && plan.doors[i] && (plan.walls.empty() || !plan.walls[i])) co_return std::unexpected(PreviewError::InvalidPlan);
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
		for (const auto* brush : std::span(brushes).first(plan.profile.count)) {
			if (ground.editorData().brush != brush) continue;
			for (const auto& variant : brush->getBorderItems()) if (variant.id == plan.items[i]) found = true;
		}
		if (!found) co_return std::unexpected(PreviewError::InvalidItem);
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
		// Walls follow the vegetation: the pieces are placed with the brush's first item and RME's own wall logic then
		// picks the right piece (straight, corner) from the neighbouring walls.
		for (size_t i = 0; i < plan.walls.size(); ++i) {
			if (!plan.walls[i]) continue;
			auto* tile = result.buffer_->getTile(Position(r.x + int(i % r.width), r.y + int(i / r.width), r.z));
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
