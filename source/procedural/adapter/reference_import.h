// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/reference_header.h"
#include "procedural/core/semantic_map.h"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

// Read-only import of a reference OTBM. Nothing here writes to the input path,
// opens an Editor or touches the user's open maps (Hard Rule 4: no Editor needed).
namespace rme::procedural {

struct ReferenceImportLimits {
	uint64_t maxFileBytes = 0; // required, 0 refuses every file
};

// Aggregates only: no tile coordinates and no raw tile content.
struct ReferenceImportSummary {
	std::string sha256;
	ReferenceHeader header;
	Compatibility compatibility = Compatibility::NoClient;
	std::string clientId;
	uint64_t tileCount = 0;
	uint64_t itemCount = 0;       // ground + stacked items; container contents not counted
	uint64_t unknownGround = 0;   // ground that the loaded client could not resolve
	uint64_t unknownItems = 0;    // stacked items that the loaded client could not resolve
	uint64_t loaderWarnings = 0;
	std::array<uint64_t, 16> tilesPerFloor {};
	std::map<uint32_t, uint64_t> unknownIds; // server id -> occurrences, ordered by id
};

enum class ReferenceImportError : uint8_t {
	NoClient, FileMissing, FileTooLarge, HeaderUnreadable, LoadFailed, BudgetExceeded, NormalizeFailed
};

[[nodiscard]] const char* referenceImportErrorName(ReferenceImportError value) noexcept;

// Client facts for the currently loaded client, or nullopt when none is loaded.
[[nodiscard]] std::optional<ClientItemVersion> currentClientItemVersion();

// Header, file size and compression only; does not read tiles.
[[nodiscard]] std::expected<ReferenceHeader, ReferenceImportError> readReferenceHeader(
	const std::filesystem::path& file, const ReferenceImportLimits& limits);

// Streams the file once; used before and after import to prove the input is unchanged.
[[nodiscard]] std::expected<std::string, ReferenceImportError> hashReferenceFile(const std::filesystem::path& file);

// Loads the map in memory (companion XML next to it are read, never written),
// then summarises it. Main thread only: the loader reports progress through g_gui.
// Peak memory is not bounded by this call; only the file size is limited.
[[nodiscard]] std::expected<ReferenceImportSummary, ReferenceImportError> importReference(
	const std::filesystem::path& file, const ReferenceImportLimits& limits);

// Import plus the structural semantic map. Same read-only guarantees as importReference;
// memory is bounded by the cell budget for the map, not for the loaded Map itself.
struct NormalizedReference {
	ReferenceImportSummary summary;
	SemanticMap map;
	SemanticSummary semantic;
};

[[nodiscard]] std::expected<NormalizedReference, ReferenceImportError> normalizeReference(
	const std::filesystem::path& file, const ReferenceImportLimits& limits, const SemanticBudget& budget);

} // namespace rme::procedural
