// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/decoration.h"
#include "procedural/core/semantic_map.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace rme::procedural {

// Classifies a doodad brush by the whole words of its name into the three kinds the generator can
// place. DecorKind::None means "not vegetation or stone for the generator" (furniture, fields,
// statues, debris), which is the answer for every name no rule matches. Heuristic, reviewed by the user.
struct DoodadRule {
	std::string_view token;
	DecorKind kind;
};

// Rules in priority order: the first token found among the words of the name decides.
[[nodiscard]] std::span<const DoodadRule> defaultDoodadRules() noexcept;
[[nodiscard]] DecorKind classifyDoodadBrush(std::string_view name, std::span<const DoodadRule> rules) noexcept;
[[nodiscard]] DecorKind classifyDoodadBrush(std::string_view name) noexcept;

// A vegetation source that is one item placed as is (from the RAW palette, without a doodad brush) is named
// "raw:<tree|bush|rock|item>:<server id>" wherever a doodad brush name is expected; the word makes classifyDoodadBrush
// give it its kind (Stone is "rock"; "item" is an overlay, DecorKind::None). Its id is checked against the loaded client before use.
[[nodiscard]] std::string rawDecorName(uint16_t id, DecorKind kind);
[[nodiscard]] std::optional<uint16_t> rawDecorId(std::string_view name) noexcept;

// The semantic-map trait that marks a tile holding such a doodad; 0 for DecorKind::None.
[[nodiscard]] constexpr uint32_t decorTraitBit(DecorKind kind) noexcept {
	switch (kind) {
		case DecorKind::Tree: return bit(Trait::TreeDoodad);
		case DecorKind::Bush: return bit(Trait::BushDoodad);
		case DecorKind::Stone: return bit(Trait::StoneDoodad);
		case DecorKind::None: break;
	}
	return 0;
}

} // namespace rme::procedural
