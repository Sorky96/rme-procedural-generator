// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/reference_import.h"

#include "app/client_version.h"
#include "app/managers/version_manager.h"
#include "game/item.h"
#include "io/iomap_otbm.h"
#include "map/map.h"
#include "map/tile.h"

#include <algorithm>
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

std::expected<ReferenceImportSummary, ReferenceImportError> importReference(
	const std::filesystem::path& file, const ReferenceImportLimits& limits) {
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

	Map map;
	// Map::open is protected and also records the path as the map's save target; the
	// loader alone keeps this import free of anything that could later be saved.
	IOMapOTBM loader(map.getVersion());
	if (!loader.loadMap(map, FileName(wxString(file.wstring())))) return std::unexpected(ReferenceImportError::LoadFailed);
	summary.loaderWarnings = loader.getWarnings().size();
	for (auto& location : map.tiles()) {
		const Tile* tile = location.get();
		if (tile == nullptr || tile->empty()) continue;
		++summary.tileCount;
		const int floor = tile->getPosition().z;
		if (floor >= 0 && floor < static_cast<int>(summary.tilesPerFloor.size())) ++summary.tilesPerFloor[static_cast<size_t>(floor)];
		if (tile->ground) countItem(*tile->ground, true, summary);
		for (const auto& item : tile->items) countItem(*item, false, summary);
	}
	return summary;
}

} // namespace rme::procedural
