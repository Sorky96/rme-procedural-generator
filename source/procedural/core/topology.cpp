// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/topology.h"

#include <algorithm>
#include <bit>
#include <new>

namespace rme::procedural {

namespace {

// Union-find labelling of one floor box. Buffers are owned by the caller and reused.
struct Labeler {
	std::vector<uint32_t> parent;
	std::vector<uint32_t> size;
	std::vector<uint8_t> touchesEdge;

	void reserve(size_t cells) {
		parent.resize(cells);
		size.resize(cells);
		touchesEdge.resize(cells);
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
		for (size_t i = 0; i < cells; ++i) { parent[i] = static_cast<uint32_t>(i); size[i] = 0; touchesEdge[i] = 0; }
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
				if (x == 0 || y == 0 || x == width - 1 || y == height - 1) touchesEdge[root] = 1;
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
		if (!labeler.run(w, h, walkable, [](uint32_t, uint32_t) { return true; }, stop)) {
			return std::unexpected(TopologyError::Cancelled);
		}
		for (uint32_t i = 0; i < static_cast<uint32_t>(floor.traits.size()); ++i) {
			if (walkable(i) && labeler.parent[i] == i) addComponent(out.walkable, labeler.size[i], options.smallComponentCells);
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
			if (!labeler.touchesEdge[i]) ++key.enclosed;
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
