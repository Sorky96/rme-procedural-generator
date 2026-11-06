// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/generation_input.h"
#include "app/managers/version_manager.h"

#include "map/map.h"
#include "map/map_region.h"
#include <algorithm>
#include <limits>
#include <new>
#include <wx/thread.h>

namespace rme::procedural {
namespace {
bool protectedLocation(const TileLocation* location) {
	if (!location) return false;
	if (location->getSpawnCount() || location->getWaypointCount() || location->getTownCount() ||
		(location->getHouseExits() && !location->getHouseExits()->empty())) return true;
	const auto* tile = location->get();
	if (!tile) return false;
	// Protect entire occupied tiles, including containers, unknown attributes and
	// UID/AID anywhere in their contents, without inspecting or copying those items.
	constexpr uint16_t harmlessState = TILESTATE_SELECTED | TILESTATE_MODIFIED;
	return tile->ground || !tile->items.empty() || tile->creature || tile->spawn ||
		tile->house_id || tile->mapflags || tile->extra || (tile->statflags & ~harmlessState);
}
}

std::expected<GenerationInput, InputError> captureGenerationInput(
	const Map& map, const GenerationConfig& config, CoreBudget coreBudget,
	uint64_t maxInputBytes, std::span<const uint8_t> mask, std::stop_token stop) {
	if (!wxIsMainThread()) return std::unexpected(InputError::WrongThread);
	if (stop.stop_requested()) return std::unexpected(InputError::Cancelled);
	// The generator deliberately stays inside both header dimensions and the
	// inclusive Position limits. Widen the latter before making them exclusive.
	const MapExtent extent {
		uint32_t(std::min(map.getWidth(), MAP_MAX_WIDTH + 1)),
		uint32_t(std::min(map.getHeight(), MAP_MAX_HEIGHT + 1)), MAP_LAYERS
	};
	const auto layout = validateGeneration(config, extent, coreBudget, mask, {}, stop);
	if (!layout) return std::unexpected(layout.error() == GenerationError::Cancelled ?
		InputError::Cancelled : InputError::InvalidGeneration);
	if (layout->cellCount > (std::numeric_limits<size_t>::max() - sizeof(GenerationInput)) / 2 ||
		sizeof(GenerationInput) + uint64_t(layout->cellCount) * 2 > maxInputBytes) {
		return std::unexpected(InputError::InputLimit);
	}
	try {
		GenerationInput result {config, extent, map.getGeneration(), map.getChangeTracker().getGeneration(),
			g_version.GetAssetRevision(),
			std::vector<uint8_t>(layout->cellCount, 1), std::vector<uint8_t>(layout->cellCount, 0)};
		for (size_t i = 0; i < layout->cellCount; ++i) {
			if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(InputError::Cancelled);
			if (!mask.empty()) result.mask[i] = mask[i];
			const int x = config.region.x + int(i % config.region.width);
			const int y = config.region.y + int(i / config.region.width);
			result.protection[i] = protectedLocation(map.getTileL(x, y, config.region.z)) ? 1 : 0;
		}
		const auto protectedLayout = validateGeneration(config, extent, coreBudget, result.mask, result.protection, stop);
		if (!protectedLayout) return std::unexpected(protectedLayout.error() == GenerationError::Cancelled ?
			InputError::Cancelled : InputError::InvalidGeneration);
		return result;
	} catch (const std::bad_alloc&) {
		return std::unexpected(InputError::AllocationFailed);
	}
}

} // namespace rme::procedural
