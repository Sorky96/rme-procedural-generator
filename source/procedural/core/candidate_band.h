// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

namespace rme::procedural {

// Ranking band of one candidate metric (index into candidateMetricNames). Bounds are hundredths of the metric's native
// unit, inclusive: 67*ref and 150*ref express the relative band exactly.
struct CandidateBand {
    size_t metric = 0;
    int64_t reference = 0;
    int64_t lowerHundredths = 0, upperHundredths = 0;
    bool operator==(const CandidateBand&) const = default;
};

} // namespace rme::procedural
