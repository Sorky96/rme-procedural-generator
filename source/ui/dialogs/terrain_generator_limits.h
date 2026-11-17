// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/generation_selection.h"
#include "procedural/adapter/terrain_commit.h"
#include "procedural/adapter/terrain_preparation.h"

namespace rme::ui::terrain_generator {

inline constexpr uint64_t MiB = 1024ULL * 1024ULL;
inline constexpr uint64_t GiB = 1024 * MiB;
inline constexpr int maxSide = 2000;
inline constexpr uint64_t maxCells = uint64_t(maxSide) * maxSide;
// Surface, cave and up to three mountain caps. Changes are counted jointly.
inline constexpr uint64_t maxFloors = 5;
inline constexpr uint64_t maxChanges = maxCells * maxFloors;
inline constexpr uint64_t maxContextCells = uint64_t(maxSide + 4) * (maxSide + 4);

// These cap conservative stage/transition estimates, not the process's RSS.
// The source map, loaded assets, renderer and unrelated history are excluded.
inline constexpr procedural::PreparationBudget preparationBudget {maxCells, maxContextCells, maxChanges, 20 * GiB};
inline constexpr procedural::SelectionMaskBudget selectionBudget {maxCells, maxCells + 64 * 1024};
inline constexpr procedural::TerrainCommitBudget commitBudget {maxChanges, 24 * GiB};

} // namespace rme::ui::terrain_generator
