// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/decor_context.h"
#include "procedural/core/style_fit.h"
#include <filesystem>

namespace rme::procedural {
// Stacked items of one doodad brush on tiles of one ground key of the sample.
struct GroundDoodadItems {
    uint16_t ground = noGroundKey;
    std::string doodad;
    uint64_t items = 0;
};
// Stacked items that are not trees, bushes or stones (or blocking walls, doors, tables, carpets, borders) of one name
// (doodad brush or "raw:item:<id>") on tiles of one ground key: onTree on tiles that also hold a tree, onGround on the others.
struct GroundOverlayItems {
    uint16_t ground = noGroundKey;
    std::string name;
    uint64_t onTree = 0, onGround = 0;
};
// A tile of the sample with a blocking item or a wall piece: its row-major cell of the sample rectangle, the items stacked
// on it (bottom first, without ground, borders, carpets and doors), whether they are all pieces of a wall brush and
// whether one of them is named as a piece of a wall.
struct PlacedStack {
    uint32_t cell = 0;
    std::vector<uint16_t> items;
    bool plainWall = false;
    bool wallLike = false; // an item of it is named as a wall, pillar, column or railing (a wall laid without a brush)
};
struct LearningSample {
    StyleStats stats;
    uint64_t present = 0, recognized = 0;
    StylePalette palette;
    DecorContext decor;               // tree/bush/stone counts by ground context, from the decor traits
    StyleDecorPalette decorPalette;
    std::array<StyleMix, maxGrassMix> grassMix; // further grass brushes with at least 5% of the grass tiles each
    std::array<StyleMix, maxTreeMix> treeMix; // further tree brushes with at least 5% of the tree items each
    std::array<StyleMix, maxTreeMix> bushMix; // further bush brushes with at least 5% of the bush items each
    std::array<StyleMixDecor, maxGrassMix> grassMixDecor; // vegetation on each grassMix ground
    uint32_t clumpPermille = 0;       // DecorTuning::clumpPermille measured on the grass of the sample
    uint32_t shoreTreePermille = 0;   // share of the water tiles beside land holding a tree (DecorTuning::shoreTreePermille)
    std::string shoreTree;            // the tree brush or RAW item most used on them; set with shoreTreePermille
    uint64_t seaTiles = 0;            // water tiles connected to the edge of the sample or to tiles without ground
    std::array<StyleOverlay, maxOverlays> overlays; // the most used overlay items on land with their rates
    StyleBuildings buildings;         // buildings and ruins of the sample
    std::array<StyleOverlay, maxOverlays> interiors; // overlay items on building floors with their rates
    std::vector<StyleWallStack> wallStacks; // wall tiles as the sample built them (see StyleProfile::wallStacks)
    StyleOverlay carpet; // the carpet most laid on building floors (see StyleProfile::carpet)
    uint32_t rockOutlinePermille = 0; // StyleProfile::rockOutlinePermille
    // Distance in 4-steps from land to the nearest water: 25th, 50th, 75th and 90th percentile (all 0 without water).
    std::array<uint32_t, 4> shoreDistance {};
    uint32_t width = 0, height = 0;   // of the sample rectangle
    uint32_t mixEdgePermille = 0;     // land edge per tile of the grass mix grounds (permille); 0 without mixes
    uint32_t rockEdgePermille = 0;    // land edge per rock tile (permille): low for one range, high for scattered outcrops
    uint32_t dirtEdgePermille = 0;    // land edge per dirt tile (permille): high for small scattered dirt patches
};
// The generator builds its land from the Grass kind. A sample whose land is mostly sand (a desert) has almost no
// Grass: when the Grass tiles are under a tenth of the Shore tiles, every Shore key becomes Grass, so the sand is
// the main ground (and, without other shore, rings the water too). Other mappings are left as they are.
void promoteMainLand(std::vector<KeyMapEntry>& mapping, const SemanticMap& map);
// Surface-only, bounded input. Unknown keys remain visible in the denominator.
// `doodadItems` are the stacked doodad items per ground key and brush read from the same rectangle.
std::expected<LearningSample, std::string> inspectLearningSample(const SemanticMap& map,
    std::span<const KeyMapEntry> mapping, std::span<const GroundDoodadItems> doodadItems, bool permission, std::stop_token stop = {},
    std::span<const GroundOverlayItems> overlayItems = {}, std::span<const GroundDoodadItems> wallBrushes = {}, std::span<const PlacedStack> placedStacks = {},
    std::span<const GroundDoodadItems> carpetItems = {});
std::expected<StyleProfile, std::string> fitLearningSample(const LearningSample& sample,
    std::vector<KeyMapEntry> mapping, std::string client, std::string otb, std::string dat,
    std::stop_token stop = {});
// ASCII portable filename stem; no paths, reserved device names or implicit overwrites.
bool validLearnedProfileName(std::string_view name);
std::expected<void, std::string> saveLearnedProfile(const std::filesystem::path& directory,
    std::string_view name, const StyleProfile& profile);
} // namespace rme::procedural
