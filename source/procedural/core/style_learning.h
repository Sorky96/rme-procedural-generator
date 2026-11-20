// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/decor_context.h"
#include "procedural/core/style_fit.h"
#include <filesystem>

namespace rme::procedural {
struct LearningSample {
    StyleStats stats;
    uint64_t present = 0, recognized = 0;
    StylePalette palette;
    DecorContext decor;               // tree/bush/stone counts by ground context, from the decor traits
    StyleDecorPalette decorPalette;
    std::array<StyleMix, maxGrassMix> grassMix; // further grass brushes with at least 5% of the grass tiles each
    std::array<StyleMix, maxTreeMix> treeMix; // further tree brushes with at least 5% of the tree items each
};
// Surface-only, bounded input. Unknown keys remain visible in the denominator.
// `doodadItems` are the stacked items per doodad brush name read from the same rectangle.
std::expected<LearningSample, std::string> inspectLearningSample(const SemanticMap& map,
    std::span<const KeyMapEntry> mapping, std::span<const KeyTiles> doodadItems, bool permission, std::stop_token stop = {});
std::expected<StyleProfile, std::string> fitLearningSample(const LearningSample& sample,
    std::vector<KeyMapEntry> mapping, std::string client, std::string otb, std::string dat,
    std::stop_token stop = {});
// ASCII portable filename stem; no paths, reserved device names or implicit overwrites.
bool validLearnedProfileName(std::string_view name);
std::expected<void, std::string> saveLearnedProfile(const std::filesystem::path& directory,
    std::string_view name, const StyleProfile& profile);
} // namespace rme::procedural
