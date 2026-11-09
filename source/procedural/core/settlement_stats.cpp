// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/settlement_stats.h"

#include <algorithm>
#include <new>
#include <numeric>
#include <unordered_map>

namespace rme::procedural {

namespace {

// Number of marked cells in any rectangle in O(1): a summed-area table with one extra row and column.
class RectCounter {
public:
	void build(std::span<const uint8_t> mask, uint32_t width, uint32_t height) {
		width_ = width;
		height_ = height;
		sums_.assign(size_t(width + 1) * (height + 1), 0);
		for (uint32_t y = 0; y < height; ++y) {
			uint32_t row = 0;
			for (uint32_t x = 0; x < width; ++x) {
				row += mask[size_t(y) * width + x] != 0;
				sums_[size_t(y + 1) * (width + 1) + x + 1] = sums_[size_t(y) * (width + 1) + x + 1] + row;
			}
		}
	}
	// Marked cells in [x0, x1] x [y0, y1], clipped to the floor; coordinates may be negative.
	[[nodiscard]] uint32_t count(int64_t x0, int64_t y0, int64_t x1, int64_t y1) const {
		x0 = std::max<int64_t>(x0, 0); y0 = std::max<int64_t>(y0, 0);
		x1 = std::min<int64_t>(x1, int64_t(width_) - 1); y1 = std::min<int64_t>(y1, int64_t(height_) - 1);
		if (x0 > x1 || y0 > y1) return 0;
		const size_t stride = width_ + 1;
		return sums_[size_t(y1 + 1) * stride + size_t(x1 + 1)] - sums_[size_t(y0) * stride + size_t(x1 + 1)]
			- sums_[size_t(y1 + 1) * stride + size_t(x0)] + sums_[size_t(y0) * stride + size_t(x0)];
	}
private:
	uint32_t width_ = 0, height_ = 0;
	std::vector<uint32_t> sums_;
};

[[nodiscard]] uint32_t gapBetween(const BuildingBox& a, const BuildingBox& b) {
	const int64_t dx = std::max<int64_t>({0, int64_t(a.x0) - b.x1, int64_t(b.x0) - a.x1});
	const int64_t dy = std::max<int64_t>({0, int64_t(a.y0) - b.y1, int64_t(b.y0) - a.y1});
	return static_cast<uint32_t>(std::max(dx, dy));
}

[[nodiscard]] uint32_t median(std::vector<uint32_t> values) {
	std::sort(values.begin(), values.end());
	const size_t n = values.size();
	return n % 2 ? values[n / 2] : static_cast<uint32_t>((uint64_t(values[n / 2 - 1]) + values[n / 2]) / 2);
}

[[nodiscard]] bool trailOutward(const RectCounter& trail, int64_t x, int64_t y, int64_t dx, int64_t dy) {
	// A trail cell on the straight line of 1..settlementPathReach steps starting beside (x, y).
	const int64_t reach = settlementPathReach;
	const int64_t fromX = x + dx, toX = x + dx * reach, fromY = y + dy, toY = y + dy * reach;
	return trail.count(std::min(fromX, toX), std::min(fromY, toY), std::max(fromX, toX), std::max(fromY, toY)) != 0;
}

} // namespace

void addSettlementFloor(const SettlementFloor& floor, SettlementStats& stats) {
	const uint32_t width = floor.width, height = floor.height;
	for (const uint8_t p : floor.present) stats.regionCells += p != 0;

	RectCounter trail, water;
	trail.build(floor.path, width, height);
	water.build(floor.water, width, height);
	const int64_t pathReach = settlementPathReach, waterReach = settlementWaterReach;

	// Control: the same questions asked of every ordinary land cell.
	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			const size_t i = size_t(y) * width + x;
			if (!floor.present[i] || floor.water[i]) continue;
			++stats.waterControlCells;
			stats.waterControlHit += water.count(int64_t(x) - waterReach, int64_t(y) - waterReach, int64_t(x) + waterReach, int64_t(y) + waterReach) != 0;
			if (floor.path[i]) continue;
			++stats.pathControlCells;
			stats.pathControlHit += trail.count(int64_t(x) - pathReach, int64_t(y) - pathReach, int64_t(x) + pathReach, int64_t(y) + pathReach) != 0;
		}
	}

	const auto& list = floor.buildings;
	stats.buildings += list.size();

	// Nearest neighbour and clusters (single linkage) on this floor.
	std::vector<uint32_t> nearest(list.size(), UINT32_MAX), parent(list.size());
	std::iota(parent.begin(), parent.end(), 0u);
	const auto find = [&](uint32_t v) {
		while (parent[v] != v) { parent[v] = parent[parent[v]]; v = parent[v]; }
		return v;
	};
	for (size_t a = 0; a < list.size(); ++a) {
		for (size_t b = a + 1; b < list.size(); ++b) {
			const uint32_t gap = gapBetween(list[a], list[b]);
			nearest[a] = std::min(nearest[a], gap);
			nearest[b] = std::min(nearest[b], gap);
			if (gap <= settlementClusterGap) parent[find(uint32_t(a))] = find(uint32_t(b));
		}
	}
	std::unordered_map<uint32_t, uint32_t> members;
	for (size_t a = 0; a < list.size(); ++a) {
		if (nearest[a] != UINT32_MAX) stats.nearestGap.push_back(nearest[a]);
		++members[find(uint32_t(a))];
	}
	// Order of the unordered_map does not matter: only the multiset of sizes is used (sorted by the summary).
	for (const auto& [root, size] : members) stats.clusterSizes.push_back(size);

	for (const BuildingBox& b : list) {
		stats.nearPath += trail.count(int64_t(b.x0) - pathReach, int64_t(b.y0) - pathReach, int64_t(b.x1) + pathReach, int64_t(b.y1) + pathReach) != 0;
		stats.nearWater += water.count(int64_t(b.x0) - waterReach, int64_t(b.y0) - waterReach, int64_t(b.x1) + waterReach, int64_t(b.y1) + waterReach) != 0;
		if (b.doorCells.empty()) continue;
		++stats.withDoor;
		stats.doorCells += b.doorCells.size();
		stats.doorsPerDoored.push_back(static_cast<uint32_t>(b.doorCells.size()));

		bool doorSide[4] = {false, false, false, false};  // N, E, S, W
		uint32_t boundary = 0;
		for (const uint32_t cell : b.doorCells) {
			const uint32_t x = cell % width, y = cell / width;
			const bool onN = y == b.y0, onS = y == b.y1, onW = x == b.x0, onE = x == b.x1;
			if (!(onN || onS || onW || onE)) continue;
			++boundary;
			doorSide[0] |= onN; doorSide[1] |= onE; doorSide[2] |= onS; doorSide[3] |= onW;
		}
		stats.boundaryDoorCells += boundary;
		if (boundary == 0) continue;

		const bool near = trail.count(int64_t(b.x0) - pathReach, int64_t(b.y0) - pathReach, int64_t(b.x1) + pathReach, int64_t(b.y1) + pathReach) != 0;
		if (!near) continue;
		// A side has a trail when a straight outward line of 1..3 cells from any cell of that edge meets one.
		bool trailSide[4] = {false, false, false, false};
		for (int64_t x = b.x0; x <= b.x1; ++x) {
			trailSide[0] |= trailOutward(trail, x, b.y0, 0, -1);
			trailSide[2] |= trailOutward(trail, x, b.y1, 0, 1);
		}
		for (int64_t y = b.y0; y <= b.y1; ++y) {
			trailSide[3] |= trailOutward(trail, b.x0, y, -1, 0);
			trailSide[1] |= trailOutward(trail, b.x1, y, 1, 0);
		}
		++stats.facingSet;
		bool hit = false;
		for (int s = 0; s < 4; ++s) {
			stats.facingTrailSides += trailSide[s];
			hit |= doorSide[s] && trailSide[s];
		}
		stats.facingHit += hit;
	}
}

