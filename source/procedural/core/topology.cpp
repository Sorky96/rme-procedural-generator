// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/topology.h"

#include <algorithm>
#include <bit>
#include <new>

namespace rme::procedural {

namespace {

struct CellState {
	uint8_t nextDirection : 3 = 0; // four directions and the completed state
	uint8_t parentDirection : 2 = 0;
	uint8_t bottleneck : 1 = 0;
	uint8_t touchesEdge : 1 = 0;
};
static_assert(sizeof(CellState) == 1);

// Scratch for union-find ground regions and the depth-first walkability traversal.
struct Labeler {
	std::vector<uint32_t> parent;
	std::vector<uint32_t> size;
	std::vector<CellState> state;

	void reserve(size_t cells) {
		parent.resize(cells);
		size.resize(cells);
		state.resize(cells);
	}

	uint32_t find(uint32_t i) {
		while (parent[i] != i) {
			parent[i] = parent[parent[i]];
			i = parent[i];
		}
		return i;
	}

	void unite(uint32_t a, uint32_t b) {
		a = find(a);
		b = find(b);
		if (a == b) return;
		if (a > b) std::swap(a, b); // smaller index stays root: result does not depend on call order
		parent[b] = a;
	}

	// active(i): cell takes part; same(i, j): neighbours i and j belong together.
	template <class Active, class Same>
	bool run(int width, int height, Active active, Same same, std::stop_token stop) {
		const auto cells = static_cast<size_t>(width) * static_cast<size_t>(height);
		for (size_t i = 0; i < cells; ++i) { parent[i] = static_cast<uint32_t>(i); size[i] = 0; state[i] = {}; }
		for (int y = 0; y < height; ++y) {
			if (stop.stop_requested()) return false;
			for (int x = 0; x < width; ++x) {
				const auto i = static_cast<uint32_t>(y * width + x);
				if (!active(i)) continue;
				if (x > 0 && active(i - 1) && same(i, i - 1)) unite(i, i - 1);
				if (y > 0 && active(i - static_cast<uint32_t>(width)) && same(i, i - static_cast<uint32_t>(width))) {
					unite(i, i - static_cast<uint32_t>(width));
				}
			}
		}
		for (int y = 0; y < height; ++y) {
			if (stop.stop_requested()) return false;
			for (int x = 0; x < width; ++x) {
				const auto i = static_cast<uint32_t>(y * width + x);
				if (!active(i)) continue;
				const uint32_t root = find(i);
				++size[root];
				if (x == 0 || y == 0 || x == width - 1 || y == height - 1) state[root].touchesEdge = true;
			}
		}
		return true;
	}
};

void addComponent(ComponentStats& stats, uint64_t size, uint32_t smallLimit) {
	++stats.count;
	stats.cells += size;
	stats.largest = std::max(stats.largest, size);
	if (size < smallLimit) { ++stats.smallCount; stats.smallCells += size; }
	const auto bucket = std::min<size_t>(static_cast<size_t>(std::bit_width(size)) - 1, sizeBucketCount - 1);
	++stats.buckets[bucket];
	for (size_t k = 0; k < topComponentCount; ++k) {
		if (size > stats.top[k]) {
			std::copy_backward(stats.top.begin() + static_cast<std::ptrdiff_t>(k), stats.top.end() - 1,
				stats.top.end());
			stats.top[k] = size;
			break;
		}
	}
}

// Iterative depth-first search. If a child's subtree cannot reach an ancestor of
// this cell, removing the cell separates the subtree from the rest of the component.
template <class Walkable>
bool analyzeWalkability(uint32_t width, uint32_t height, Walkable walkable, Labeler& scratch,
	FloorTopology& out, uint32_t smallLimit, std::stop_token stop) {
	const uint32_t cells = width * height;
	// Directions are left, right, up, down. Flipping bit 0 reverses a step.
	const std::array<int64_t, 4> offsets {-1, 1, -static_cast<int64_t>(width), width};
	const auto neighbor = [&](uint32_t cell, uint8_t direction) {
		return static_cast<uint32_t>(cell + offsets[direction]);
	};
	auto& discovered = scratch.parent;
	auto& low = scratch.size;
	std::fill_n(discovered.begin(), cells, 0);
	uint32_t time = 0;
	uint32_t steps = 0;
	for (uint32_t root = 0; root < cells; ++root) {
		if ((root % 4096 == 0) && stop.stop_requested()) return false;
		if (!walkable(root) || discovered[root] != 0) continue;
		const uint32_t start = time;
		uint32_t rootChildren = 0;
		low[root] = discovered[root] = ++time;
		scratch.state[root] = {};
		uint32_t cell = root;
		for (;;) {
			if ((++steps % 4096 == 0) && stop.stop_requested()) return false;
			CellState& state = scratch.state[cell];
			if (state.nextDirection == offsets.size()) {
				if (cell == root) {
					out.bottleneckCells += rootChildren > 1;
					break;
				}
				out.bottleneckCells += state.bottleneck;
				const uint32_t parent = neighbor(cell, state.parentDirection);
				low[parent] = std::min(low[parent], low[cell]);
				scratch.state[parent].bottleneck |= low[cell] >= discovered[parent];
				cell = parent;
				continue;
			}

			const uint8_t direction = state.nextDirection++;
			if (cell != root && direction == state.parentDirection) continue;
			switch (direction) {
				case 0: if (cell % width == 0) continue; break;
				case 1: if (cell % width == width - 1) continue; break;
				case 2: if (cell < width) continue; break;
				default: if (cell >= cells - width) continue; break;
			}
			const uint32_t next = neighbor(cell, direction);
			if (!walkable(next)) continue;
			if (discovered[next] != 0) {
				low[cell] = std::min(low[cell], discovered[next]);
			} else {
				rootChildren += cell == root;
				low[next] = discovered[next] = ++time;
				scratch.state[next] = {};
				scratch.state[next].parentDirection = direction ^ 1;
				cell = next;
			}
		}
		addComponent(out.walkable, time - start, smallLimit);
	}
	return !stop.stop_requested();
}

} // namespace

std::expected<TopologyReport, TopologyError> analyzeTopology(
	const SemanticMap& map, const SemanticSummary& summary, const TopologyOptions& options, std::stop_token stop) {
	if (options.impassablePercent < 1 || options.impassablePercent > 100 || options.smallComponentCells == 0
		|| summary.groundKeyStats.size() != map.groundKeys().size()) {
		return std::unexpected(TopologyError::InvalidOptions);
	}
	TopologyReport report;
	Labeler labeler;
	try {
		report.keys.resize(summary.groundKeyStats.size());
		for (size_t k = 0; k < report.keys.size(); ++k) {
			const GroundKeyStat& stat = summary.groundKeyStats[k];
			report.keys[k].tiles = stat.tiles;
			report.keys[k].impassable = k != noGroundKey && stat.tiles > 0
				&& stat.blockingTiles * 100 >= stat.tiles * options.impassablePercent;
		}
		size_t largestFloor = 0;
		for (const SemanticFloor& floor : map.floors()) largestFloor = std::max(largestFloor, floor.traits.size());
		labeler.reserve(largestFloor);
		report.floors.reserve(map.floors().size());
	} catch (const std::bad_alloc&) {
		return std::unexpected(TopologyError::AllocationFailed);
	}

	constexpr uint32_t walkBlockers = bit(Trait::Blocking) | bit(Trait::Unknown);
	for (const SemanticFloor& floor : map.floors()) {
		const int w = floor.bounds.width, h = floor.bounds.height;
		FloorTopology out;
		out.bounds = floor.bounds;
		for (const uint32_t traits : floor.traits) {
			if (!(traits & bit(Trait::Present))) continue;
			++out.tiles;
			if (traits & bit(Trait::Unknown)) ++out.unknownCells;
			if (traits & bit(Trait::FloorChange)) ++out.floorChangeCells;
			if (traits & bit(Trait::Teleport)) ++out.teleportCells;
			if ((traits & bit(Trait::HasGround)) && !(traits & walkBlockers)) ++out.walkableCells;
		}
		auto walkable = [&](uint32_t i) {
			const uint32_t t = floor.traits[i];
			return (t & bit(Trait::HasGround)) && !(t & walkBlockers);
		};
		if (!analyzeWalkability(w, h, walkable, labeler, out, options.smallComponentCells, stop)) {
			return std::unexpected(TopologyError::Cancelled);
		}

		auto present = [&](uint32_t i) { return (floor.traits[i] & bit(Trait::Present)) != 0; };
		if (!labeler.run(w, h, present, [&](uint32_t a, uint32_t b) { return floor.ground[a] == floor.ground[b]; }, stop)) {
			return std::unexpected(TopologyError::Cancelled);
		}
		for (uint32_t i = 0; i < static_cast<uint32_t>(floor.traits.size()); ++i) {
			if (!present(i) || labeler.parent[i] != i) continue;
			KeyRegionStats& key = report.keys[floor.ground[i]];
			++key.regions;
			key.largest = std::max<uint64_t>(key.largest, labeler.size[i]);
			if (!labeler.state[i].touchesEdge) ++key.enclosed;
		}
		try {
			report.floors.push_back(out);
		} catch (const std::bad_alloc&) {
			return std::unexpected(TopologyError::AllocationFailed);
		}
	}
	return report;
}

} // namespace rme::procedural
