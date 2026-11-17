// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/candidate_score.h"
#include "procedural/core/scale_stats.h"
#include "procedural/core/water_shape.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <tuple>

namespace rme::procedural {

bool betterCandidate(const CandidateScore& a, const CandidateScore& b) noexcept {
    return std::tuple(a.hardFail, -int64_t(a.inBand), a.distance, a.index) <
        std::tuple(b.hardFail, -int64_t(b.inBand), b.distance, b.index);
}

uint64_t candidateSeed(uint64_t userSeed, uint32_t index) noexcept {
    if (!index) return userSeed;
    uint64_t x = userSeed + uint64_t(index) * 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

bool candidateHardFail(bool validationPassed, const LevelReachabilityReport& reach, const LevelRepairReport& repair) noexcept {
    return !validationPassed || !reach.allReached() || reach.walkableEdges != reach.edges || repair.unrepaired || repair.removedPois || repair.removedCaps ||
        repair.removedStoreys || repair.removedDungeons;
}

std::expected<CandidateScore, CandidateScoreError> scoreCandidateMetrics(
    const CandidateMetrics& metrics, std::span<const CandidateBand> bands, bool hardFail, uint32_t index) {
    if (bands.empty() || bands.size() > metrics.size()) return std::unexpected(CandidateScoreError::InvalidBands);
    std::array<bool, candidateMetricNames.size()> used {};
    CandidateScore result {hardFail, 0, 0, index};
    double distance = 0;
    // Canonical metric order makes the rounded sum independent of the order of band records.
    std::array<const CandidateBand*, candidateMetricNames.size()> ordered {};
    for (const auto& band : bands) {
        if (band.metric >= metrics.size() || used[band.metric] ||
            band.upperHundredths < band.lowerHundredths) return std::unexpected(CandidateScoreError::InvalidBands);
        if (!candidateSignedMetric(band.metric) && (band.reference < 0 || band.lowerHundredths < 0))
            return std::unexpected(CandidateScoreError::InvalidBands);
        used[band.metric] = true;
        ordered[band.metric] = &band;
    }
    for (const auto* band : ordered) {
        if (!band) continue;
        const auto& value = metrics[band->metric];
        const double cap = std::log(64.0);
        if (!value) { distance += cap; continue; }
        const bool signedMetric = candidateSignedMetric(band->metric);
        if (*value < 0 && !signedMetric) return std::unexpected(CandidateScoreError::NegativeMetric);
        // Measured metrics fit int32; avoid overflow even for synthetic caller input.
        if (*value > INT64_MAX / 100 || *value < INT64_MIN / 100) return std::unexpected(CandidateScoreError::InvalidBands);
        result.inBand += *value * 100 >= band->lowerHundredths && *value * 100 <= band->upperHundredths;
        const double contribution = signedMetric ? std::abs(std::asinh(double(*value)) - std::asinh(double(band->reference))) :
            band->reference == 0 ? std::log1p(double(*value)) :
            *value == 0 ? cap : std::abs(std::log(double(*value) / double(band->reference)));
        distance += std::min(cap, contribution);
    }
    result.distance = std::llround(distance * 1000);
    return result;
}

std::expected<CandidateMetrics, CandidateScoreError> candidateMetricsOf(const SurfaceGrid& grid, std::stop_token stop) try {
    if (!grid.valid()) return std::unexpected(CandidateScoreError::InvalidGrid);
    if (stop.stop_requested()) return std::unexpected(CandidateScoreError::Cancelled);
    CandidateMetrics result {};
    const auto water = waterShapesOf(grid, stop);
    const auto channels = channelShapesOf(grid, stop);
    const auto paths = pathShapesOf(grid, stop);
    if (!water || !channels || !paths) return std::unexpected(stop.stop_requested() ? CandidateScoreError::Cancelled : CandidateScoreError::MeasurementFailed);
    const auto river = summarizeRivers(water->rivers);
    if (river.count) { result[27] = river.tortuosityPermille; result[28] = river.straightPermille; result[29] = river.widthMilli; }
    const auto channel = summarizeChannels(*channels);
    result[0] = channel.lengthPer64k;
    if (channel.count) { result[1] = channel.widthMilli; result[2] = channel.branchesPer100Milli; result[3] = channel.connectedPermille; }
    const auto lag = lagExcessPermille(lagStatsOf(grid));
    for (size_t i = 0; i < lag.size(); ++i) result[i == 0 ? 26 : i + 3] = lag[i];
    const auto patch = summarizePatches(patchStatsOf(grid));
    for (size_t i = 0; i < 4; ++i) {
        if (patch.rockComposition) result[12 + i] = (*patch.rockComposition)[i];
        if (patch.dirtComposition) result[16 + i] = (*patch.dirtComposition)[i];
    }
    const auto path = summarizeChannels(*paths);
    result[20] = path.lengthPer64k;
    if (path.count) { result[21] = path.widthMilli; result[30] = path.branchesPer100Milli; }
    uint64_t adjacent = 0, near = 0;
    const auto w = grid.region.width, h = grid.region.height;
    // Same scan order as g5_shape_table/style_render, including its early exit on the first nearby rock.
    for (size_t c = 0; c < grid.cells.size(); ++c) {
        if (c % w == 0 && stop.stop_requested()) return std::unexpected(CandidateScoreError::Cancelled);
        if (grid.cells[c] != SurfaceKind::Path) continue;
        bool found = false, next = false;
        for (int32_t dy = -3; dy <= 3 && !found; ++dy) for (int32_t dx = -3; dx <= 3; ++dx) {
            const auto x = int64_t(c % w) + dx, y = int64_t(c / w) + dy;
            if (x < 0 || y < 0 || x >= w || y >= h || grid.cells[size_t(y) * w + size_t(x)] != SurfaceKind::Rock) continue;
            found = true;
            if (std::abs(dx) + std::abs(dy) == 1) next = true;
        }
        adjacent += next; near += found;
    }
    if (paths->maskCells) { result[31] = adjacent * 1000 / paths->maskCells; result[32] = near * 1000 / paths->maskCells; }
    const auto buildings = floorBuildingsOf(grid);
    const auto build = summarizeBuildings(buildings);
    result[22] = build.per64k;
    if (build.count) {
        result[23] = build.longMedian; result[24] = build.shortMedian;
        std::vector<uint32_t> outlines;
        for (const auto& b : buildings.buildings) outlines.push_back(2 * (b.longSide + b.shortSide) - 4);
        std::ranges::sort(outlines);
        result[25] = outlines[outlines.size() / 2]; // Same G4 measurement as g5_shape_table/style_render.
    }
    result[6] = patch.boundary; result[7] = patch.compact; result[8] = patch.beach;
    result[9] = patch.rockBoundary; result[10] = patch.rockCompact; result[11] = patch.rockDustShare;
    if (stop.stop_requested()) return std::unexpected(CandidateScoreError::Cancelled);
    return result;
} catch (const std::bad_alloc&) { return std::unexpected(CandidateScoreError::AllocationFailed); }

std::expected<CandidateScore, CandidateScoreError> scoreCandidate(
    const SurfaceGrid& surface, const LevelPlan& plan, const LevelReachabilityReport& reach,
    const LevelRepairReport& repair, bool validationPassed, std::span<const CandidateBand> bands,
    uint32_t index, std::stop_token stop) {
    const auto metrics = candidateMetricsOf(surface, stop);
    if (!metrics) return std::unexpected(metrics.error());
    const bool consistent = plan.region == surface.region && !plan.floors.empty() &&
        reach.walkable.size() == surface.cells.size() * plan.floors.size() && reach.reached.size() == reach.walkable.size();
    return scoreCandidateMetrics(*metrics, bands, candidateHardFail(validationPassed && consistent, reach, repair), index);
}

} // namespace rme::procedural
