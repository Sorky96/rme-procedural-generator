// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/semantic_map.h"
#include <cstdint>
#include <map>
#include <string>
class Map;
namespace rme::procedural {
// Stacked items per doodad brush name, summed over the rows read so far.
using DoodadItemCounts = std::map<std::string, uint64_t, std::less<>>;
// GUI-thread read only. One row (at most 512 cells) per call; never holds tile/brush pointers.
// A tile holding a tree, bush or stone doodad gets the matching decor trait (classifyDoodadBrush).
std::expected<void, std::string> readLearningRow(const Map& source, SemanticMap& destination, DoodadItemCounts& doodads, int row);
}
