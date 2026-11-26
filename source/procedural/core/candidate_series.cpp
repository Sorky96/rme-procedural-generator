// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/candidate_series.h"
namespace rme::procedural {
namespace {
constexpr std::array m06 {
    CandidateBand{0, 384, 25728, 57600},
    CandidateBand{1, 2898, 194166, 434700},
    CandidateBand{2, 41134, 2755978, 6170100},
    CandidateBand{3, 614, 41138, 92100},
    CandidateBand{4, 394, 26398, 59100},
    CandidateBand{5, 233, 15611, 34950},
    CandidateBand{6, 421, 28207, 63150},
    CandidateBand{7, 749, 50183, 112350},
    CandidateBand{8, 5638, 377746, 845700},
    CandidateBand{9, 333, 22311, 49950},
    CandidateBand{10, 167, 11189, 25050},
    CandidateBand{11, 36, 2412, 5400},
    CandidateBand{12, 33, 0, 8300},
    CandidateBand{13, 938, 62846, 140700},
    CandidateBand{14, 24, 0, 7400},
    CandidateBand{15, 4, 0, 5400},
    CandidateBand{16, 951, 63717, 142650},
    CandidateBand{17, 32, 0, 8200},
    CandidateBand{18, 12, 0, 6200},
    CandidateBand{19, 4, 0, 5400},
    CandidateBand{20, 70, 4690, 10500},
    CandidateBand{21, 3642, 244014, 546300},
    CandidateBand{22, 31, 2077, 4650},
    CandidateBand{23, 9, 603, 1350},
    CandidateBand{24, 7, 469, 1050},
    CandidateBand{25, 30, 2010, 4500}
};
constexpr std::array m19 {
    CandidateBand{0, 302, 20234, 45300},
    CandidateBand{1, 4337, 290579, 650550},
    CandidateBand{2, 38913, 2607171, 5836950},
    CandidateBand{3, 865, 57955, 129750},
    CandidateBand{4, 425, 28475, 63750},
    CandidateBand{5, 197, 13199, 29550},
    CandidateBand{6, 451, 30217, 67650},
    CandidateBand{7, 625, 41875, 93750},
    CandidateBand{8, 5656, 378952, 848400},
    CandidateBand{9, 121, 8107, 18150},
    CandidateBand{10, 862, 57754, 129300},
    CandidateBand{11, 15, 0, 3500},
    CandidateBand{12, 99, 6633, 14850},
    CandidateBand{13, 502, 33634, 75300},
    CandidateBand{14, 251, 16817, 37650},
    CandidateBand{15, 147, 9849, 22050},
    CandidateBand{16, 728, 48776, 109200},
    CandidateBand{17, 155, 10385, 23250},
    CandidateBand{18, 27, 0, 7700},
    CandidateBand{19, 88, 5896, 13200},
    CandidateBand{20, 78, 5226, 11700},
    CandidateBand{21, 4606, 308602, 690900},
    CandidateBand{22, 37, 2479, 5550},
    CandidateBand{23, 9, 603, 1350},
    CandidateBand{24, 7, 469, 1050},
    CandidateBand{25, 24, 1608, 3600}
};
}
std::span<const CandidateBand> candidateBandsFor(const StyleProfile& profile) noexcept {
    if (!profile.bands.empty()) return profile.bands;
    if (profile.sources.size() != 1 || profile.sources[0].weightPermille != 1000) return {};
    if (profile.sources[0].alias == "M06") return m06;
    if (profile.sources[0].alias == "M19") return m19;
    return {};
}
std::optional<uint32_t> recommendedCandidate(std::span<const CandidateScore> scores, bool ranked) noexcept {
    std::optional<uint32_t> best;
    for (uint32_t i = 0; i < scores.size(); ++i) {
        if (scores[i].hardFail) continue;
        if (!best || (ranked && betterCandidate(scores[i], scores[*best]))) best = i;
    }
    return best;
}
std::optional<uint64_t> candidateWorkingBudget(uint64_t total, uint32_t count,
    uint64_t retainedBytes, uint64_t imageBytes) noexcept {
    // Native thumbnail, list image copy and metadata allowance for every row, including failed rows.
    constexpr uint64_t rowBytes = 128 * 128 * 8 + 4096;
    if (!validCandidateCount(count)) return {};
    // Owned requests/profile strings, row vector and native list bookkeeping.
    for (const auto charge : {uint64_t(256 * 1024), rowBytes * count, retainedBytes, imageBytes}) {
        if (charge >= total) return {};
        total -= charge;
    }
    return total;
}
} // namespace rme::procedural
