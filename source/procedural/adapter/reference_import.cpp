// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/reference_import.h"

#include "app/client_version.h"
#include "app/managers/version_manager.h"
#include "brushes/brush.h"
#include "brushes/ground/ground_brush.h"
#include "game/item.h"
#include "io/iomap_otbm.h"
#include "map/map.h"
#include "map/tile.h"

#include <algorithm>
#include <climits>
#include <unordered_map>
#include <fstream>
#include <vector>

namespace rme::procedural {

namespace {
constexpr size_t hashChunkBytes = 1 << 20;

void countItem(const Item& item, bool ground, ReferenceImportSummary& summary) {
	++summary.itemCount;
	if (item.isInvalidOTBMItem() || !item.typeExists()) {
		++(ground ? summary.unknownGround : summary.unknownItems);
		++summary.unknownIds[item.getID()];
	}
}
}

const char* referenceImportErrorName(ReferenceImportError value) noexcept {
	switch (value) {
		case ReferenceImportError::NoClient: return "no_client";
		case ReferenceImportError::FileMissing: return "file_missing";
		case ReferenceImportError::FileTooLarge: return "file_too_large";
		case ReferenceImportError::HeaderUnreadable: return "header_unreadable";
		case ReferenceImportError::LoadFailed: return "load_failed";
		case ReferenceImportError::BudgetExceeded: return "budget_exceeded";
		case ReferenceImportError::NormalizeFailed: return "normalize_failed";
	}
	return "unknown";
}

std::optional<ClientItemVersion> currentClientItemVersion() {
	if (!g_version.IsVersionLoaded()) return std::nullopt;
	const ClientVersion& client = g_version.GetCurrentVersion();
	const OtbVersion otb = client.getOTBVersion();
	ClientItemVersion result;
	result.itemsMajor = static_cast<uint32_t>(otb.format_version);
	result.itemsMinor = static_cast<uint32_t>(otb.id);
	result.otbmSupported = true; // refined per file in readReferenceHeader's caller
	return result;
}

std::expected<ReferenceHeader, ReferenceImportError> readReferenceHeader(
	const std::filesystem::path& file, const ReferenceImportLimits& limits) {
	std::error_code error;
	if (!std::filesystem::is_regular_file(file, error)) return std::unexpected(ReferenceImportError::FileMissing);
	const uint64_t bytes = std::filesystem::file_size(file, error);
	if (error) return std::unexpected(ReferenceImportError::FileMissing);
	if (bytes > limits.maxFileBytes) return std::unexpected(ReferenceImportError::FileTooLarge);

	OTBMStartupPeekResult info;
	if (!IOMapOTBM::peekStartupInfo(FileName(wxString(file.wstring())), info) || info.has_error) {
		return std::unexpected(ReferenceImportError::HeaderUnreadable);
	}
	ReferenceHeader header;
	header.otbmVersion = static_cast<uint32_t>(info.otbm_version);
	header.width = info.width;
	header.height = info.height;
	header.itemsMajor = info.items_major_version;
	header.itemsMinor = info.items_minor_version;
	header.compressed = info.is_compressed;
	header.fileBytes = bytes;
	return header;
}

std::expected<std::string, ReferenceImportError> hashReferenceFile(const std::filesystem::path& file) {
	std::ifstream input(file, std::ios::binary);
	if (!input) return std::unexpected(ReferenceImportError::FileMissing);
	std::vector<char> chunk(hashChunkBytes);
	Sha256 hash;
	while (input) {
		input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
		const auto got = static_cast<size_t>(input.gcount());
		if (got > 0) hash.update({reinterpret_cast<const std::byte*>(chunk.data()), got});
	}
	if (!input.eof()) return std::unexpected(ReferenceImportError::FileMissing);
	return Sha256::hex(hash.finish());
}

namespace {
// Header, hash and in-memory load shared by import and normalization; counting is left to callers.
std::expected<ReferenceImportSummary, ReferenceImportError> loadReference(
	Map& map, const std::filesystem::path& file, const ReferenceImportLimits& limits) {
	auto client = currentClientItemVersion();
	if (!client) return std::unexpected(ReferenceImportError::NoClient);
	auto header = readReferenceHeader(file, limits);
	if (!header) return std::unexpected(header.error());

	// OTBM display version 1..5 maps to MapVersionID 0..4.
	const auto& supported = g_version.GetCurrentVersion().getMapVersionsSupported();
	const auto mapVersion = static_cast<MapVersionID>(static_cast<int>(header->otbmVersion) - 1);
	client->otbmSupported = std::ranges::find(supported, mapVersion) != supported.end();

	ReferenceImportSummary summary;
	summary.header = *header;
	summary.compatibility = checkCompatibility(*header, &*client);
	summary.clientId = std::string(g_version.GetCurrentVersionIDView());
	if (auto digest = hashReferenceFile(file)) summary.sha256 = std::move(*digest);
	else return std::unexpected(digest.error());

	// Map::open is protected and also records the path as the map's save target; the
	// loader alone keeps this import free of anything that could later be saved.
	IOMapOTBM loader(map.getVersion());
	if (!loader.loadMap(map, FileName(wxString(file.wstring())))) return std::unexpected(ReferenceImportError::LoadFailed);
	summary.loaderWarnings = loader.getWarnings().size();
	return summary;
}

void countTile(const Tile& tile, ReferenceImportSummary& summary) {
	++summary.tileCount;
	const int floor = tile.getPosition().z;
	if (floor >= 0 && floor < static_cast<int>(summary.tilesPerFloor.size())) ++summary.tilesPerFloor[static_cast<size_t>(floor)];
	if (tile.ground) countItem(*tile.ground, true, summary);
	for (const auto& item : tile.items) countItem(*item, false, summary);
}

uint32_t itemTraits(const Item& item, bool ground) {
	if (item.isInvalidOTBMItem() || !item.typeExists()) return bit(Trait::Unknown);
	const auto definition = item.getDefinition();
	uint32_t traits = 0;
	if (item.isBlocking()) traits |= bit(Trait::Blocking);
	if (definition.hasFlag(ItemFlag::BlockMissiles)) traits |= bit(Trait::BlocksMissiles);
	if (definition.hasFlag(ItemFlag::BlockPathfinder)) traits |= bit(Trait::BlocksPath);
	if (definition.hasFlag(ItemFlag::FloorChange) || definition.hasFlag(ItemFlag::FloorChangeDown)
		|| definition.hasFlag(ItemFlag::FloorChangeNorth) || definition.hasFlag(ItemFlag::FloorChangeSouth)
		|| definition.hasFlag(ItemFlag::FloorChangeEast) || definition.hasFlag(ItemFlag::FloorChangeWest)) {
		traits |= bit(Trait::FloorChange);
	}
	if (item.isTeleport()) traits |= bit(Trait::Teleport);
	if (item.isMagicField()) traits |= bit(Trait::MagicField);
	if (item.asContainer() != nullptr) traits |= bit(Trait::Container);
	if (ground) return traits;
	const bool wall = item.isWall(), door = item.isDoor(), table = item.isTable(), carpet = item.isCarpet(), border = item.isBorder();
	if (wall) traits |= bit(Trait::Wall);
	if (door) traits |= bit(Trait::Door);
	if (table) traits |= bit(Trait::Table);
	if (carpet) traits |= bit(Trait::Carpet);
	if (border) traits |= bit(Trait::Border);
	if (item.getDoodadBrush() != nullptr && !wall && !door && !table && !carpet && !border) traits |= bit(Trait::Doodad);
	if (item.isMoveable()) traits |= bit(Trait::LooseItem);
	return traits;
}
}

std::expected<ReferenceImportSummary, ReferenceImportError> importReference(
	const std::filesystem::path& file, const ReferenceImportLimits& limits) {
	Map map;
	auto summary = loadReference(map, file, limits);
	if (!summary) return summary;
	for (auto& location : map.tiles()) {
		const Tile* tile = location.get();
		if (tile != nullptr && !tile->empty()) countTile(*tile, *summary);
	}
	return summary;
}

std::expected<NormalizedReference, ReferenceImportError> normalizeReference(
	const std::filesystem::path& file, const ReferenceImportLimits& limits, const SemanticBudget& budget) {
	Map map;
	auto loaded = loadReference(map, file, limits);
	if (!loaded) return std::unexpected(loaded.error());
	NormalizedReference result;
	result.summary = std::move(*loaded);

	// Pass 1: per-floor boxes, counts, and the set of ground brushes (by pointer).
	struct Box { int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN; bool used = false; };
	std::array<Box, 16> boxes {};
	std::unordered_map<const Brush*, std::string> brushNames;
	for (auto& location : map.tiles()) {
		const Tile* tile = location.get();
		if (tile == nullptr || tile->empty()) continue;
		countTile(*tile, result.summary);
		const Position position = tile->getPosition();
		if (position.z < 0 || position.z > 15) return std::unexpected(ReferenceImportError::NormalizeFailed);
		Box& box = boxes[static_cast<size_t>(position.z)];
		box.used = true;
		box.x0 = std::min(box.x0, position.x); box.x1 = std::max(box.x1, position.x);
		box.y0 = std::min(box.y0, position.y); box.y1 = std::max(box.y1, position.y);
		if (tile->ground && !tile->ground->isInvalidOTBMItem() && tile->ground->typeExists()) {
			if (const Brush* brush = tile->ground->getGroundBrush()) brushNames.try_emplace(brush, brush->getName());
		}
	}
	std::vector<FloorBounds> bounds;
	for (size_t z = 0; z < boxes.size(); ++z) {
		const Box& box = boxes[z];
		if (box.used) bounds.push_back({static_cast<int>(z), box.x0, box.y0, box.x1 - box.x0 + 1, box.y1 - box.y0 + 1});
	}
	auto created = SemanticMap::create(bounds, budget);
	if (!created) {
		return std::unexpected(created.error() == SemanticError::CellLimit ? ReferenceImportError::BudgetExceeded
			: ReferenceImportError::NormalizeFailed);
	}
	result.map = std::move(*created);

	// Keys are interned alphabetically so the table does not depend on tile iteration order.
	std::vector<std::string> names;
	for (const auto& entry : brushNames) names.push_back(entry.second);
	std::ranges::sort(names);
	names.erase(std::ranges::unique(names).begin(), names.end());
	for (const auto& name : names) {
		if (!result.map.internGroundKey(name)) return std::unexpected(ReferenceImportError::NormalizeFailed);
	}
	std::unordered_map<const Brush*, uint16_t> keys;
	for (const auto& [brush, name] : brushNames) keys.emplace(brush, result.map.internGroundKey(name).value());

	// Pass 2: fill cells.
	for (auto& location : map.tiles()) {
		const Tile* tile = location.get();
		if (tile == nullptr || tile->empty()) continue;
		uint32_t traits = 0;
		uint16_t key = noGroundKey;
		if (tile->ground) {
			traits |= bit(Trait::HasGround) | itemTraits(*tile->ground, true);
			if (const Brush* brush = tile->ground->isInvalidOTBMItem() ? nullptr : tile->ground->getGroundBrush()) key = keys.at(brush);
		}
		for (const auto& item : tile->items) traits |= itemTraits(*item, false);
		const Position position = tile->getPosition();
		if (!result.map.set(position.x, position.y, position.z, key, traits)) return std::unexpected(ReferenceImportError::NormalizeFailed);
	}
	result.semantic = summarize(result.map);
	return result;
}

} // namespace rme::procedural
