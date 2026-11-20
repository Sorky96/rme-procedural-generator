// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/feature_report.h"

#include <iostream>
#include <string>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
bool contains(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

// 3x2 map at z=7: grass with a doodad at (1,0) and a wall at (0,1); water in the last column.
//   . D w
//   # . w
SemanticMap smallMap(const char* grassName = "grass") {
	auto map = SemanticMap::create(std::vector<FloorBounds> {{7, 0, 0, 3, 2}}, {1000}).value();
	const uint16_t grass = map.internGroundKey(grassName).value();
	const uint16_t water = map.internGroundKey("water").value();
	const uint32_t ground = bit(Trait::HasGround);
	check(map.set(0, 0, 7, grass, ground).has_value(), "set");
	check(map.set(1, 0, 7, grass, ground | bit(Trait::Doodad)).has_value(), "set");
	check(map.set(2, 0, 7, water, ground | bit(Trait::Blocking)).has_value(), "set");
	check(map.set(0, 1, 7, grass, ground | bit(Trait::Blocking) | bit(Trait::Wall)).has_value(), "set");
	check(map.set(1, 1, 7, grass, ground).has_value(), "set");
	check(map.set(2, 1, 7, water, ground | bit(Trait::Blocking)).has_value(), "set");
	return map;
}

struct Analysis {
	SemanticMap map;
	SemanticSummary summary;
	TopologyReport topology;
	FeatureSet features;
};

Analysis analyze(SemanticMap map) {
	Analysis a {std::move(map), {}, {}, {}};
	a.summary = summarize(a.map);
	a.topology = analyzeTopology(a.map, a.summary, {}, {}).value();
	const auto features = extractFeatures(a.map, a.topology, {}, {});
	check(features.has_value(), "features succeed");
	if (features) a.features = *features;
	return a;
}

ReportInput inputFor(const Analysis& a) {
	ReportInput in;
	in.sha256 = "00ff";
	in.clientId = "test-client";
	in.compatibility = "compatible";
	in.header = {2, 3, 2, 3, 57, false, 99};
	in.tiles = 6;
	in.items = 6;
	in.unknownIds = {{60000, 2}};
	in.unknownGround = 1;
	in.unknownItems = 1;
	in.map = &a.map;
	in.semantic = &a.summary;
	in.topology = &a.topology;
	in.features = &a.features;
	return in;
}
}

