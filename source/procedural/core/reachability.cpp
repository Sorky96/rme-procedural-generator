// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/reachability.h"

#include <algorithm>
#include <new>
#include <vector>

namespace rme::procedural {

namespace {

constexpr uint8_t flagDoor = 1;
constexpr uint8_t flagBuildingSeen = 2;

bool naturallyWalkable(SurfaceKind kind) noexcept {
	return kind == SurfaceKind::Grass || kind == SurfaceKind::Dirt || kind == SurfaceKind::Rock || kind == SurfaceKind::Shore ||
		kind == SurfaceKind::Path;
}

} // namespace

std::expected<WalkableLabels, ReachabilityError> walkableLabelsOf(
	std::span<const SurfaceKind> cells, uint32_t widthIn, uint32_t heightIn, std::span<const uint32_t> doors, bool walls, std::stop_token stop) {
	const size_t width = widthIn, height = heightIn, count = cells.size();
	if (!width || !height || width * height != count) return std::unexpected(ReachabilityError::InvalidGrid);
	try {
		std::vector<uint8_t> isDoor(walls ? count : 0, 0);
		for (const uint32_t door : doors) {
			if (door >= count || cells[door] != SurfaceKind::Floor) return std::unexpected(ReachabilityError::InvalidGrid);
			if (walls) isDoor[door] = 1;
		}
		auto isOutline = [&](size_t i) {
			const size_t x = i % width, y = i / width;
			return x == 0 || y == 0 || x + 1 == width || y + 1 == height || cells[i - 1] != SurfaceKind::Floor ||
				cells[i + 1] != SurfaceKind::Floor || cells[i - width] != SurfaceKind::Floor || cells[i + width] != SurfaceKind::Floor;
		};
		auto walkable = [&](size_t i) {
			const SurfaceKind kind = cells[i];
			if (kind == SurfaceKind::Floor) return !walls || isDoor[i] || !isOutline(i);
			return naturallyWalkable(kind);
		};

		WalkableLabels result;
		result.label.assign(count, 0);
		result.sizes.push_back(0);
		std::vector<uint32_t> queue;
		queue.reserve(count);
		auto spread = [&](size_t start, uint32_t id) {
			uint64_t size = 0;
			queue.clear();
			queue.push_back(uint32_t(start));
			result.label[start] = id;
			for (size_t head = 0; head < queue.size(); ++head) {
				const size_t i = queue[head];
				++size;
				const size_t x = i % width, y = i / width;
				const size_t next[4] = {x > 0 ? i - 1 : count, x + 1 < width ? i + 1 : count, y > 0 ? i - width : count, y + 1 < height ? i + width : count};
				for (const size_t n : next) {
					if (n == count || result.label[n] || !walkable(n)) continue;
					result.label[n] = id;
					queue.push_back(uint32_t(n));
				}
			}
			return size;
		};
		for (size_t y = 0; y < height; ++y) {
			if (stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
			for (size_t x = 0; x < width; ++x) {
				const size_t i = y * width + x;
				if (result.label[i] || !walkable(i)) continue;
				result.sizes.push_back(spread(i, uint32_t(result.sizes.size())));
			}
		}
		uint64_t best = 0;
		for (uint32_t id = 1; id < result.sizes.size(); ++id) {
			if (result.sizes[id] > best) { best = result.sizes[id]; result.largest = id; }
		}
		return result;
	} catch (const std::bad_alloc&) {
		return std::unexpected(ReachabilityError::AllocationFailed);
	}
}

std::expected<ReachabilityReport, ReachabilityError> reachabilityOf(const SurfaceGrid& grid, bool walls, std::stop_token stop) {
	if (!grid.valid()) return std::unexpected(ReachabilityError::InvalidGrid);
	const size_t width = grid.region.width, height = grid.region.height, count = grid.cells.size();
	ReachabilityReport report;
	report.cells = count;

	auto labels = walkableLabelsOf(grid.cells, grid.region.width, grid.region.height, grid.doors, walls, stop);
	if (!labels) return std::unexpected(labels.error());
	const uint32_t largestLabel = labels->largest;
	for (uint32_t id = 1; id < labels->sizes.size(); ++id) {
		++report.components;
		report.walkable += labels->sizes[id];
		if (id != largestLabel && labels->sizes[id] >= reachabilityPocketCells) ++report.pockets;
	}
	report.largest = largestLabel ? labels->sizes[largestLabel] : 0;
	report.cutOff = report.walkable - report.largest;

	try {
		// Buildings: 4-connected Floor components. With walls a building needs a door cell in the largest component;
		// without walls any of its cells there is enough.
		std::vector<uint8_t> flags(count, 0);
		for (const uint32_t door : grid.doors) flags[door] |= flagDoor;
		std::vector<uint32_t> queue;
		queue.reserve(count);
		for (size_t y = 0; y < height; ++y) {
			if (stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
			for (size_t x = 0; x < width; ++x) {
				const size_t start = y * width + x;
				if (grid.cells[start] != SurfaceKind::Floor || (flags[start] & flagBuildingSeen)) continue;
				++report.buildings;
				bool reachable = false;
				queue.clear();
				queue.push_back(uint32_t(start));
				flags[start] |= flagBuildingSeen;
				for (size_t head = 0; head < queue.size(); ++head) {
					const size_t i = queue[head];
					if (labels->label[i] == largestLabel && largestLabel && (!walls || (flags[i] & flagDoor))) reachable = true;
					const size_t ix = i % width, iy = i / width;
					const size_t next[4] = {ix > 0 ? i - 1 : count, ix + 1 < width ? i + 1 : count, iy > 0 ? i - width : count, iy + 1 < height ? i + width : count};
					for (const size_t n : next) {
						if (n == count || grid.cells[n] != SurfaceKind::Floor || (flags[n] & flagBuildingSeen)) continue;
						flags[n] |= flagBuildingSeen;
						queue.push_back(uint32_t(n));
					}
				}
				if (!reachable) ++report.sealed;
			}
		}
	} catch (const std::bad_alloc&) {
		return std::unexpected(ReachabilityError::AllocationFailed);
	}
	return report;
}

} // namespace rme::procedural