std::expected<SettlementStats, StatsError> extractSettlementStats(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, std::span<const std::string_view> pathKeys,
	int zMin, int zMax, std::stop_token stop) {
	if (zMin > zMax) return std::unexpected(StatsError::InvalidRange);
	try {
		std::unordered_map<std::string_view, StyleKind> kindOf;
		for (const auto& e : keymap) kindOf.emplace(e.key, e.kind);
		const auto& names = map.groundKeys();
		std::vector<uint8_t> isWater(names.size(), 0), isPath(names.size(), 0);
		for (size_t i = 0; i < names.size(); ++i) {
			const auto it = kindOf.find(names[i]);
			if (it != kindOf.end()) isWater[i] = it->second == StyleKind::Water;
			else if (i != noGroundKey) return std::unexpected(StatsError::KeyNotMapped);
			isPath[i] = std::find(pathKeys.begin(), pathKeys.end(), std::string_view(names[i])) != pathKeys.end();
		}
		SettlementStats stats;
		SettlementFloor view;
		for (const SemanticFloor& floor : map.floors()) {
			if (floor.bounds.z < zMin || floor.bounds.z > zMax) continue;
			if (stop.stop_requested()) return std::unexpected(StatsError::Cancelled);
			view.width = static_cast<uint32_t>(floor.bounds.width);
			view.height = static_cast<uint32_t>(floor.bounds.height);
			const size_t n = floor.ground.size();
			view.present.assign(n, 0); view.water.assign(n, 0); view.path.assign(n, 0);
			for (size_t i = 0; i < n; ++i) {
				const bool present = (floor.traits[i] & bit(Trait::Present)) != 0;
				view.present[i] = present;
				view.water[i] = present && isWater[floor.ground[i]];
				view.path[i] = present && isPath[floor.ground[i]];
			}
			view.buildings = buildingBoxesOf(floor);
			addSettlementFloor(view, stats);
		}
		return stats;
	} catch (const std::bad_alloc&) {
		return std::unexpected(StatsError::AllocationFailed);
	}
}

