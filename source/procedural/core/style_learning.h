// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/style_fit.h"
#include <filesystem>

namespace rme::procedural {
struct LearningSample {
    StyleStats stats;
    uint64_t present = 0, recognized = 0;
    StylePalette palette;
};
// Surface-only, bounded input. Unknown keys remain visible in the denominator.
std::expected<LearningSample, std::string> inspectLearningSample(
    const SemanticMap& map, std::span<const KeyMapEntry> mapping, bool permission, std::stop_token stop = {});
std::expected<StyleProfile, std::string> fitLearningSample(const LearningSample& sample,
    std::vector<KeyMapEntry> mapping, std::string client, std::string otb, std::string dat,
    std::stop_token stop = {});
// ASCII portable filename stem; no paths, reserved device names or implicit overwrites.
bool validLearnedProfileName(std::string_view name);
std::expected<void, std::string> saveLearnedProfile(const std::filesystem::path& directory,
    std::string_view name, const StyleProfile& profile);
} // namespace rme::procedural
