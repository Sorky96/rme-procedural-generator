// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/decoration.h"

#include <algorithm>
#include <new>

namespace rme::procedural {

namespace {

constexpr uint64_t forestStream = 0x666f726573745f31ULL; // "forest_1"
constexpr uint64_t treeStream = 0x747265655f726f6cULL;   // "tree_rol"
constexpr uint64_t detailStream = 0x64657461696c5f31ULL; // "detail_1"
constexpr uint32_t forestStep = 20;
constexpr uint64_t clumpStream = 0x636c756d705f3120ULL;  // "clump_1 "
constexpr uint32_t clumpStep = 12;
// Chance multiplier in permille from clump noise: 0 below 30% of the range, rising to 2000 above 70%.
// Symmetric around the middle, so it keeps the average chance for noise centred on the middle.
uint32_t clumpMultiplier(uint32_t noise) {
	constexpr uint32_t low = 65536 * 3 / 10, span = 65536 * 4 / 10;
	const uint32_t t = noise <= low ? 0 : std::min<uint32_t>(1000, (noise - low) * 1000 / span);
	return 2 * t;
}
constexpr uint32_t forestThreshold = 36000;  // of 65536; higher values are dense forest
constexpr uint32_t forestTreePermille = 110;
constexpr uint32_t clearingTreePermille = 6;
constexpr uint32_t waterReach = 2;
constexpr uint32_t rockReach = 2;

uint32_t permille(uint16_t value) { return uint32_t(value) * 1000 >> 16; }

bool isWater(SurfaceKind kind) { return kind == SurfaceKind::Lake || kind == SurfaceKind::Shore || kind == SurfaceKind::River; }

// Marks cells within `reach` 4-neighbour steps of a seed cell.
std::vector<uint8_t> grow(const SurfaceGrid& grid, bool (*isSeed)(SurfaceKind), uint32_t reach) {
	const uint32_t width = grid.region.width, height = grid.region.height;
	std::vector<uint8_t> mark(grid.cells.size(), 0);
	for (size_t i = 0; i < mark.size(); ++i) mark[i] = isSeed(grid.cells[i]);
	for (uint32_t step = 0; step < reach; ++step) {
		const auto previous = mark;
		for (size_t i = 0; i < mark.size(); ++i) {
			if (mark[i]) continue;
			const uint32_t x = uint32_t(i % width), y = uint32_t(i / width);
			mark[i] = (x > 0 && previous[i - 1]) || (x + 1 < width && previous[i + 1]) ||
				(y > 0 && previous[i - width]) || (y + 1 < height && previous[i + width]);
		}
	}
	return mark;
}

} // namespace

bool DecorGrid::valid() const noexcept {
	return region.width != 0 && region.height != 0 && uint64_t(region.width) * region.height == cells.size() &&
		std::ranges::all_of(cells, [](DecorKind kind) { return kind <= DecorKind::Stone; });
}

std::expected<DecorGrid, SurfaceError> generateDecoration(const SurfaceGrid& grid, uint64_t seed, std::stop_token stop) {
	return generateDecoration(grid, seed, DecorTuning {}, stop);
}

std::expected<DecorGrid, SurfaceError> generateDecoration(
	const SurfaceGrid& grid, uint64_t seed, const DecorTuning& tuning, std::stop_token stop) {
	if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
	if (!grid.valid()) return std::unexpected(SurfaceError::InvalidRegion);
	if (!valid(tuning)) return std::unexpected(SurfaceError::InvalidParameters);
	const uint32_t bushPermille = tuning.bushPermille, waterBushPermille = tuning.waterBushPermille;
	const uint32_t dirtStonePermille = tuning.dirtStonePermille, rockStonePermille = tuning.rockStonePermille;
	try {
		const auto nearWater = grow(grid, isWater, waterReach);
		const auto nearRock = grow(grid, [](SurfaceKind kind) { return kind == SurfaceKind::Rock; }, rockReach);
		std::vector<DecorKind> cells(grid.cells.size(), DecorKind::None);
		const uint32_t width = grid.region.width;
		for (size_t i = 0; i < cells.size(); ++i) {
			if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
			const auto kind = grid.cells[i];
			const uint32_t ax = uint32_t(grid.region.x) + uint32_t(i % width);
			const uint32_t ay = uint32_t(grid.region.y) + uint32_t(i / width);
			if (tuning.shoreTreePermille && (kind == SurfaceKind::Lake || kind == SurfaceKind::River)) {
				// A water cell with a 4-neighbour of land may hold a tree standing in the water.
				const uint32_t x = uint32_t(i % width), y = uint32_t(i / width), height = grid.region.height;
				const auto land = [&](size_t j) { const auto k = grid.cells[j]; return k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Shore; };
				const bool bank = (x && land(i - 1)) || (x + 1 < width && land(i + 1)) || (y && land(i - width)) || (y + 1 < height && land(i + width));
				if (bank && permille(surfaceLattice(seed, treeStream, ax, ay)) < tuning.shoreTreePermille) cells[i] = DecorKind::Tree;
				continue;
			}
			if (kind != SurfaceKind::Grass && kind != SurfaceKind::Dirt) continue;
			const uint32_t treeRoll = permille(surfaceLattice(seed, treeStream, ax, ay));
			const uint32_t detailRoll = permille(surfaceLattice(seed, detailStream, ax, ay));
			if (kind == SurfaceKind::Grass) {
				const bool forest = surfaceValueNoise(seed, forestStream, ax, ay, forestStep) >= forestThreshold;
				const uint32_t clump = tuning.clumpPermille ? clumpMultiplier(surfaceValueNoise(seed, clumpStream, ax, ay, clumpStep)) : 1000;
				const auto clumped = [&](uint64_t chance) {
					const uint64_t scale = uint64_t(1000 - tuning.clumpPermille) * 1000 + uint64_t(tuning.clumpPermille) * clump;
					return uint32_t(std::min<uint64_t>(1000, chance * scale / 1000000));
				};
				const uint32_t treeChance = clumped(nearWater[i] ? tuning.waterTreePermille
					: std::min<uint32_t>(1000, (forest ? forestTreePermille : clearingTreePermille) * tuning.treeScalePermille / 1000));
				const uint32_t bushChance = clumped(nearWater[i] ? waterBushPermille : bushPermille);
				if (treeRoll < treeChance) cells[i] = DecorKind::Tree;
				else if (detailRoll < bushChance) cells[i] = DecorKind::Bush;
				else if (nearRock[i] && detailRoll < bushChance + rockStonePermille) cells[i] = DecorKind::Stone;
			} else if (detailRoll < (nearRock[i] ? rockStonePermille : dirtStonePermille)) {
				cells[i] = DecorKind::Stone;
			}
		}
		for (const auto e : grid.entrances) cells[e] = DecorKind::None;
		return DecorGrid { grid.region, std::move(cells) };
	} catch (const std::bad_alloc&) {
		return std::unexpected(SurfaceError::AllocationFailed);
	}
}

std::expected<uint32_t, SurfaceError> preserveDecorationConnectivity(
    DecorGrid& decor, std::span<const uint8_t> open, std::array<bool, decorKindCount> blockingClasses,
    std::span<uint8_t> mask, std::stop_token stop) try {
    if (!decor.valid() || open.size() != decor.cells.size() || (!mask.empty() && mask.size() != open.size()))
        return std::unexpected(SurfaceError::InvalidRegion);
    const auto w = decor.region.width, h = decor.region.height;
    std::vector<uint8_t> walkable(open.begin(), open.end());
    if (!mask.empty()) for (size_t i = 0; i < mask.size(); ++i) {
        if (mask[i] > 2) return std::unexpected(SurfaceError::InvalidParameters);
        if (mask[i] == 2) walkable[i] = 0;
    }
    std::vector<uint32_t> seen(open.size()), queue;
    const auto neighbours = [&](uint32_t i, auto visit) {
        if (i >= w) visit(i - w);
        if (i % w) visit(i - 1);
        if (i % w + 1 < w) visit(i + 1);
        if (i / w + 1 < h) visit(i + w);
    };
    uint32_t removed = 0;
    for (uint32_t i = 0; i < open.size(); ++i) {
        if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
        const auto kind = size_t(decor.cells[i]);
        const bool blocks = mask.empty() ? kind && blockingClasses[kind - 1] : mask[i] == 1;
        if (!walkable[i] || !blocks) continue;
        std::array<uint32_t, 4> adjacent {};
        size_t count = 0;
        neighbours(i, [&](uint32_t j) { if (walkable[j]) adjacent[count++] = j; });
        walkable[i] = 0;
        bool splits = false;
        if (count > 1) {
            // All neighbours shared a component through i. Its removal splits
            // exactly when they cannot all reach one another without i.
            queue.clear(); queue.push_back(adjacent[0]); seen[adjacent[0]] = i + 1;
            size_t remaining = count - 1;
            for (size_t head = 0; head < queue.size() && remaining; ++head) {
                if (head % 4096 == 0 && stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
                neighbours(queue[head], [&](uint32_t j) {
                    if (!walkable[j] || seen[j] == i + 1) return;
                    seen[j] = i + 1; queue.push_back(j);
                    for (size_t a = 1; a < count; ++a) if (j == adjacent[a]) --remaining;
                });
            }
            splits = remaining != 0;
        }
        if (splits) {
            walkable[i] = 1;
            decor.cells[i] = DecorKind::None;
            if (!mask.empty()) mask[i] = 0;
            ++removed;
        }
    }
    return removed;
} catch (const std::bad_alloc&) { return std::unexpected(SurfaceError::AllocationFailed); }

} // namespace rme::procedural
