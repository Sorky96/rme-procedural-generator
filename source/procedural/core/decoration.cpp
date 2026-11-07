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
constexpr uint32_t forestThreshold = 36000;  // of 65536; higher values are dense forest
constexpr uint32_t forestTreePermille = 110;
constexpr uint32_t clearingTreePermille = 6;
constexpr uint32_t bushPermille = 40;
constexpr uint32_t waterBushPermille = 60;
constexpr uint32_t dirtStonePermille = 30;
constexpr uint32_t rockStonePermille = 80;
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
	if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
	if (!grid.valid()) return std::unexpected(SurfaceError::InvalidRegion);
	try {
		const auto nearWater = grow(grid, isWater, waterReach);
		const auto nearRock = grow(grid, [](SurfaceKind kind) { return kind == SurfaceKind::Rock; }, rockReach);
		std::vector<DecorKind> cells(grid.cells.size(), DecorKind::None);
		const uint32_t width = grid.region.width;
		for (size_t i = 0; i < cells.size(); ++i) {
			if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
			const auto kind = grid.cells[i];
			if (kind != SurfaceKind::Grass && kind != SurfaceKind::Dirt) continue;
			const uint32_t ax = uint32_t(grid.region.x) + uint32_t(i % width);
			const uint32_t ay = uint32_t(grid.region.y) + uint32_t(i / width);
			const uint32_t treeRoll = permille(surfaceLattice(seed, treeStream, ax, ay));
			const uint32_t detailRoll = permille(surfaceLattice(seed, detailStream, ax, ay));
			if (kind == SurfaceKind::Grass) {
				const bool forest = surfaceValueNoise(seed, forestStream, ax, ay, forestStep) >= forestThreshold;
				const uint32_t treeChance = nearWater[i] ? 0 : (forest ? forestTreePermille : clearingTreePermille);
				const uint32_t bushChance = nearWater[i] ? waterBushPermille : bushPermille;
				if (treeRoll < treeChance) cells[i] = DecorKind::Tree;
				else if (detailRoll < bushChance) cells[i] = DecorKind::Bush;
				else if (nearRock[i] && detailRoll < bushChance + rockStonePermille) cells[i] = DecorKind::Stone;
			} else if (detailRoll < (nearRock[i] ? rockStonePermille : dirtStonePermille)) {
				cells[i] = DecorKind::Stone;
			}
		}
		return DecorGrid { grid.region, std::move(cells) };
	} catch (const std::bad_alloc&) {
		return std::unexpected(SurfaceError::AllocationFailed);
	}
}

} // namespace rme::procedural
