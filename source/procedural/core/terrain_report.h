// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/candidate_score.h"
#include "procedural/core/copy_guard.h"
#include "procedural/core/surface.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rme::procedural {

inline constexpr size_t surfaceKindCount = size_t(SurfaceKind::Bridge) + 1;

// What the generator dialog shows for one prepared candidate (I58). Plain inputs; pointers may be null when the
// candidate has no such part (no levels, no copy index).
struct TerrainReportInput {
    uint32_t width = 0, height = 0;
    // Cells of the final Surface grid per SurfaceKind; all zero when the candidate is not a Surface one.
    std::array<uint64_t, surfaceKindCount> surfaceCells {};
    const LevelPlan* plan = nullptr;
    const LevelReachabilityReport* reach = nullptr;
    const LevelRepairReport* repair = nullptr;
    // Learned out-of-reach shares; none = the rule without them (candidateHardFail without a tolerance).
    std::optional<ReachTolerance> tolerance;
    const CopyAssessment* copy = nullptr;
    std::span<const std::string> aliases; // copy-index references, in index order
    uint64_t memoryBytes = 0, memoryBudget = 0;
    std::optional<uint64_t> elapsedMs;
};

struct TerrainWarning {
    bool blocking = false; // one entry per cause of candidateHardFail
    std::string text;
};

struct TerrainReport {
    std::vector<std::pair<std::string, std::string>> rows; // statistic, value (UTF-8)
    std::vector<TerrainWarning> warnings;
};

// Blocking warnings are non-empty exactly when the copy assessment or candidateHardFail (with the given rule) fails.
// Throws std::bad_alloc.
[[nodiscard]] TerrainReport terrainReportOf(const TerrainReportInput& input);

} // namespace rme::procedural
