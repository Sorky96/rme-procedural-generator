// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/scale_stats.h"

#include <algorithm>
#include <new>
#include <unordered_map>

namespace rme::procedural {

namespace {

// Calls fn(kinds, width, height) for every floor zMin..zMax with natural kinds 0..4 and 255 for everything else.
template <typename Fn>
std::expected<void, StatsError> forEachKindFloor(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop, Fn&& fn) {
	if (zMin > zMax) return std::unexpected(StatsError::InvalidRange);
	try {
		std::unordered_map<std::string_view, StyleKind> byKey;
		for (const auto& e : keymap) byKey.emplace(e.key, e.kind);
		const auto& names = map.groundKeys();
		std::vector<uint8_t> kindOfKey(names.size(), 255);
		for (size_t i = 0; i < names.size(); ++i) {
			const auto it = byKey.find(names[i]);
			if (it != byKey.end()) { if (static_cast<size_t>(it->second) < 5) kindOfKey[i] = static_cast<uint8_t>(it->second); }
			else if (i != noGroundKey) return std::unexpected(StatsError::KeyNotMapped);
		}
		std::vector<uint8_t> kinds;
		for (const SemanticFloor& floor : map.floors()) {
			if (floor.bounds.z < zMin || floor.bounds.z > zMax) continue;
			if (stop.stop_requested()) return std::unexpected(StatsError::Cancelled);
			kinds.assign(floor.ground.size(), 255);
			for (size_t i = 0; i < kinds.size(); ++i) if (floor.traits[i] & bit(Trait::Present)) kinds[i] = kindOfKey[floor.ground[i]];
			fn(std::span<const uint8_t>(kinds), static_cast<uint32_t>(floor.bounds.width), static_cast<uint32_t>(floor.bounds.height));
		}
		return {};
	} catch (const std::bad_alloc&) {
		return std::unexpected(StatsError::AllocationFailed);
	}
}

std::vector<uint8_t> kindsOfGrid(const SurfaceGrid& grid) {
	std::vector<uint8_t> kinds(grid.cells.size(), 255);
	for (size_t i = 0; i < kinds.size(); ++i) {
		switch (grid.cells[i]) {
			case SurfaceKind::Grass: kinds[i] = 0; break;
			case SurfaceKind::Dirt: kinds[i] = 1; break;
			case SurfaceKind::Rock: kinds[i] = 2; break;
			case SurfaceKind::Shore: kinds[i] = 3; break;
			case SurfaceKind::Lake: case SurfaceKind::River: kinds[i] = 4; break;
			case SurfaceKind::Preserve: case SurfaceKind::Path: break;
		}
	}
	return kinds;
}

} // namespace

LagStats& LagStats::operator+=(const LagStats& other) noexcept {
	for (size_t i = 0; i < same.size(); ++i) { same[i] += other.same[i]; pairs[i] += other.pairs[i]; }
	for (size_t i = 0; i < cells.size(); ++i) cells[i] += other.cells[i];
	return *this;
}

void addLagStats(std::span<const uint8_t> kinds, uint32_t width, uint32_t height, LagStats& out) {
	if (uint64_t(width) * height != kinds.size()) return;
	for (size_t i = 0; i < kinds.size(); ++i) if (kinds[i] < 5) ++out.cells[kinds[i]];
	for (size_t l = 0; l < scaleLags.size(); ++l) {
		const size_t d = scaleLags[l];
		for (size_t y = 0; y < height; ++y) {
			const uint8_t* row = kinds.data() + y * width;
			for (size_t x = 0; x + d < width; ++x) {
				if (row[x] < 5 && row[x + d] < 5) { ++out.pairs[l]; out.same[l] += row[x] == row[x + d]; }
			}
		}
		for (size_t y = 0; y + d < height; ++y) {
			const uint8_t* row = kinds.data() + y * width;
			const uint8_t* next = kinds.data() + (y + d) * width;
			for (size_t x = 0; x < width; ++x) {
				if (row[x] < 5 && next[x] < 5) { ++out.pairs[l]; out.same[l] += row[x] == next[x]; }
			}
		}
	}
}

std::expected<LagStats, StatsError> lagStatsOf(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop) {
	LagStats total;
	const auto done = forEachKindFloor(map, keymap, zMin, zMax, stop, [&](std::span<const uint8_t> kinds, uint32_t w, uint32_t h) { addLagStats(kinds, w, h, total); });
	if (!done) return std::unexpected(done.error());
	return total;
}

LagStats lagStatsOf(const SurfaceGrid& grid) {
	LagStats stats;
	addLagStats(kindsOfGrid(grid), grid.region.width, grid.region.height, stats);
	return stats;
}

std::array<int32_t, 3> lagExcessPermille(const LagStats& stats) {
	std::array<int32_t, 3> out {};
	uint64_t n = 0, squares = 0;
	for (const uint64_t c : stats.cells) n += c;
	if (n == 0) return out;
	for (const uint64_t c : stats.cells) squares += c * c;
	const int64_t chance = static_cast<int64_t>(squares * 1000 / (n * n));
	for (size_t l = 0; l < out.size(); ++l) {
		if (stats.pairs[l] == 0) continue;
		out[l] = static_cast<int32_t>(static_cast<int64_t>(stats.same[l] * 1000 / stats.pairs[l]) - chance);
	}
	return out;
}

PatchStats& PatchStats::operator+=(const PatchStats& other) noexcept {
	rockCells += other.rockCells; rockPairs += other.rockPairs; rockInLarge += other.rockInLarge; rockDust += other.rockDust;
	dirtCells += other.dirtCells; dirtGrassPairs += other.dirtGrassPairs; dirtInLarge += other.dirtInLarge;
	shoreCells += other.shoreCells; shoreNextToWater += other.shoreNextToWater; shoreNearWater += other.shoreNearWater;
	for (size_t i = 0; i < cells.size(); ++i) cells[i] += other.cells[i];
	return *this;
}

void addPatchStats(std::span<const uint8_t> kinds, uint32_t width, uint32_t height, PatchStats& out) {
	if (uint64_t(width) * height != kinds.size()) return;
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			const size_t i = y * width + x;
			const uint8_t k = kinds[i];
			if (k >= 5) continue;
			++out.cells[k];
			const uint8_t right = x + 1 < width ? kinds[i + 1] : 255, down = y + 1 < height ? kinds[i + width] : 255;
			if (k == 2) {
				++out.rockCells;
				out.rockPairs += (right < 5 && right != 2) + (down < 5 && down != 2);
			} else if (right == 2 && k < 5) {
				++out.rockPairs;
			}
			if (down == 2 && k < 5 && k != 2) ++out.rockPairs;
			if (k == 1) {
				++out.dirtCells;
				out.dirtGrassPairs += (right == 0) + (down == 0);
			} else if (k == 0) {
				out.dirtGrassPairs += (right == 1) + (down == 1);
			} else if (k == 3) {
				++out.shoreCells;
				const uint8_t left = x > 0 ? kinds[i - 1] : 255, up = y > 0 ? kinds[i - width] : 255;
				out.shoreNextToWater += left == 4 || right == 4 || up == 4 || down == 4;
			}
		}
	}
	{
		// Beach: Shore reached from Shore cells next to Water through Shore only, at most beachReach steps.
		std::vector<uint8_t> reached(kinds.size(), 0);
		std::vector<size_t> frontier, next;
		for (size_t i = 0; i < kinds.size(); ++i) {
			if (kinds[i] != 3) continue;
			const size_t x = i % width, y = i / width;
			if ((x > 0 && kinds[i - 1] == 4) || (x + 1 < width && kinds[i + 1] == 4) || (y > 0 && kinds[i - width] == 4) || (y + 1 < height && kinds[i + width] == 4)) {
				reached[i] = 1;
				frontier.push_back(i);
			}
		}
		uint64_t count = frontier.size();
		for (uint32_t step = 1; step < beachReach && !frontier.empty(); ++step) {
			next.clear();
			for (const size_t i : frontier) {
				const size_t x = i % width, y = i / width;
				auto grow = [&](size_t j) { if (kinds[j] == 3 && !reached[j]) { reached[j] = 1; next.push_back(j); } };
				if (x > 0) grow(i - 1);
				if (x + 1 < width) grow(i + 1);
				if (y > 0) grow(i - width);
				if (y + 1 < height) grow(i + width);
			}
			count += next.size();
			frontier.swap(next);
		}
		out.shoreNearWater += count;
	}
	std::vector<uint8_t> seen(kinds.size(), 0);
	std::vector<size_t> stack;
	for (size_t start = 0; start < kinds.size(); ++start) {
		const uint8_t kind = kinds[start];
		if ((kind != 1 && kind != 2) || seen[start]) continue;
		size_t size = 0;
		stack.assign(1, start);
		seen[start] = 1;
		while (!stack.empty()) {
			const size_t i = stack.back();
			stack.pop_back();
			++size;
			const size_t x = i % width, y = i / width;
			auto visit = [&](size_t j) { if (kinds[j] == kind && !seen[j]) { seen[j] = 1; stack.push_back(j); } };
			if (x > 0) visit(i - 1);
			if (x + 1 < width) visit(i + 1);
			if (y > 0) visit(i - width);
			if (y + 1 < height) visit(i + width);
		}
		if (kind == 1) {
			if (size >= dirtLargePatch) out.dirtInLarge += size;
		} else {
			if (size >= rockLargePatch) out.rockInLarge += size;
			if (size < rockDustPatch) out.rockDust += size;
		}
	}
}

