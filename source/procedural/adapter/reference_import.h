// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/feature_report.h"
#include "procedural/core/reference_header.h"
#include "procedural/core/semantic_map.h"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <stop_token>
#include <string>

// Read-only import of a reference OTBM. Nothing here writes to the input path,
// opens an Editor or touches the user's open maps (Hard Rule 4: no Editor needed).
namespace rme::procedural {

// Budgets of one import. Both limits are required: 0 refuses every file.
struct ReferenceImportLimits {
	uint64_t maxFileBytes = 0;
	// Working set the whole process may reach, enforced three ways: before loading, the current
	// working set plus a lower-bound estimate must fit (a cheap refusal of hopeless files); while the
	// map loads, the measured working set is polled about every 5 ms and the load is aborted past the
	// limit (the overshoot is bounded by what 5 ms of loading allocates); before the semantic model is
	// built, the measured working set plus its bound must fit. Windows only; elsewhere measured = 0.
	uint64_t maxProcessBytes = 0;
	// Pre-load estimate factor (minLoadBytesPerFileByte, a lower bound). Raising it refuses sooner.
	uint64_t loadBytesPerFileByte = minLoadBytesPerFileByte;
	// Polled between stages, while the map loads (about every 5 ms) and every few thousand tiles
	// afterwards. A request during loading aborts the load at the next tile.
	std::stop_token stop;
};

// What an import measured about itself. Not part of any report: times vary between runs.
struct ImportMeasurements {
	uint64_t processBytesBefore = 0;       // working set when the import started
	uint64_t estimatedLoadBytes = 0;       // lower-bound estimate the pre-load check used (0 = none)
	uint64_t peakBytesAfterLoad = 0;       // process peak working set right after loading
	uint64_t peakBytesAfterNormalize = 0;  // ... and after the semantic map was built
	uint32_t loadMs = 0;
	uint32_t normalizeMs = 0;
};

// A valid ground item that no ground brush owns (key "" in the semantic map). Aggregates per item type.
struct UnbrushedGround {
	uint64_t count = 0;
	std::string name;       // from the loaded client's definition
	uint32_t clientId = 0;
	bool blocking = false;
};

// Stacked items owned by one doodad brush, split by surface (z <= 7) and underground floors.
struct DoodadUse {
	uint64_t surfaceItems = 0;
	uint64_t undergroundItems = 0;
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
	std::map<uint32_t, UnbrushedGround> groundWithoutBrush; // server id -> use; filled by normalizeReference only
	std::map<std::string, DoodadUse> doodadBrushes;         // doodad brush name -> items; filled by normalizeReference only
};

enum class ReferenceImportError : uint8_t {
	NoClient, FileMissing, FileTooLarge, HeaderUnreadable, LoadFailed, BudgetExceeded, NormalizeFailed,
	Cancelled,
	EstimateUnavailable,   // compressed file: expanded size unknown, so no pre-load estimate exists
	MemoryEstimateExceeded, // refused before loading: current working set + lower-bound estimate > maxProcessBytes
	MemoryBudgetExceeded    // measured working set over maxProcessBytes, while loading or before building the model
};

[[nodiscard]] const char* referenceImportErrorName(ReferenceImportError value) noexcept;

// Working set of this process now / at its peak so far, in bytes. 0 where it cannot be read.
[[nodiscard]] uint64_t currentProcessBytes();
[[nodiscard]] uint64_t peakProcessBytes();

// Client facts for the currently loaded client, or nullopt when none is loaded.
[[nodiscard]] std::optional<ClientItemVersion> currentClientItemVersion();

// Header, file size and compression only; does not read tiles.
[[nodiscard]] std::expected<ReferenceHeader, ReferenceImportError> readReferenceHeader(
	const std::filesystem::path& file, const ReferenceImportLimits& limits);

// Streams the file once; used before and after import to prove the input is unchanged.
[[nodiscard]] std::expected<std::string, ReferenceImportError> hashReferenceFile(const std::filesystem::path& file);

// Loads the map in memory (companion XML next to it are read, never written),
// then summarises it. Main thread only: the loader reports progress through g_gui.
// Memory is bounded by limits.maxProcessBytes as described there.
[[nodiscard]] std::expected<ReferenceImportSummary, ReferenceImportError> importReference(
	const std::filesystem::path& file, const ReferenceImportLimits& limits, ImportMeasurements* measurements = nullptr);

// Import plus the structural semantic map. Same read-only guarantees as importReference;
// the semantic map is bounded by `budget` cells and the whole process by limits.maxProcessBytes.
struct NormalizedReference {
	ReferenceImportSummary summary;
	SemanticMap map;
	SemanticSummary semantic;
};

struct NormalizeOptions {
	// A ground item that no ground brush owns gets the key "item <name>", the name from the loaded
	// client's definition, instead of the empty key. Off by default so earlier reports stay unchanged.
	bool nameUnbrushedGrounds = false;
};

[[nodiscard]] std::expected<NormalizedReference, ReferenceImportError> normalizeReference(
	const std::filesystem::path& file, const ReferenceImportLimits& limits, const SemanticBudget& budget,
	ImportMeasurements* measurements = nullptr, const NormalizeOptions& options = {});

// Fills the consolidated report input from an import. The referenced objects must outlive it.
[[nodiscard]] ReportInput makeReportInput(const NormalizedReference& normalized, const TopologyReport& topology,
	const FeatureSet& features, const TopologyOptions& options);

} // namespace rme::procedural
