// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string_view>
class DoodadBrush;
namespace rme::procedural {
// Where a vegetation item comes from: a doodad brush with single-tile items, picked by a roll of 1..total
// over the brush's chances, or one RAW item (a "raw:<id>" name) that every roll places.
struct DecorSource {
	static constexpr uint64_t rawTotal = 1000; // rolls of a RAW source, so scaling a roll into it or out of it keeps its spread
	const DoodadBrush* doodad = nullptr;
	uint16_t raw = 0;
	uint64_t total = 0;
	[[nodiscard]] explicit operator bool() const noexcept { return total != 0; }
	// The item for roll 1..total; 0 when the brush has none for it.
	[[nodiscard]] uint16_t item(uint64_t roll) const;
};
// GUI thread. The source a doodad brush name or "raw:<id>" names in the loaded client, or an empty one when it
// does not exist, has no single-tile items or a chance sum above maxChance. Items are not checked; see decorItemPlaceable.
[[nodiscard]] DecorSource findDecorSource(std::string_view name, uint64_t maxChance);
// A real, plain, non-ground, non-border item of the loaded client.
[[nodiscard]] bool decorItemPlaceable(uint16_t id);
}
