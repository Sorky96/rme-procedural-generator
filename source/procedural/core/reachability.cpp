// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/reachability.h"

#include <algorithm>
#include <cstdlib>
#include <new>
#include <numeric>
#include <tuple>
#include <vector>

namespace rme::procedural {

namespace {

constexpr uint8_t flagDoor = 1;
constexpr uint8_t flagBuildingSeen = 2;

bool naturallyWalkable(SurfaceKind kind) noexcept {
	return kind == SurfaceKind::Grass || kind == SurfaceKind::Dirt || kind == SurfaceKind::Rock || kind == SurfaceKind::Shore ||
		kind == SurfaceKind::Path || kind == SurfaceKind::Bridge;
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

std::vector<uint32_t> openingsOf(const SurfaceGrid& grid) {
	std::vector<uint32_t> openings = grid.doors;
	openings.insert(openings.end(), grid.gaps.begin(), grid.gaps.end());
	return openings;
}

std::optional<uint32_t> fixedStartOf(const WalkableLabels& labels, uint32_t widthIn, uint32_t heightIn) {
	const int64_t width = widthIn, height = heightIn;
	std::optional<uint32_t> start;
	int64_t bestDistance = 0;
	for (size_t i = 0; i < labels.label.size(); ++i) {
		if (!labels.label[i]) continue;
		const int64_t dx = 2 * int64_t(i % size_t(width)) - (width - 1), dy = 2 * int64_t(i / size_t(width)) - (height - 1);
		const int64_t distance = dx * dx + dy * dy;
		if (!start || distance < bestDistance) { start = uint32_t(i); bestDistance = distance; }
	}
	return start;
}

std::vector<uint32_t> exitCellsOf(const WalkableLabels& labels, uint32_t widthIn, uint32_t heightIn) {
	const int64_t width = widthIn, height = heightIn;
	std::vector<uint32_t> cells;
	// One exit per side: scan the edge in index order and keep the cell nearest to the middle of the side.
	for (int side = 0; side < 4; ++side) {
		int64_t best = -1, bestOffset = 0;
		const int64_t length = side < 2 ? width : height;
		for (int64_t k = 0; k < length; ++k) {
			const int64_t x = side == 0 ? k : side == 1 ? k : side == 2 ? 0 : width - 1;
			const int64_t y = side == 0 ? 0 : side == 1 ? height - 1 : k;
			const size_t cell = size_t(y * width + x);
			if (!labels.label[cell]) continue;
			const int64_t offset = std::abs(2 * k - (length - 1));
			if (best < 0 || offset < bestOffset) { best = int64_t(cell); bestOffset = offset; }
		}
		if (best >= 0) cells.push_back(uint32_t(best));
	}
	return cells;
}

std::expected<PoiReport, ReachabilityError> poiReachabilityOf(const SurfaceGrid& grid, bool walls, std::stop_token stop) {
	if (!grid.valid()) return std::unexpected(ReachabilityError::InvalidGrid);
	const int64_t width = grid.region.width, height = grid.region.height;
	const std::vector<uint32_t> openings = openingsOf(grid);
	auto labels = walkableLabelsOf(grid.cells, grid.region.width, grid.region.height, openings, walls, stop);
	if (!labels) return std::unexpected(labels.error());
	PoiReport report;
	for (size_t id = 1; id < labels->sizes.size(); ++id) report.walkable += labels->sizes[id];

	if (const auto start = fixedStartOf(*labels, grid.region.width, grid.region.height)) { report.hasStart = true; report.start = *start; }
	if (!report.hasStart) {
		report.entrances = uint32_t(grid.doors.size());
		report.sites = uint32_t(grid.sites.size());
		return report;
	}
	const uint32_t startLabel = labels->label[report.start];
	report.startComponent = labels->sizes[startLabel];
	for (const uint32_t door : grid.doors) {
		++report.entrances;
		report.entrancesReached += labels->label[door] == startLabel;
	}
	for (const uint32_t site : grid.sites) {
		++report.sites;
		report.sitesReached += labels->label[site] == startLabel;
	}
	for (const uint32_t cell : exitCellsOf(*labels, grid.region.width, grid.region.height)) {
		++report.exits;
		report.exitsReached += labels->label[cell] == startLabel;
	}
	return report;
}

std::expected<ReachabilityReport, ReachabilityError> reachabilityOf(const SurfaceGrid& grid, bool walls, std::stop_token stop) {
	if (!grid.valid()) return std::unexpected(ReachabilityError::InvalidGrid);
	const size_t width = grid.region.width, height = grid.region.height, count = grid.cells.size();
	ReachabilityReport report;
	report.cells = count;

	const std::vector<uint32_t> openings = openingsOf(grid);
	auto labels = walkableLabelsOf(grid.cells, grid.region.width, grid.region.height, openings, walls, stop);
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
		for (const uint32_t opening : openings) flags[opening] |= flagDoor;
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

bool LevelReachabilityReport::allReached() const noexcept {
	return start && std::ranges::all_of(categories, [](const auto& c) { return c.reached == c.total; });
}

std::expected<LevelPlan, ReachabilityError> levelPlanOf(
	const SurfaceGrid& surface, bool walls, bool rockBlocks, const CaveGrid* cave,
	const CaveExitPlan& exits, std::span<const ExitShape> caveShapes,
	std::span<const PlateauGrid> caps, std::span<const DecorGrid> decor, std::stop_token stop) try {
	if (!surface.valid() || caps.size() > 3) return std::unexpected(ReachabilityError::InvalidGrid);
	const auto n = surface.cells.size();
	const auto w = surface.region.width;
	auto labels = walkableLabelsOf(surface.cells, w, surface.region.height, openingsOf(surface), walls, stop);
	if (!labels) return std::unexpected(labels.error());
	LevelPlan p {surface.region};
	p.floors.push_back({surface.region.z, std::vector<uint8_t>(n)});
	for (size_t i = 0; i < n; ++i) p.floors[0].walkable[i] = labels->label[i] && !(rockBlocks && surface.cells[i] == SurfaceKind::Rock);
	for (const auto i : surface.doors) p.pois.push_back({LevelPoiKind::Door, {0, i}});
	for (const auto i : surface.sites) p.pois.push_back({LevelPoiKind::Site, {0, i}});
	const auto matches = [&](const GenerationRegion& r, int z) {
		auto expected = surface.region; expected.z = z; return expected == r;
	};
	const auto shifted = [&](uint32_t cell, int dx, int dy) -> std::optional<uint32_t> {
		const int64_t x = int64_t(cell % w) + dx, y = int64_t(cell / w) + dy;
		if (cell >= n || x < 0 || y < 0 || x >= w || y >= surface.region.height) return {};
		return uint32_t(y * w + x);
	};
	constexpr int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
	if (cave) {
		if (!cave->valid() || !matches(cave->region, surface.region.z + 1)) return std::unexpected(ReachabilityError::InvalidGrid);
		p.floors.push_back({cave->region.z, std::vector<uint8_t>(n)});
		for (uint32_t i = 0; i < n; ++i) if (cave->cells[i] == CaveKind::Floor) {
			p.floors[1].walkable[i] = 1;
			p.pois.push_back({LevelPoiKind::CaveCell, {1, i}});
		}
		for (const auto& e : exits.exits) {
			if ((!e.distant && e.entrance >= surface.entrances.size()) || e.cell >= n || e.shape >= caveShapes.size()) return std::unexpected(ReachabilityError::InvalidGrid);
			const auto d = size_t(caveShapes[e.shape].direction);
			if (d >= 4) return std::unexpected(ReachabilityError::InvalidGrid);
			const auto arrival = shifted(e.cell, dx[d], dy[d]);
			if (!arrival) return std::unexpected(ReachabilityError::InvalidGrid);
            if (!e.distant) {
                const auto entry = surface.entrances[e.entrance];
                p.edges.push_back({{0, entry}, {1, entry}});
                p.pois.push_back({LevelPoiKind::CaveEntrance, {0, entry}});
            }
			p.edges.push_back({{1, e.cell}, {0, *arrival}});
			p.pois.push_back({LevelPoiKind::CaveExit, {1, e.cell}});
		}
        for (const auto& c : cave->corridors) {
            if (c.door >= n || cave->cells[c.door] != CaveKind::Floor) return std::unexpected(ReachabilityError::InvalidGrid);
            p.pois.push_back({LevelPoiKind::Door, {1, c.door}});
        }
	} else if (!exits.exits.empty()) return std::unexpected(ReachabilityError::InvalidGrid);
	for (size_t k = 0; k < caps.size(); ++k) {
		const auto& cap = caps[k];
		if (!matches(cap.region, surface.region.z - int(k + 1)) || cap.cells.size() != n || cap.landings.size() != n)
			return std::unexpected(ReachabilityError::InvalidGrid);
		const uint32_t upper = uint32_t(p.floors.size()), lower = k ? upper - 1 : 0;
		p.floors.push_back({cap.region.z, std::vector<uint8_t>(n)});
		for (uint32_t i = 0; i < n; ++i) {
			p.floors[upper].walkable[i] = cap.cells[i] == PlateauKind::Top || cap.landings[i];
			if (cap.cells[i] == PlateauKind::Top) p.floors[lower].walkable[i] = 0;
		}
		for (const auto& r : cap.ramps) {
			if (r.cell >= n || r.destination >= n) return std::unexpected(ReachabilityError::InvalidGrid);
			p.floors[upper].walkable[r.cell] = 1; // floorchange-down opening
			p.edges.push_back({{lower, r.cell}, {upper, r.destination}});
		}
	}
	for (size_t f = cave ? 2 : 1; f < p.floors.size(); ++f)
		for (uint32_t i = 0; i < n; ++i) if (p.floors[f].walkable[i]) p.pois.push_back({LevelPoiKind::Cap, {uint32_t(f), i}});
	if (!decor.empty()) {
		if (decor.size() != p.floors.size()) return std::unexpected(ReachabilityError::InvalidGrid);
		for (size_t f = 0; f < decor.size(); ++f) {
			if (decor[f].cells.empty()) continue;
			if (decor[f].cells.size() != n || !matches(decor[f].region, p.floors[f].z)) return std::unexpected(ReachabilityError::InvalidGrid);
			p.floors[f].decor = decor[f].cells;
		}
	}
	return p;
} catch (const std::bad_alloc&) { return std::unexpected(ReachabilityError::AllocationFailed); }

std::expected<LevelReachabilityReport, ReachabilityError> levelReachabilityOf(
	const LevelPlan& p, std::array<bool, decorKindCount> blockingClasses,
	std::span<const uint8_t> mask, std::stop_token stop) try {
	const size_t n = size_t(p.region.width) * p.region.height, floors = p.floors.size();
	if (!n || !floors || floors > 5 || n > UINT32_MAX / floors || p.floors[0].z != p.region.z ||
		(!mask.empty() && mask.size() != n * floors)) return std::unexpected(ReachabilityError::InvalidGrid);
	LevelReachabilityReport r;
	r.walkable.resize(n * floors); r.reached.resize(n * floors); r.pockets.resize(floors);
	for (size_t f = 0; f < floors; ++f) {
		if (p.floors[f].walkable.size() != n || (!p.floors[f].decor.empty() && p.floors[f].decor.size() != n))
			return std::unexpected(ReachabilityError::InvalidGrid);
		for (size_t other = 0; other < f; ++other) if (p.floors[other].z == p.floors[f].z) return std::unexpected(ReachabilityError::InvalidGrid);
		for (size_t i = 0; i < n; ++i) {
			if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
			const auto kind = p.floors[f].decor.empty() ? 0 : size_t(p.floors[f].decor[i]);
			if (kind > decorKindCount) return std::unexpected(ReachabilityError::InvalidGrid);
			const bool blocks = mask.empty() ? kind && blockingClasses[kind - 1] : mask[f * n + i] != 0;
			r.walkable[f * n + i] = p.floors[f].walkable[i] && !blocks;
		}
	}
	const auto validCell = [&](LevelCell c) { return c.floor < floors && c.cell < n; };
	const auto index = [&](LevelCell c) { return uint32_t(c.floor * n + c.cell); };
	// CSR adjacency for sparse Z edges: no allocation or edge-list scan per visited cell.
	std::vector<uint32_t> offsets(n * floors + 1), neighbours(p.edges.size() * 2);
	for (const auto& e : p.edges) {
		if (!validCell(e.a) || !validCell(e.b) || std::abs(int64_t(p.floors[e.a.floor].z) - p.floors[e.b.floor].z) != 1)
			return std::unexpected(ReachabilityError::InvalidGrid);
		++offsets[index(e.a) + 1]; ++offsets[index(e.b) + 1];
		++r.edges; r.walkableEdges += r.walkable[index(e.a)] && r.walkable[index(e.b)];
	}
	std::partial_sum(offsets.begin(), offsets.end(), offsets.begin());
	auto cursor = offsets;
	for (const auto& e : p.edges) {
		neighbours[cursor[index(e.a)]++] = index(e.b);
		neighbours[cursor[index(e.b)]++] = index(e.a);
	}
	WalkableLabels startLabels;
	startLabels.label.assign(r.walkable.begin(), r.walkable.begin() + n);
	r.start = fixedStartOf(startLabels, p.region.width, p.region.height);
	std::vector<uint32_t> q;
	q.reserve(n * floors);
	const auto adjacent = [&](uint32_t i, auto visit) {
		const auto cell = i % n, x = cell % p.region.width, y = cell / p.region.width;
		if (x) visit(i - 1);
		if (x + 1 < p.region.width) visit(i + 1);
		if (y) visit(i - p.region.width);
		if (y + 1 < p.region.height) visit(i + p.region.width);
	};
	if (r.start) { r.reached[*r.start] = 1; q.push_back(*r.start); }
	for (size_t h = 0; h < q.size(); ++h) {
		if (h % 4096 == 0 && stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
		const auto visit = [&](uint32_t i) { if (r.walkable[i] && !r.reached[i]) { r.reached[i] = 1; q.push_back(i); } };
		adjacent(q[h], visit);
		for (auto j = offsets[q[h]]; j < offsets[q[h] + 1]; ++j) visit(neighbours[j]);
	}
	auto seen = r.reached;
	for (uint32_t i = 0; i < seen.size(); ++i) if (r.walkable[i] && !seen[i]) {
		++r.pockets[i / n]; q.clear(); q.push_back(i); seen[i] = 1;
		for (size_t h = 0; h < q.size(); ++h) {
			if (h % 4096 == 0 && stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
			adjacent(q[h], [&](uint32_t j) { if (r.walkable[j] && !seen[j]) { seen[j] = 1; q.push_back(j); } });
		}
	}
	std::vector<LevelPoi> sorted = p.pois;
	for (const auto& poi : sorted)
		if (!validCell(poi.at) || size_t(poi.kind) >= levelPoiKindCount) return std::unexpected(ReachabilityError::InvalidGrid);
	std::ranges::sort(sorted, [&](const auto& a, const auto& b) {
		return std::tuple{a.kind, p.floors[a.at.floor].z, a.at.cell} < std::tuple{b.kind, p.floors[b.at.floor].z, b.at.cell};
	});
	for (const auto& poi : sorted) {
		// Caps count walkable terrace cells; every cave Floor remains a target,
		// including one obstructed by a resolved client item.
		if (poi.kind == LevelPoiKind::Cap && !r.walkable[index(poi.at)]) continue;
		auto& c = r.categories[size_t(poi.kind)]; ++c.total; c.reached += r.reached[index(poi.at)] != 0;
		if (!r.reached[index(poi.at)] && r.firstUnreachable.size() < 16)
			r.firstUnreachable.push_back({poi.kind, p.region.x + int32_t(poi.at.cell % p.region.width),
				p.region.y + int32_t(poi.at.cell / p.region.width), p.floors[poi.at.floor].z});
	}
	return r;
} catch (const std::bad_alloc&) { return std::unexpected(ReachabilityError::AllocationFailed); }

} // namespace rme::procedural
