// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/grass_patches.h"

#include <algorithm>

namespace rme::procedural {
namespace {
uint64_t mixHash(uint64_t value) noexcept {
	value += 0x9e3779b97f4a7c15ULL;
	value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
	value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
	return value ^ (value >> 31);
}
} // namespace

uint32_t grassPatchValue(uint64_t seed, int64_t x, int64_t y, uint32_t step) noexcept {
	const auto corner = [&](int64_t cx, int64_t cy) {
		return uint32_t(mixHash(seed ^ mixHash(uint64_t(cx) * 0x100000001b3ULL ^ uint64_t(cy))) & 0xffff);
	};
	const int64_t s = std::max<int64_t>(1, step);
	const int64_t cx = x >= 0 ? x / s : (x - s + 1) / s, cy = y >= 0 ? y / s : (y - s + 1) / s;
	const uint64_t fx = uint64_t(x - cx * s), fy = uint64_t(y - cy * s);
	const uint64_t top = corner(cx, cy) * (s - fx) + corner(cx + 1, cy) * fx;
	const uint64_t bottom = corner(cx, cy + 1) * (s - fx) + corner(cx + 1, cy + 1) * fx;
	return uint32_t((top * (s - fy) + bottom * fy) / uint64_t(s * s));
}

std::vector<uint8_t> assignGrassPatches(const SurfaceGrid& grid, uint64_t seed,
	std::span<const uint16_t, grassPatchKinds> permille, uint32_t step, std::stop_token stop) {
	if (std::ranges::all_of(permille, [](uint16_t p) { return p == 0; })) return {};
	std::vector<uint8_t> slot(grid.cells.size(), 0);
	const size_t width = grid.region.width, height = grid.region.height;
	const size_t grassCells = size_t(std::ranges::count(grid.cells, SurfaceKind::Grass));
	std::vector<std::pair<uint32_t, uint32_t>> values; // patch value, cell
	for (size_t m = 0; m < grassPatchKinds; ++m) {
		if (!permille[m]) continue;
		if (stop.stop_requested()) return {};
		values.clear();
		for (size_t i = 0; i < grid.cells.size(); ++i) {
			if (grid.cells[i] != SurfaceKind::Grass || slot[i]) continue;
			values.emplace_back(grassPatchValue(seed ^ grassPatchStreams[m], grid.region.x + int64_t(i % width),
				grid.region.y + int64_t(i / width), step), uint32_t(i));
		}
		const size_t wanted = std::min(values.size(), grassCells * permille[m] / 1000);
		std::ranges::nth_element(values, values.begin() + std::ptrdiff_t(wanted));
		for (size_t k = 0; k < wanted; ++k) slot[values[k].second] = uint8_t(m + 1);
	}
	std::vector<uint8_t> next;
	for (int pass = 0; pass < 2; ++pass) {
		next = slot;
		for (size_t i = 0; i < grid.cells.size(); ++i) {
			if (grid.cells[i] != SurfaceKind::Grass) continue;
			const size_t x = i % width, y = i / width;
			std::array<uint8_t, grassPatchKinds + 1> around {};
			for (const auto [ok, j] : {std::pair{x > 0, i - 1}, std::pair{x + 1 < width, i + 1},
					std::pair{y > 0, i - width}, std::pair{y + 1 < height, i + width}}) {
				if (ok && grid.cells[j] == SurfaceKind::Grass) ++around[slot[j]];
			}
			if (around[slot[i]] >= 2) continue;
			const auto best = std::ranges::max_element(around);
			if (*best >= 2) next[i] = uint8_t(best - around.begin());
		}
		slot.swap(next);
	}
	return slot;
}

} // namespace rme::procedural
