// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/doodad_class.h"

namespace rme::procedural {
namespace {

constexpr DoodadRule defaultRules[] = {
	{"tree", DecorKind::Tree}, {"trees", DecorKind::Tree}, {"palm", DecorKind::Tree}, {"mangrove", DecorKind::Tree}, {"trunk", DecorKind::Tree},
	{"bush", DecorKind::Bush}, {"bushes", DecorKind::Bush}, {"plant", DecorKind::Bush}, {"plants", DecorKind::Bush},
	{"flower", DecorKind::Bush}, {"flowers", DecorKind::Bush}, {"sunflowers", DecorKind::Bush}, {"herbs", DecorKind::Bush},
	{"grass", DecorKind::Bush}, {"mushroom", DecorKind::Bush}, {"mushrooms", DecorKind::Bush}, {"lilypads", DecorKind::Bush},
	{"rock", DecorKind::Stone}, {"rocks", DecorKind::Stone}, {"stones", DecorKind::Stone}, {"kamienie", DecorKind::Stone},
	{"boulders", DecorKind::Stone}, {"stalagmites", DecorKind::Stone}, {"staglamites", DecorKind::Stone},
};

bool hasWord(std::string_view name, std::string_view token) noexcept {
	auto isWord = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };
	size_t i = 0;
	while (i < name.size()) {
		while (i < name.size() && !isWord(name[i])) ++i;
		size_t j = i;
		while (j < name.size() && isWord(name[j])) ++j;
		if (j - i == token.size()) {
			bool same = true;
			for (size_t k = 0; k < token.size() && same; ++k) {
				char c = name[i + k];
				if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
				same = c == token[k];
			}
			if (same) return true;
		}
		i = j;
	}
	return false;
}

} // namespace

std::span<const DoodadRule> defaultDoodadRules() noexcept { return defaultRules; }

DecorKind classifyDoodadBrush(std::string_view name, std::span<const DoodadRule> rules) noexcept {
	for (const DoodadRule& rule : rules) {
		if (hasWord(name, rule.token)) return rule.kind;
	}
	return DecorKind::None;
}

DecorKind classifyDoodadBrush(std::string_view name) noexcept { return classifyDoodadBrush(name, defaultRules); }

} // namespace rme::procedural