std::expected<PatchStats, StatsError> patchStatsOf(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop) {
	PatchStats total;
	const auto done = forEachKindFloor(map, keymap, zMin, zMax, stop, [&](std::span<const uint8_t> kinds, uint32_t w, uint32_t h) { addPatchStats(kinds, w, h, total); });
	if (!done) return std::unexpected(done.error());
	return total;
}

PatchStats patchStatsOf(const SurfaceGrid& grid) {
	PatchStats stats;
	addPatchStats(kindsOfGrid(grid), grid.region.width, grid.region.height, stats);
	return stats;
}

PatchSummary summarizePatches(const PatchStats& stats) {
	PatchSummary s;
	if (stats.dirtCells >= minDirtCells) {
		s.boundary = static_cast<uint32_t>(stats.dirtGrassPairs * 1000 / stats.dirtCells);
		s.compact = static_cast<uint32_t>(stats.dirtInLarge * 1000 / stats.dirtCells);
	}
	if (stats.rockCells >= minRockCellsForStats) {
		s.rockBoundary = static_cast<uint32_t>(stats.rockPairs * 1000 / stats.rockCells);
		s.rockCompact = static_cast<uint32_t>(stats.rockInLarge * 1000 / stats.rockCells);
		s.rockDustShare = static_cast<uint32_t>(stats.rockDust * 1000 / stats.rockCells);
	}
	if (stats.shoreNearWater >= minShoreCells) s.beach = static_cast<uint32_t>(stats.shoreNearWater * 1000 / std::max<uint64_t>(stats.shoreNextToWater, 1));
	uint64_t n = 0;
	for (const uint64_t c : stats.cells) n += c;
	if (n) for (size_t i = 0; i < 5; ++i) s.sharePermille[i] = static_cast<uint32_t>(stats.cells[i] * 1000 / n);
	return s;
}

} // namespace rme::procedural