SettlementStats settlementStatsOf(const SurfaceGrid& grid) {
	SettlementFloor view;
	view.width = grid.region.width;
	view.height = grid.region.height;
	const size_t n = grid.cells.size();
	view.present.assign(n, 1); view.water.assign(n, 0); view.path.assign(n, 0);
	for (size_t i = 0; i < n; ++i) {
		view.water[i] = grid.cells[i] == SurfaceKind::Lake || grid.cells[i] == SurfaceKind::River;
		view.path[i] = grid.cells[i] == SurfaceKind::Path;
	}
	view.buildings = floorBoxesOf(grid);
	for (const uint32_t door : grid.doors) {
		const uint32_t x = door % view.width, y = door / view.width;
		for (BuildingBox& box : view.buildings) {
			if (x >= box.x0 && x <= box.x1 && y >= box.y0 && y <= box.y1) { box.doorCells.push_back(door); break; }
		}
	}
	SettlementStats stats;
	addSettlementFloor(view, stats);
	return stats;
}

SettlementSummary summarizeSettlements(const SettlementStats& s) {
	SettlementSummary out;
	out.buildings = static_cast<uint32_t>(s.buildings);
	out.facingSet = static_cast<uint32_t>(s.facingSet);
	if (s.buildings < settlementMinBuildings || s.regionCells == 0) return out;
	const auto permille = [](uint64_t part, uint64_t whole) { return static_cast<uint32_t>(part * 1000 / whole); };
	if (!s.nearestGap.empty()) out.gapMedian = median(s.nearestGap);
	out.clustersPer64kMilli = static_cast<uint32_t>(uint64_t(s.clusterSizes.size()) * 65536 * 1000 / s.regionCells);
	if (!s.clusterSizes.empty()) out.clusterSizeMedian = median(s.clusterSizes);
	uint64_t alone = 0, inLarge = 0;
	for (const uint32_t size : s.clusterSizes) {
		if (size == 1) alone += size;
		if (size >= 3) inLarge += size;
	}
	out.aloneP = permille(alone, s.buildings);
	out.inLargeP = permille(inLarge, s.buildings);
	out.nearPathP = permille(s.nearPath, s.buildings);
	out.nearWaterP = permille(s.nearWater, s.buildings);
	if (s.pathControlCells) out.pathControlP = permille(s.pathControlHit, s.pathControlCells);
	if (s.waterControlCells) out.waterControlP = permille(s.waterControlHit, s.waterControlCells);
	out.withDoorP = permille(s.withDoor, s.buildings);
	if (!s.doorsPerDoored.empty()) out.doorsMedian = median(s.doorsPerDoored);
	if (s.doorCells) out.boundaryDoorP = permille(s.boundaryDoorCells, s.doorCells);
	if (s.facingSet >= settlementMinBuildings) {
		out.facingP = permille(s.facingHit, s.facingSet);
		out.facingChanceP = static_cast<uint32_t>(s.facingTrailSides * 1000 / (4 * s.facingSet));
	}
	return out;
}

} // namespace rme::procedural
