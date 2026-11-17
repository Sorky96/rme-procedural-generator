// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/candidate_score.h"
#include "procedural/core/scale_stats.h"
#include "procedural/core/water_shape.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace rme::procedural;
namespace {
void require(bool ok, std::string_view message) {
    if (!ok) throw std::runtime_error(std::string(message));
}
void handCounted() {
    // References 100, bands [67,150]. A=(100,100), two bands, but fails a hard condition.
    // B=(120,200) => 875; C=D=(110,200) => 788, one band each. C's index breaks C/D's tie.
    const CandidateBand band {0, 100, 6700, 15000};
    const auto score = [&](int64_t value, bool hard, uint32_t index) {
        CandidateMetrics metrics {}; metrics[0] = value;
        const auto result = scoreCandidateMetrics(metrics, std::span(&band, 1), hard, index);
        require(bool(result), "hand score failed");
        return *result;
    };
    const std::array pairBands {band, CandidateBand{1, 100, 6700, 15000}};
    const auto pairScore = [&](int64_t first, int64_t second, bool hard, uint32_t index) {
        CandidateMetrics metrics {}; metrics[0] = first; metrics[1] = second;
        const auto result = scoreCandidateMetrics(metrics, pairBands, hard, index);
        require(bool(result), "hand pair score failed");
        return *result;
    };
    const auto a = pairScore(100, 100, true, 0), b = pairScore(120, 200, false, 1),
        c = pairScore(110, 200, false, 2), d = pairScore(110, 200, false, 3);
    require(a.inBand == 2 && b.inBand == 1 && a.distance == 0 && b.distance == 875 && c.distance == 788, "hand distances");
    std::array ranked {a, b, d, c};
    std::ranges::sort(ranked, betterCandidate);
    require(ranked == std::array{c, d, b, a}, "hard, distance and index order");
    require(score(120, false, 0).distance == 182 && score(110, false, 0).distance == 95, "single-metric hand distances");
    require(score(67, false, 0).inBand == 1 && score(150, false, 0).inBand == 1, "inclusive boundaries");
    require(score(66, false, 0).inBand == 0 && score(151, false, 0).inBand == 0, "outside boundaries");
    require(score(0, false, 0).distance == 4159, "zero ratio log(1/64)");
    require(betterCandidate({false, 2, 10000, 3}, {false, 1, 0, 0}), "inBand precedes distance");
    // Round the SUM, not each term: 2 * log(1.0004) * 1000 rounds to 1, separately to 0.
    CandidateMetrics metrics {}; metrics[0] = metrics[1] = 2501;
    std::array bands {CandidateBand{0, 2500, 0, 400000}, CandidateBand{1, 2500, 0, 400000}};
    const auto sum = scoreCandidateMetrics(metrics, bands, false, 0);
    require(sum && sum->distance == 1, "round once after summation");
    std::ranges::reverse(bands);
    require(scoreCandidateMetrics(metrics, bands, false, 0) == sum, "band input order independent");
}
void refusals() {
    CandidateMetrics metrics {}; metrics[0] = 1;
    CandidateBand band {0, 1, 67, 150};
    require(!scoreCandidateMetrics(metrics, {}, false, 0), "empty policy refused");
    band.reference = 0;
    require(scoreCandidateMetrics(metrics, std::span(&band, 1), false, 0)->distance == 693, "zero ref: ln(2)");
    band.reference = 1; metrics[0].reset();
    require(scoreCandidateMetrics(metrics, std::span(&band, 1), false, 0) == CandidateScore{false, 0, 4159, 0}, "NA keeps denominator and penalty");
    metrics[0] = -1;
    require(scoreCandidateMetrics(metrics, std::span(&band, 1), false, 0).error() == CandidateScoreError::NegativeMetric, "negative unsigned metric refused");
    std::array duplicate {band, band};
    require(!scoreCandidateMetrics(metrics, duplicate, false, 0), "duplicate weights refused");
    band.metric = candidateMetricNames.size();
    require(!scoreCandidateMetrics(metrics, std::span(&band, 1), false, 0), "unknown metric refused");
    require(!candidateMetricsOf({}), "invalid surface refused");
}
void hardConditions() {
    LevelReachabilityReport reach; reach.start = 0;
    LevelRepairReport repair;
    require(!candidateHardFail(true, reach, repair), "valid baseline");
    require(candidateHardFail(false, reach, repair), "validation failure");
    reach.start.reset(); require(candidateHardFail(true, reach, repair), "missing start"); reach.start = 0;
    reach.edges = 1; require(candidateHardFail(true, reach, repair), "unwalkable Z edge"); reach.walkableEdges = 1;
    for (auto& category : reach.categories) {
        category.total = 1; require(candidateHardFail(true, reach, repair), "unreached category"); category.reached = 1;
    }
    repair.removedDecor = 10; require(!candidateHardFail(true, reach, repair), "decor removal is not lost POI");
    for (auto field : {&LevelRepairReport::removedPois, &LevelRepairReport::removedCaps,
        &LevelRepairReport::removedStoreys, &LevelRepairReport::removedDungeons}) {
        repair.*field = 1; require(candidateHardFail(true, reach, repair), "removed place"); repair.*field = 0;
    }
    repair.unrepaired = true; require(candidateHardFail(true, reach, repair), "unrepaired result");
}
void bindingDistances() {
    const auto score = [](size_t metric, std::optional<int64_t> value, int64_t ref, int64_t lo, int64_t hi) {
        CandidateMetrics metrics {}; metrics[metric] = value;
        const CandidateBand band {metric, ref, lo, hi};
        const auto result = scoreCandidateMetrics(metrics, std::span(&band, 1), false, 0);
        require(bool(result), "binding distance score");
        return *result;
    };
    // Independently tabulated: asinh(1)=0.8813735870; asinh(-1)=-0.8813735870.
    require(score(4, -1, 1, -100, 100).distance == 1763, "signed crosses zero");
    require(score(4, -1, 0, -100, 100).distance == 881, "signed zero reference");
    require(score(4, -1, -1, -100, 100).inBand == 1, "signed raw inclusive band");
    require(score(0, 63, 0, 0, 6300).distance == 4159, "zero reference ln(64)");
    require(score(0, 0, 0, 0, 0).distance == 0, "zero reference and value coincide");
    require(score(0, 1000000, 1, 0, 100).distance == 4159, "upper ratio capped");
    require(score(0, 1, 1000000, 0, 100).distance == 4159, "lower ratio capped");
    require(score(4, -1000, 1000, 0, 150000).distance == 4159, "signed capped");
    require(score(0, 67, 100, 6701, 15000).inBand == 0, "exact hundredths not truncated");
    require(score(11, 35, 15, 0, 3500).inBand == 1 && score(11, 36, 15, 0, 3500).inBand == 0, "U20 inclusive");
    require(score(12, 83, 33, 0, 8300).inBand == 1 && score(12, 84, 33, 0, 8300).inBand == 0, "A50 inclusive");
    CandidateMetrics full {};
    std::array<CandidateBand, candidateRankingMetricCount> bands;
    for (size_t i = 0; i < bands.size(); ++i) bands[i] = {i, 0, 0, 100};
    const auto allNa = scoreCandidateMetrics(full, bands, false, 0);
    require(allNa && allNa->inBand == 0 && allNa->distance == 108131, "26 * ln64, one rounding");
    full.fill(1);
    const auto allOne = scoreCandidateMetrics(full, bands, false, 0);
    // 24*ln(2) + 2*asinh(1) = 18.3982795075 (native u=1 in both transforms).
    require(allOne && allOne->inBand == 26 && allOne->distance == 18398, "26 frozen positions include both signed excesses");
    // No paths/buildings: length/density 0, widths/sides/outline N/A. Small patches stay N/A.
    SurfaceGrid grid; grid.region = {0, 0, 7, 10, 10}; grid.cells.assign(100, SurfaceKind::Grass);
    const auto metrics = candidateMetricsOf(grid);
    require(metrics && (*metrics)[0] == 0 && (*metrics)[20] == 0 && (*metrics)[22] == 0 &&
        !(*metrics)[1] && !(*metrics)[21] && !(*metrics)[23] && !(*metrics)[25] && !(*metrics)[6] && (*metrics)[5] == 0, "G4 N/A thresholds and zero excess without pairs");
}
void seedAndFingerprint() {
    require(candidateSeed(0, 1) == 0xe220a8397b1dcdafULL, "SplitMix64 golden vector");
    for (const auto seed : {0ULL, 1729ULL, UINT64_MAX}) {
        require(candidateSeed(seed, 0) == seed, "candidate zero preserves all seed bits");
        std::array<uint64_t, 16> seeds {};
        for (uint32_t i = 0; i < seeds.size(); ++i) seeds[i] = candidateSeed(seed, i);
        std::ranges::sort(seeds);
        require(std::adjacent_find(seeds.begin(), seeds.end()) == seeds.end(), "distinct candidate seeds");
    }
    const SurfaceConfig config {{100, 100, 7, 100, 100}, 1729};
    const SurfaceBudget budget {10000, 80000, 8 * 1024 * 1024};
    const auto original = generateSurface(config, {65001, 65001, 16}, budget);
    auto candidate = config; candidate.seed = candidateSeed(config.seed, 0);
    const auto zero = generateSurface(candidate, {65001, 65001, 16}, budget);
    require(original && zero && original->cells == zero->cells && zero->fingerprint() == 17159159863505677800ULL, "N=1 golden surface");
    const auto before = zero->fingerprint();
    const auto one = candidateMetricsOf(*zero), two = candidateMetricsOf(*zero);
    require(one && two && *one == *two && zero->fingerprint() == before, "pure repeatable measurement");
    const auto excess = lagExcessPermille(lagStatsOf(*zero));
    require((*one)[26] == excess[0] && (*one)[4] == excess[1] && (*one)[5] == excess[2], "shared scale metric units");
    const auto water = waterShapesOf(*zero);
    require(bool(water), "water stats");
    const auto river = summarizeRivers(water->rivers);
    if (river.count) require((*one)[27] == river.tortuosityPermille && (*one)[29] == river.widthMilli, "shared river metric units");
    auto plan = levelPlanOf(*zero, true, false, nullptr, {}, {}, {});
    require(bool(plan), "level plan");
    auto reach = levelReachabilityOf(*plan, {});
    require(bool(reach), "reach report");
    const CandidateBand band {7, 100, 0, 100000};
    const auto result = scoreCandidate(*zero, *plan, *reach, {}, true, std::span(&band, 1), 0);
    require(bool(result), "complete candidate evaluation");
    plan->region.x++;
    const auto mismatch = scoreCandidate(*zero, *plan, *reach, {}, true, std::span(&band, 1), 0);
    require(mismatch && mismatch->hardFail, "mismatched plan fails hard");
    std::stop_source stop; stop.request_stop();
    require(candidateMetricsOf(*zero, stop.get_token()).error() == CandidateScoreError::Cancelled, "cancel measurement");
}
} // namespace
int main() try {
    handCounted(); refusals(); bindingDistances(); hardConditions(); seedAndFingerprint();
    std::cout << "CANDIDATE_SCORE_PASS\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
