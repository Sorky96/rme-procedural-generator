// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/semantic_map.h"
#include <cstdint>
#include <map>
#include <string>
#include <utility>
class Map;
namespace rme::procedural {
// Stacked items per (ground key of the sample, doodad brush name), summed over the rows read so far.
using DoodadItemCounts = std::map<std::pair<uint16_t, std::string>, uint64_t>;
// Every stacked item of the sample (blocking walls, doors, tables, carpets and borders aside) by server ID: how many,
// its client name and the doodad brush it belongs to (empty when it has none, e.g. placed from the RAW palette).
struct SampleItem { uint64_t count = 0; std::string name, brush; };
using SampleItemCounts = std::map<uint16_t, SampleItem>;
// Overlay items (not a tree, bush or stone) per (ground key, doodad brush name or "raw:item:<id>"): how many stand on a
// tile that also holds a tree, and how many on the other tiles.
using OverlayItemCounts = std::map<std::pair<uint16_t, std::string>, std::pair<uint64_t, uint64_t>>;
// GUI-thread read only. One row (at most 512 cells) per call; never holds tile/brush pointers.
// A tile holding a tree, bush or stone doodad gets the matching decor trait (classifyDoodadBrush).
std::expected<void, std::string> readLearningRow(const Map& source, SemanticMap& destination, DoodadItemCounts& doodads,
    SampleItemCounts& items, OverlayItemCounts& overlays, int row);
}
