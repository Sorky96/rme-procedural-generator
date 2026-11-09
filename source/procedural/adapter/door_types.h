// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "brushes/brush_enums.h"

#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace rme::procedural {

// Door kinds a generated building may get (G4 iteration 12e). The names are what the generator dialog offers and what
// GroundProfile::door stores; the wall brush of the profile decides which item each kind becomes.
inline constexpr std::array<std::pair<std::string_view, DoorType>, 6> doorKinds {{
	{"Normal door", WALL_DOOR_NORMAL},
	{"Locked door", WALL_DOOR_LOCKED},
	{"Magic door", WALL_DOOR_MAGIC},
	{"Quest door", WALL_DOOR_QUEST},
	{"Normal door (alt)", WALL_DOOR_NORMAL_ALT},
	{"Archway", WALL_ARCHWAY},
}};

[[nodiscard]] constexpr std::optional<DoorType> doorTypeOf(std::string_view name) noexcept {
	for (const auto& [kindName, type] : doorKinds) if (kindName == name) return type;
	return std::nullopt;
}

} // namespace rme::procedural
