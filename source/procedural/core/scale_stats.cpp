// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/scale_stats.h"

#include <new>
#include <unordered_map>

namespace rme::procedural {

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
		LagStats total;
		std::vector<uint8_t> kinds;
		for (const SemanticFloor& floor : map.floors()) {
			if (floor.bounds.z < zMin || floor.bounds.z > zMax) continue;
			if (stop.stop_requested()) return std::unexpected(StatsError::Cancelled);
			kinds.assign(floor.ground.size(), 255);
			for (size_t i = 0; i < kinds.size(); ++i) if (floor.traits[i] & bit(Trait::Present)) kinds[i] = kindOfKey[floor.ground[i]];
			addLagStats(kinds, static_cast<uint32_t>(floor.bounds.width), static_cast<uint32_t>(floor.bounds.height), total);
		}
		return total;
	} catch (const std::bad_alloc&) {
		return std::unexpected(StatsError::AllocationFailed);
	}
}

LagStats lagStatsOf(const SurfaceGrid& grid) {
	std::vector<uint8_t> kinds(grid.cells.size(), 255);
	for (size_t i = 0; i < kinds.size(); ++i) {
		switch (grid.cells[i]) {
			case SurfaceKind::Grass: kinds[i] = 0; break;
			case SurfaceKind::Dirt: kinds[i] = 1; break;
			case SurfaceKind::Rock: kinds[i] = 2; break;
			case SurfaceKind::Shore: kinds[i] = 3; break;
			case SurfaceKind::Lake: case SurfaceKind::River: kinds[i] = 4; break;
			case SurfaceKind::Preserve: break;
		}
	}
	LagStats stats;
	addLagStats(kinds, grid.region.width, grid.region.height, stats);
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

} // namespace rme::procedural
