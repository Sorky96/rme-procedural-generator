// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/region_graph.h"

#include "procedural/core/reachability.h"

#include <algorithm>
#include <map>
#include <new>
#include <utility>

namespace rme::procedural {

namespace {

// 0 = Land, 1 = Rock, 2 = Water, 255 = no class (Preserve)
uint8_t classOf(SurfaceKind kind) noexcept {
	switch (kind) {
		case SurfaceKind::Grass: case SurfaceKind::Dirt: case SurfaceKind::Shore: case SurfaceKind::Path: case SurfaceKind::Floor: return 0;
		case SurfaceKind::Rock: return 1;
		case SurfaceKind::Lake: case SurfaceKind::River: return 2;
		default: return 255;
	}
}

} // namespace

std::expected<RegionGraph, RegionGraphError> buildRegionGraph(const SurfaceGrid& grid, bool walls, std::stop_token stop) {
	if (!grid.valid()) return std::unexpected(RegionGraphError::InvalidGrid);
	const size_t width = grid.region.width, height = grid.region.height, count = grid.cells.size();
	RegionGraph graph;
	try {
		// Components of one class (4-neighbour), numbered in order of their first cell; label 0 = excluded.
		std::vector<uint32_t> label(count, 0);
		std::vector<uint32_t> queue;
		queue.reserve(count);
		std::vector<uint32_t> sizes {0};
		std::vector<uint8_t> classes {255};
		for (size_t y = 0; y < height; ++y) {
			if (stop.stop_requested()) return std::unexpected(RegionGraphError::Cancelled);
			for (size_t x = 0; x < width; ++x) {
				const size_t start = y * width + x;
				const uint8_t kind = classOf(grid.cells[start]);
				if (kind == 255) { if (!label[start]) { label[start] = 0; ++graph.excludedCells; } continue; }
				if (label[start]) continue;
				const uint32_t id = uint32_t(sizes.size());
				queue.assign(1, uint32_t(start));
				label[start] = id;
				for (size_t head = 0; head < queue.size(); ++head) {
					const size_t i = queue[head];
					const size_t ix = i % width, iy = i / width;
					const size_t around[4] = {ix > 0 ? i - 1 : count, ix + 1 < width ? i + 1 : count, iy > 0 ? i - width : count, iy + 1 < height ? i + width : count};
					for (const size_t n : around) {
						if (n == count || label[n] || classOf(grid.cells[n]) != kind) continue;
						label[n] = id;
						queue.push_back(uint32_t(n));
					}
				}
				sizes.push_back(uint32_t(queue.size()));
				classes.push_back(kind);
			}
		}

		// Regions: components of at least regionMinCells cells; the rest are fragments.
		std::vector<int32_t> regionOf(sizes.size(), -1);
		for (uint32_t id = 1; id < sizes.size(); ++id) {
			if (sizes[id] < regionMinCells) { ++graph.smallFragments; graph.smallFragmentCells += sizes[id]; continue; }
			regionOf[id] = int32_t(graph.regions.size());
			RegionNode node;
			node.kind = classes[id] == 0 ? RegionClass::Land : classes[id] == 1 ? RegionClass::Rock : RegionClass::Water;
			node.cells = sizes[id];
			node.minX = uint32_t(width); node.minY = uint32_t(height);
			graph.regions.push_back(node);
		}
		std::vector<uint64_t> sumX(graph.regions.size(), 0), sumY(graph.regions.size(), 0);
		for (size_t i = 0; i < count; ++i) {
			const int32_t region = regionOf[label[i]];
			if (region < 0) continue;
			RegionNode& node = graph.regions[size_t(region)];
			const uint32_t x = uint32_t(i % width), y = uint32_t(i / width);
			node.minX = std::min(node.minX, x); node.maxX = std::max(node.maxX, x);
			node.minY = std::min(node.minY, y); node.maxY = std::max(node.maxY, y);
			sumX[size_t(region)] += x; sumY[size_t(region)] += y;
		}
		for (size_t r = 0; r < graph.regions.size(); ++r) {
			graph.regions[r].centroidX = uint32_t(sumX[r] / graph.regions[r].cells);
			graph.regions[r].centroidY = uint32_t(sumY[r] / graph.regions[r].cells);
		}

		// Adjacency: every pair of 4-adjacent cells that belong to two different regions, counted once.
		std::map<std::pair<uint32_t, uint32_t>, uint32_t> boundary;
		for (size_t y = 0; y < height; ++y) {
			if (stop.stop_requested()) return std::unexpected(RegionGraphError::Cancelled);
			for (size_t x = 0; x < width; ++x) {
				const size_t i = y * width + x;
				const int32_t a = regionOf[label[i]];
				if (a < 0) continue;
				for (const size_t n : {x + 1 < width ? i + 1 : count, y + 1 < height ? i + width : count}) {
					if (n == count) continue;
					const int32_t b = regionOf[label[n]];
					if (b < 0 || b == a) continue;
					++boundary[{uint32_t(std::min(a, b)), uint32_t(std::max(a, b))}];
				}
			}
		}
		for (const auto& [pair, weight] : boundary) graph.edges.push_back({pair.first, pair.second, weight});

		// POI nodes with their distance from the start over walkable cells.
		const std::vector<uint32_t> openings = openingsOf(grid);
		auto walkable = walkableLabelsOf(grid.cells, grid.region.width, grid.region.height, openings, walls, stop);
		if (!walkable) return std::unexpected(walkable.error() == ReachabilityError::Cancelled ? RegionGraphError::Cancelled : RegionGraphError::AllocationFailed);
		const auto start = fixedStartOf(*walkable, grid.region.width, grid.region.height);
		if (!start) return graph;
		std::vector<int32_t> distance(count, -1);
		queue.assign(1, *start);
		distance[*start] = 0;
		for (size_t head = 0; head < queue.size(); ++head) {
			const size_t i = queue[head];
			const size_t ix = i % width, iy = i / width;
			const size_t around[4] = {ix > 0 ? i - 1 : count, ix + 1 < width ? i + 1 : count, iy > 0 ? i - width : count, iy + 1 < height ? i + width : count};
			for (const size_t n : around) {
				if (n == count || distance[n] >= 0 || !walkable->label[n]) continue;
				distance[n] = distance[i] + 1;
				queue.push_back(uint32_t(n));
			}
		}
		const auto addPoi = [&](PoiKind kind, uint32_t cell) {
			graph.pois.push_back({kind, cell, regionOf[label[cell]], distance[cell]});
		};
		addPoi(PoiKind::Start, *start);
		for (const uint32_t door : grid.doors) addPoi(PoiKind::Entrance, door);
		for (const uint32_t cell : exitCellsOf(*walkable, grid.region.width, grid.region.height)) addPoi(PoiKind::Exit, cell);
	} catch (const std::bad_alloc&) {
		return std::unexpected(RegionGraphError::AllocationFailed);
	}
	return graph;
}

} // namespace rme::procedural
