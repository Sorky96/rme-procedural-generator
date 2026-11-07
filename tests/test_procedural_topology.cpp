// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/topology.h"

#include <iostream>
#include <string>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

// '.' walkable grass, '#' blocking grass (wall), 'w' water (blocking ground), 'u' grass with unknown
// content, 'f' walkable grass with a floor change, 't' walkable grass with a teleport, ' ' no tile.
SemanticMap build(const std::vector<std::string>& rows, int z = 7) {
	const FloorBounds bounds {z, 0, 0, static_cast<int>(rows.front().size()), static_cast<int>(rows.size())};
	auto map = SemanticMap::create(std::vector<FloorBounds> {bounds}, {1'000'000}).value();
	const uint16_t grass = map.internGroundKey("grass").value();
	const uint16_t water = map.internGroundKey("water").value();
	for (size_t y = 0; y < rows.size(); ++y) {
		for (size_t x = 0; x < rows[y].size(); ++x) {
			const char c = rows[y][x];
			uint16_t key = grass;
			uint32_t traits = bit(Trait::HasGround);
			switch (c) {
				case ' ': continue;
				case '#': traits |= bit(Trait::Blocking) | bit(Trait::Wall); break;
				case 'w': key = water; traits |= bit(Trait::Blocking); break;
				case 'u': traits |= bit(Trait::Unknown); break;
				case 'f': traits |= bit(Trait::FloorChange); break;
				case 't': traits |= bit(Trait::Teleport); break;
				default: break;
			}
			check(map.set(static_cast<int>(x), static_cast<int>(y), z, key, traits).has_value(), "set cell");
		}
	}
	return map;
}

TopologyReport analyze(const SemanticMap& map, const TopologyOptions& options = {}) {
	const auto result = analyzeTopology(map, summarize(map), options, {});
	check(result.has_value(), "analysis succeeds");
	return result.value_or(TopologyReport {});
}
}

int main() {
	// 7x4 grid worked out by hand: three walkable components (4, 2, 10 cells), one unknown cell,
	// one empty position, one sea strip that runs off the right edge.
	const auto grid = build({
		"..#...w",
		"..#.u.w",
		"###...w",
		".. ..ww"});
	TopologyOptions options;
	options.smallComponentCells = 3;
	const TopologyReport report = analyze(grid, options);
	check(report.floors.size() == 1, "one floor");
	const FloorTopology& floor = report.floors.at(0);
	check(floor.tiles == 27 && floor.unknownCells == 1 && floor.walkableCells == 16, "floor cell counts");
	check(floor.walkable.count == 3 && floor.walkable.cells == 16 && floor.walkable.largest == 10, "walkable components");
	check(floor.walkable.top == std::array<uint64_t, topComponentCount> {10, 4, 2, 0, 0}, "top sizes");
	check(floor.walkable.smallCount == 1 && floor.walkable.smallCells == 2, "small pockets (<3 cells)");
	check(floor.walkable.buckets[1] == 1 && floor.walkable.buckets[2] == 1 && floor.walkable.buckets[3] == 1, "size buckets");
	check(report.keys.size() == 3, "keys: none, grass, water");
	const KeyRegionStats& grass = report.keys[1];
	const KeyRegionStats& sea = report.keys[2];
	check(grass.tiles == 22 && grass.regions == 1 && grass.largest == 22 && grass.enclosed == 0 && !grass.impassable, "grass region");
	check(sea.tiles == 5 && sea.regions == 1 && sea.largest == 5 && sea.enclosed == 0 && sea.impassable, "sea touches edge");

	// Diagonal contact does not connect: two walkable cells touching only at a corner.
	const TopologyReport diagonal = analyze(build({".#", "#."}));
	check(diagonal.floors[0].walkable.count == 2, "diagonals do not connect");

	// A lake inside land is enclosed; a pond in the corner is not.
	const TopologyReport lakes = analyze(build({
		"w....",
		".....",
		"..w..",
		".....",
		"....."}));
	check(lakes.keys[2].regions == 2 && lakes.keys[2].enclosed == 1 && lakes.keys[2].largest == 1, "enclosed lake");

	// Floor changes and teleports are counted per floor and do not block walking.
	const TopologyReport links = analyze(build({"ft"}, 6));
	check(links.floors[0].floorChangeCells == 1 && links.floors[0].teleportCells == 1 &&
		links.floors[0].walkable.count == 1 && links.floors[0].walkable.cells == 2 && links.floors[0].bounds.z == 6, "floor links");

	// The ground-key table of `none` (key 0) forms regions too, but is never impassable.
	const TopologyReport none = analyze(build({".   ."}));
	check(none.keys[0].regions == 0 && !none.keys[0].impassable, "empty positions are not regions");

	// Cancellation and option checks.
	std::stop_source cancel;
	cancel.request_stop();
	const auto cancelled = analyzeTopology(grid, summarize(grid), {}, cancel.get_token());
	check(!cancelled.has_value() && cancelled.error() == TopologyError::Cancelled, "cancelled before work");
	TopologyOptions bad;
	bad.impassablePercent = 0;
	check(analyzeTopology(grid, summarize(grid), bad, {}).error() == TopologyError::InvalidOptions, "percent 0 refused");
	bad = {};
	bad.smallComponentCells = 0;
	check(analyzeTopology(grid, summarize(grid), bad, {}).error() == TopologyError::InvalidOptions, "small limit 0 refused");
	SemanticSummary wrongSize = summarize(grid);
	wrongSize.groundKeyStats.pop_back();
	check(analyzeTopology(grid, wrongSize, {}, {}).error() == TopologyError::InvalidOptions, "summary size mismatch refused");

	// Repeatable.
	const TopologyReport again = analyze(grid, options);
	check(again.floors[0].walkable.top == floor.walkable.top && again.floors[0].walkable.buckets == floor.walkable.buckets &&
		again.keys[2].regions == sea.regions, "repeatable");

	if (failures == 0) std::cout << "TOPOLOGY_PASS\n";
	return failures == 0 ? 0 : 1;
}
