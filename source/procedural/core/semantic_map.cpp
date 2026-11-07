// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/semantic_map.h"

#include <algorithm>
#include <limits>
#include <new>

namespace rme::procedural {

const char* traitName(size_t index) noexcept {
	static constexpr const char* names[traitCount] = {
		"has_ground", "blocking", "blocks_missiles", "blocks_path", "wall", "door", "table", "carpet",
		"border", "floor_change", "teleport", "magic_field", "container", "doodad", "loose_item", "unknown"};
	return index < traitCount ? names[index] : "";
}

std::expected<SemanticMap, SemanticError> SemanticMap::create(
	std::span<const FloorBounds> bounds, const SemanticBudget& budget) {
	SemanticMap map;
	uint64_t total = 0;
	std::vector<FloorBounds> ordered(bounds.begin(), bounds.end());
	std::ranges::sort(ordered, {}, &FloorBounds::z);
	for (size_t i = 0; i < ordered.size(); ++i) {
		const FloorBounds& b = ordered[i];
		if (b.width <= 0 || b.height <= 0 || b.z < 0 || b.z > 15 || (i > 0 && ordered[i - 1].z == b.z)) {
			return std::unexpected(SemanticError::InvalidBounds);
		}
		total += static_cast<uint64_t>(b.width) * static_cast<uint64_t>(b.height);
		if (total > budget.maxCells) return std::unexpected(SemanticError::CellLimit);
	}
	try {
		map.floors_.reserve(ordered.size());
		for (const FloorBounds& b : ordered) {
			const auto cells = static_cast<size_t>(b.width) * static_cast<size_t>(b.height);
			map.floors_.push_back({b, std::vector<uint16_t>(cells, 0), std::vector<uint32_t>(cells, 0)});
		}
	} catch (const std::bad_alloc&) {
		return std::unexpected(SemanticError::AllocationFailed);
	}
	map.cells_ = total;
	return map;
}

std::expected<uint16_t, SemanticError> SemanticMap::internGroundKey(std::string_view name) {
	if (name.empty()) return noGroundKey;
	for (size_t i = 1; i < keys_.size(); ++i) {
		if (keys_[i] == name) return static_cast<uint16_t>(i);
	}
	if (keys_.size() > std::numeric_limits<uint16_t>::max()) return std::unexpected(SemanticError::TooManyGroundKeys);
	try {
		keys_.emplace_back(name);
	} catch (const std::bad_alloc&) {
		return std::unexpected(SemanticError::AllocationFailed);
	}
	return static_cast<uint16_t>(keys_.size() - 1);
}

std::expected<void, SemanticError> SemanticMap::set(int x, int y, int z, uint16_t groundKey, uint32_t traits) {
	const auto it = std::ranges::find(floors_, z, [](const SemanticFloor& f) { return f.bounds.z; });
	if (it == floors_.end() || groundKey >= keys_.size()) return std::unexpected(SemanticError::OutOfBounds);
	const FloorBounds& b = it->bounds;
	if (x < b.x0 || y < b.y0 || x >= b.x0 + b.width || y >= b.y0 + b.height) {
		return std::unexpected(SemanticError::OutOfBounds);
	}
	const auto index = static_cast<size_t>(y - b.y0) * static_cast<size_t>(b.width) + static_cast<size_t>(x - b.x0);
	it->ground[index] = groundKey;
	it->traits[index] = traits | bit(Trait::Present);
	return {};
}

SemanticSummary summarize(const SemanticMap& map) {
	SemanticSummary summary;
	summary.groundKeyStats.assign(map.groundKeys().size(), {});
	for (const SemanticFloor& floor : map.floors()) {
		for (size_t i = 0; i < floor.traits.size(); ++i) {
			const uint32_t traits = floor.traits[i];
			if (!(traits & bit(Trait::Present))) continue;
			++summary.tilesPerFloor[static_cast<size_t>(floor.bounds.z)];
			for (size_t t = 0; t < traitCount; ++t) {
				if (traits & (1u << t)) ++summary.tilesPerTrait[t];
			}
			auto& stat = summary.groundKeyStats[floor.ground[i]];
			++stat.tiles;
			if (traits & bit(Trait::Blocking)) ++stat.blockingTiles;
		}
	}
	return summary;
}

} // namespace rme::procedural
