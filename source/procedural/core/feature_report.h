// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/features.h"
#include "procedural/core/reference_header.h"
#include "procedural/core/semantic_map.h"
#include "procedural/core/topology.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rme::procedural {

struct UnknownIdCount {
	uint32_t id = 0;
	uint64_t count = 0;
};

// Everything one consolidated report needs. The pointers must outlive the render call.
// Reports are aggregates only: no tile coordinates and no raw tile content.
struct ReportInput {
	std::string sha256;
	std::string clientId;
	std::string compatibility;
	ReferenceHeader header;
	uint64_t tiles = 0;
	uint64_t items = 0;
	uint64_t unknownGround = 0;
	uint64_t unknownItems = 0;
	uint64_t loaderWarnings = 0;
	std::vector<UnknownIdCount> unknownIds; // ascending by id
	TopologyOptions topologyOptions;
	const SemanticMap* map = nullptr;
	const SemanticSummary* semantic = nullptr;
	const TopologyReport* topology = nullptr;
	const FeatureSet* features = nullptr;
};

enum class CsvTable : uint8_t { GroundKeys, Floors, Adjacency, Scales, Distance };
inline constexpr std::array<CsvTable, 5> allCsvTables {
	CsvTable::GroundKeys, CsvTable::Floors, CsvTable::Adjacency, CsvTable::Scales, CsvTable::Distance};

[[nodiscard]] const char* csvTableName(CsvTable table) noexcept;

// Pure functions of the input: same input, same bytes. Integers only (ratios in permille),
// '\n' line endings, no time, no addresses, no locale-dependent formatting.
[[nodiscard]] std::string renderJson(const ReportInput& input);
[[nodiscard]] std::string renderMarkdown(const ReportInput& input);
[[nodiscard]] std::string renderCsv(const ReportInput& input, CsvTable table);

} // namespace rme::procedural
