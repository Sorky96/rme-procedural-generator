// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/features.h"

#include <algorithm>
#include <new>

namespace rme::procedural {

namespace {

constexpr uint32_t walkBlockers = bit(Trait::Blocking) | bit(Trait::Unknown);

[[nodiscard]] constexpr int floorDiv(int value, int divisor) noexcept {
	return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

[[nodiscard]] constexpr size_t distanceBin(uint32_t d) noexcept {
	if (d <= 1) return 0;
	if (d == 2) return 1;
	if (d <= 4) return 2;
	if (d <= 8) return 3;
	if (d <= 16) return 4;
	return 5;
}

[[nodiscard]] constexpr size_t decile(uint64_t part, uint64_t whole) noexcept {
	return std::min<uint64_t>(fractionBins - 1, part * fractionBins / whole);
}

struct BlockCount { uint32_t present = 0, walkable = 0, impassable = 0; };

} // namespace

std::expected<FeatureSet, FeatureError> extractFeatures(
	const SemanticMap& map, const TopologyReport& topology, const FeatureOptions& options, std::stop_token stop) {
	const size_t keyCount = map.groundKeys().size();
	if (topology.keys.size() != keyCount) return std::unexpected(FeatureError::InvalidInput);
	if (keyCount > options.maxKeysForAdjacency) return std::unexpected(FeatureError::TooManyKeys);

	FeatureSet out;
	std::vector<uint8_t> distance;
	std::vector<uint32_t> queue;
	try {
		out.adjacency.keyCount = keyCount;
		out.adjacency.east.assign(keyCount * keyCount, 0);
		out.adjacency.south.assign(keyCount * keyCount, 0);
		size_t largest = 0;
		for (const SemanticFloor& floor : map.floors()) largest = std::max(largest, floor.traits.size());
		distance.resize(largest);
		queue.reserve(largest);
		out.floors.reserve(map.floors().size());
	} catch (const std::bad_alloc&) {
		return std::unexpected(FeatureError::AllocationFailed);
	}
	for (size_t s = 0; s < blockScales.size(); ++s) out.scales[s].scale = blockScales[s];

	for (const SemanticFloor& floor : map.floors()) {
		const int w = floor.bounds.width, h = floor.bounds.height;
		const auto cells = floor.traits.size();
		auto present = [&](size_t i) { return (floor.traits[i] & bit(Trait::Present)) != 0; };
		auto impassable = [&](size_t i) { return present(i) && topology.keys[floor.ground[i]].impassable; };
		auto walkable = [&](size_t i) {
			const uint32_t t = floor.traits[i];
			return (t & bit(Trait::HasGround)) && !(t & walkBlockers);
		};

		FloorFeature feature;
		feature.z = floor.bounds.z;
		for (int y = 0; y < h; ++y) {
			if (stop.stop_requested()) return std::unexpected(FeatureError::Cancelled);
			for (int x = 0; x < w; ++x) {
				const auto i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
				if (!present(i)) continue;
				++feature.tiles;
				for (size_t t = 0; t < traitCount; ++t) {
					if (floor.traits[i] & (1u << t)) ++feature.traitTiles[t];
				}
				if (x + 1 < w && present(i + 1)) ++out.adjacency.east[floor.ground[i] * keyCount + floor.ground[i + 1]];
				if (y + 1 < h && present(i + static_cast<size_t>(w))) {
					++out.adjacency.south[floor.ground[i] * keyCount + floor.ground[i + static_cast<size_t>(w)]];
				}
			}
		}
		try {
			out.floors.push_back(feature);
		} catch (const std::bad_alloc&) {
			return std::unexpected(FeatureError::AllocationFailed);
		}

		// Aligned absolute blocks.
		for (size_t s = 0; s < blockScales.size(); ++s) {
			const int scale = static_cast<int>(blockScales[s]);
			const int bx0 = floorDiv(floor.bounds.x0, scale), bx1 = floorDiv(floor.bounds.x0 + w - 1, scale);
			const int by0 = floorDiv(floor.bounds.y0, scale), by1 = floorDiv(floor.bounds.y0 + h - 1, scale);
			const auto bw = static_cast<size_t>(bx1 - bx0 + 1);
			std::vector<BlockCount> blocks;
			try {
				blocks.resize(bw * static_cast<size_t>(by1 - by0 + 1));
			} catch (const std::bad_alloc&) {
				return std::unexpected(FeatureError::AllocationFailed);
			}
			for (int y = 0; y < h; ++y) {
				if (stop.stop_requested()) return std::unexpected(FeatureError::Cancelled);
				for (int x = 0; x < w; ++x) {
					const auto i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
					if (!present(i)) continue;
					const auto bx = static_cast<size_t>(floorDiv(floor.bounds.x0 + x, scale) - bx0);
					const auto by = static_cast<size_t>(floorDiv(floor.bounds.y0 + y, scale) - by0);
					BlockCount& block = blocks[by * bw + bx];
					++block.present;
					if (walkable(i)) ++block.walkable;
					if (impassable(i)) ++block.impassable;
				}
			}
			ScaleStats& stats = out.scales[s];
			const uint64_t area = static_cast<uint64_t>(scale) * static_cast<uint64_t>(scale);
			for (const BlockCount& block : blocks) {
				if (block.present == 0) continue;
				++stats.blocks;
				++stats.occupancy[decile(block.present, area)];
				++stats.walkableShare[decile(block.walkable, block.present)];
				++stats.impassableShare[decile(block.impassable, block.present)];
			}
		}

		// Distance to the nearest impassable-ground tile: multi-source BFS over the floor box.
		constexpr uint8_t unreached = 255;
		std::fill_n(distance.begin(), static_cast<std::ptrdiff_t>(cells), unreached);
		queue.clear();
		for (size_t i = 0; i < cells; ++i) {
			if (impassable(i)) { distance[i] = 0; queue.push_back(static_cast<uint32_t>(i)); }
		}
		for (size_t head = 0; head < queue.size(); ++head) {
			if ((head & 4095) == 0 && stop.stop_requested()) return std::unexpected(FeatureError::Cancelled);
			const uint32_t i = queue[head];
			const int x = static_cast<int>(i % static_cast<uint32_t>(w)), y = static_cast<int>(i / static_cast<uint32_t>(w));
			const uint8_t next = distance[i] == 254 ? uint8_t {254} : static_cast<uint8_t>(distance[i] + 1);
			auto visit = [&](size_t j) {
				if (distance[j] == unreached) { distance[j] = next; queue.push_back(static_cast<uint32_t>(j)); }
			};
			if (x > 0) visit(i - 1);
			if (x + 1 < w) visit(i + 1);
			if (y > 0) visit(i - static_cast<size_t>(w));
			if (y + 1 < h) visit(i + static_cast<size_t>(w));
		}
		for (size_t i = 0; i < cells; ++i) {
			if (!present(i) || distance[i] == 0) continue;
			const size_t bin = distanceBin(distance[i] == unreached ? 255u : distance[i]);
			const uint32_t t = floor.traits[i];
			++out.distance.tiles[bin];
			if (t & bit(Trait::Doodad)) ++out.distance.doodad[bin];
			if (t & bit(Trait::Wall)) ++out.distance.wall[bin];
			if (t & bit(Trait::Border)) ++out.distance.border[bin];
		}
	}
	return out;
}

} // namespace rme::procedural