int main() {
	const Analysis a = analyze(smallMap());
	const auto& adj = a.features.adjacency;
	check(adj.keyCount == 3, "key count");
	check(adj.east[1 * 3 + 1] == 2 && adj.east[1 * 3 + 2] == 2 && adj.south[1 * 3 + 1] == 2 && adj.south[2 * 3 + 2] == 1, "adjacency");
	uint64_t eastTotal = 0, southTotal = 0;
	for (size_t i = 0; i < 9; ++i) { eastTotal += adj.east[i]; southTotal += adj.south[i]; }
	check(eastTotal == 4 && southTotal == 3, "no extra adjacency pairs (2 east per row, 1 south per column)");

	for (const ScaleStats& sc : a.features.scales) {
		check(sc.blocks == 1 && sc.occupancy[0] == 1 && sc.walkableShare[5] == 1 && sc.impassableShare[3] == 1, "one block per scale");
	}
	check(a.features.floors.size() == 1 && a.features.floors[0].tiles == 6, "floor tiles");
	const auto& traits = a.features.floors[0].traitTiles;
	check(traits[0] == 6 && traits[1] == 3 && traits[4] == 1 && traits[13] == 1, "floor trait counts");

	const auto& d = a.features.distance;
	check(d.tiles[0] == 2 && d.tiles[1] == 2 && d.tiles[2] == 0, "distance tiles");
	check(d.doodad[0] == 1 && d.wall[1] == 1 && d.border[0] == 0, "distance trait counts");

	// Blocks are aligned to absolute coordinates, also for negative x.
	auto negative = SemanticMap::create(std::vector<FloorBounds> {{7, -1, 0, 2, 1}}, {10}).value();
	const uint16_t key = negative.internGroundKey("grass").value();
	check(negative.set(-1, 0, 7, key, bit(Trait::HasGround)).has_value() && negative.set(0, 0, 7, key, bit(Trait::HasGround)).has_value(), "set");
	const Analysis neg = analyze(std::move(negative));
	check(neg.features.scales[0].blocks == 2 && neg.features.scales[2].blocks == 2, "x=-1 and x=0 are in different blocks");

	// Distance with no impassable ground on the floor falls into the last bin.
	auto dry = SemanticMap::create(std::vector<FloorBounds> {{7, 0, 0, 2, 1}}, {10}).value();
	const uint16_t dryKey = dry.internGroundKey("grass").value();
	check(dry.set(0, 0, 7, dryKey, bit(Trait::HasGround)).has_value() && dry.set(1, 0, 7, dryKey, bit(Trait::HasGround)).has_value(), "set");
	const Analysis dryAnalysis = analyze(std::move(dry));
	check(dryAnalysis.features.distance.tiles[5] == 2, "no source: last bin");

	// Refusals.
	const SemanticSummary s = summarize(a.map);
	const TopologyReport t = analyzeTopology(a.map, s, {}, {}).value();
	FeatureOptions few;
	few.maxKeysForAdjacency = 2;
	check(extractFeatures(a.map, t, few, {}).error() == FeatureError::TooManyKeys, "too many keys refused");
	TopologyReport mismatched = t;
	mismatched.keys.pop_back();
	check(extractFeatures(a.map, mismatched, {}, {}).error() == FeatureError::InvalidInput, "topology mismatch refused");
	std::stop_source cancel;
	cancel.request_stop();
	check(extractFeatures(a.map, t, {}, cancel.get_token()).error() == FeatureError::Cancelled, "cancelled");

	// Reports: repeatable bytes, escaping and quoting, permille, every table has a header.
	const Analysis quoted = analyze(smallMap("brick roof, \"down\""));
	const ReportInput in = inputFor(quoted);
	const std::string json = renderJson(in);
	check(json == renderJson(in) && renderMarkdown(in) == renderMarkdown(in), "renderers repeatable");
	check(contains(json, "\"schema\": \"g3-reference-analysis/1\"") && contains(json, "brick roof, \\\"down\\\""), "json schema and escaping");
	check(contains(json, "\"unknown_ids\": [{\"id\": 60000, \"count\": 2}]"), "json unknown ids");
	check(contains(json, "[1, 2, 2, 0]") && contains(json, "[2, 2, 0, 1]"), "json adjacency pairs");
	check(contains(json, "\"bottleneck_cells\": 1"), "json includes the middle tile of the three-cell walkable path");
	const std::string keysCsv = renderCsv(in, CsvTable::GroundKeys);
	check(contains(keysCsv, "1,\"brick roof, \"\"down\"\"\",4,"), "csv quoting");
	const std::string distanceCsv = renderCsv(in, CsvTable::Distance);
	check(contains(distanceCsv, "1,2,1,0,0,500,0,0\n") && contains(distanceCsv, "2,2,0,1,0,0,500,0\n"), "csv distance permille");
	check(renderCsv(in, CsvTable::Floors).starts_with("z,width,height,tiles,walkable_cells") &&
		renderCsv(in, CsvTable::Scales).starts_with("scale,histogram,bin,blocks") &&
		renderCsv(in, CsvTable::Adjacency).starts_with("key_a,key_b,east,south") &&
		std::string(csvTableName(CsvTable::Adjacency)) == "adjacency", "csv headers");
	const std::string md = renderMarkdown(in);
	check(contains(md, "# Reference map analysis (G3)") && contains(md, "## Floors") && contains(md, "## Unknown ids") &&
		contains(md, "| 60000 | 2 |") && !contains(md, "\r"), "markdown sections");
	check(contains(md, "| bottleneck tiles |") && contains(md, "| 7 | 6 | 3 | 1 | 3 | 1 | 0 | 0 | 1 |"), "markdown bottleneck count");
	const std::string floorsCsv = renderCsv(in, CsvTable::Floors);
	check(contains(floorsCsv, ",bottleneck_cells\n") && floorsCsv.ends_with(",1\n"), "floors CSV bottleneck count");

	if (failures == 0) std::cout << "FEATURES_PASS\n";
	return failures == 0 ? 0 : 1;
}
