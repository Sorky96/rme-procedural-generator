// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/candidate_band.h"
#include "procedural/core/level_repair.h"

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace rme::procedural {

// Names and units of g5_shape_table. Counts and shares that only control the measurement are excluded.
inline constexpr std::array<std::string_view, 33> candidateMetricNames {
    "channels.L", "channels.W", "channels.B", "channels.C", "scale.E16", "scale.E64",
    "patch.D", "patch.F", "patch.P", "patch.RB", "patch.RF", "patch.RS",
    "trans.R0", "trans.R1", "trans.R2", "trans.R3", "trans.D0", "trans.D1", "trans.D2", "trans.D3",
    "paths.L", "paths.W",
    "build.per64k", "build.long", "build.short", "outline.median",
    "scale.E4", "rivers.T", "rivers.S", "rivers.W", "paths.B", "paths.rockadj", "paths.rocknear"
};
// First 26 entries are the frozen I51 policy, in the binding G6_METRICS table order.
inline constexpr size_t candidateRankingMetricCount = 26;
[[nodiscard]] constexpr bool candidateSignedMetric(size_t metric) noexcept {
    return metric == 4 || metric == 5 || metric == 26;
}
using CandidateMetrics = std::array<std::optional<int64_t>, candidateMetricNames.size()>;

// Bands come from reviewed references (g6-bands-v1) or from measured learning samples; never inferred from tuning.
// A learned band exists for the ranking metrics except paths and buildings, which a learning sample does not contain.
[[nodiscard]] constexpr bool learnableCandidateMetric(size_t metric) noexcept {
    return metric < candidateRankingMetricCount && (metric < 20 || metric > 25);
}
// The band rule of policy 396e057 for a measured reference: [0.67r; 1.5r]; trans.R*/D* [max(0, r-50); r+50] when r < 50;
// patch.RS [0; r+20] when r < 20. A signed reference below zero gets its bounds in ascending order. nullopt for a metric
// that is not learnable or a negative reference of an unsigned metric.
[[nodiscard]] std::optional<CandidateBand> learnedCandidateBand(size_t metric, int64_t reference) noexcept;
enum class CandidateScoreError : uint8_t {
    InvalidGrid, MeasurementFailed, Cancelled, AllocationFailed, InvalidBands, NegativeMetric
};
struct CandidateScore {
    bool hardFail = true;
    uint32_t inBand = 0;
    int64_t distance = 0; // round(1000 * sum of capped contributions), once in table order
    uint32_t index = 0;
    bool operator==(const CandidateScore&) const = default;
};
[[nodiscard]] bool betterCandidate(const CandidateScore& a, const CandidateScore& b) noexcept;
// Preregistered I51b experiment only; production ordering above remains A.
[[nodiscard]] bool betterCandidateI51b(const CandidateScore& a, const CandidateScore& b) noexcept;
// SplitMix64 finalizer, fixed unsigned arithmetic. Index 0 is the unchanged user seed.
[[nodiscard]] uint64_t candidateSeed(uint64_t userSeed, uint32_t index) noexcept;
[[nodiscard]] std::expected<CandidateMetrics, CandidateScoreError> candidateMetricsOf(
    const SurfaceGrid& grid, std::stop_token stop = {});
// nullopt is definitional N/A (constant ln(64) penalty), never a missing input column.
// Signed excess uses asinh in native permille; other metrics must be nonnegative.
[[nodiscard]] std::expected<CandidateScore, CandidateScoreError> scoreCandidateMetrics(
    const CandidateMetrics& metrics, std::span<const CandidateBand> bands, bool hardFail, uint32_t index);
[[nodiscard]] bool candidateHardFail(bool validationPassed, const LevelReachabilityReport& reach,
    const LevelRepairReport& repair) noexcept;
// How much a map may leave out of reach, permille, as its reference did (StyleProfile::unreachedRockPermille): of the
// walkable cells above the ground floor (mountain tops), and of the walkable cells of the ground floor and the caves.
struct ReachTolerance { uint32_t capPermille = 0, walkPermille = 0; };
// As above, but cells and cap places out of reach fail only beyond the tolerance; every other kind of place (doors, cave
// entrances, storeys, dungeons) must still be reached.
[[nodiscard]] bool candidateHardFail(bool validationPassed, const LevelPlan& plan, const LevelReachabilityReport& reach,
    const LevelRepairReport& repair, ReachTolerance tolerance) noexcept;
// Read-only evaluation of an existing result. Validation status comes from the producing pipeline, including adapter checks.
[[nodiscard]] std::expected<CandidateScore, CandidateScoreError> scoreCandidate(
    const SurfaceGrid& surface, const LevelPlan& plan, const LevelReachabilityReport& reach,
    const LevelRepairReport& repair, bool validationPassed, std::span<const CandidateBand> bands,
    uint32_t index, std::stop_token stop = {});

} // namespace rme::procedural
