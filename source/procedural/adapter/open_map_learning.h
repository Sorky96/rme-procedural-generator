// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/semantic_map.h"
class Map;
namespace rme::procedural {
// GUI-thread read only. One row (at most 512 cells) per call; never holds tile/brush pointers.
std::expected<void, std::string> readLearningRow(const Map& source, SemanticMap& destination, int row);
}
