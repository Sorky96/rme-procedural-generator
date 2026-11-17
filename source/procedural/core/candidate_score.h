// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/level_repair.h"

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace rme::procedural {

// Names and units of g5_shape_table. Counts and shares that only control the measurement are excluded.
inline constexpr std::array<std::string_view, 33> candidateMetricNames {
    "rivers.T", "rivers.S", "rivers.W", "channels.L", "channels.W", "channels.B", "channels.C",
    "scale.E4", "scale.E16", "scale.E64",
    "trans.R0", "trans.R1", "trans.R2", "trans.R3", "trans.D0", "trans.D1", "trans.D2", "trans.D3",
    "paths.L", "paths.W", "paths.B", "paths.rockadj", "paths.rocknear",
    "build.per64k", "build.long", "build.short", "outline.median",
    "patch.D", "patch.F", "patch.P", "patch.RB", "patch.RF", "patch.RS"
};
using CandidateMetrics = std::array<std::optional<int64_t>, candidateMetricNames.size()>;

// Shape references are not stored in g4-profile/6. Supply reviewed bands explicitly; never infer them from tuning.
// Bounds are hundredths of the metric's native unit, inclusive: 67*ref and 150*ref express the relative band exactly.
struct CandidateBand {
    size_t metric = 0;
    int64_t reference = 0;
    int64_t lowerHundredths = 0, upperHundredths = 0;
};
enum class CandidateScoreError : uint8_t {
    InvalidGrid, MeasurementFailed, Cancelled, AllocationFailed, InvalidBands, MissingMetric, NonPositiveReference, NegativeMetric
};
struct CandidateScore {
    bool hardFail = true;
    uint32_t inBand = 0;
    int64_t distance = 0; // round(1000 * SUM(abs(log(value/ref)))); zero value uses log(1/64)
    uint32_t index = 0;
    bool operator==(const CandidateScore&) const = default;
};
[[nodiscard]] bool betterCandidate(const CandidateScore& a, const CandidateScore& b) noexcept;
// SplitMix64 finalizer, fixed unsigned arithmetic. Index 0 is the unchanged user seed.
[[nodiscard]] uint64_t candidateSeed(uint64_t userSeed, uint32_t index) noexcept;
[[nodiscard]] std::expected<CandidateMetrics, CandidateScoreError> candidateMetricsOf(
    const SurfaceGrid& grid, std::stop_token stop = {});
// No N/A or negative value is silently dropped: all candidates must use exactly the same metric set.
[[nodiscard]] std::expected<CandidateScore, CandidateScoreError> scoreCandidateMetrics(
    const CandidateMetrics& metrics, std::span<const CandidateBand> bands, bool hardFail, uint32_t index);
[[nodiscard]] bool candidateHardFail(bool validationPassed, const LevelReachabilityReport& reach,
    const LevelRepairReport& repair) noexcept;
// Read-only evaluation of an existing result. Validation status comes from the producing pipeline, including adapter checks.
[[nodiscard]] std::expected<CandidateScore, CandidateScoreError> scoreCandidate(
    const SurfaceGrid& surface, const LevelPlan& plan, const LevelReachabilityReport& reach,
    const LevelRepairReport& repair, bool validationPassed, std::span<const CandidateBand> bands,
    uint32_t index, std::stop_token stop = {});

} // namespace rme::procedural
