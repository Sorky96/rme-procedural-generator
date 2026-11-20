// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/candidate_score.h"
#include "procedural/core/style_profile.h"

namespace rme::procedural {
inline constexpr uint32_t defaultCandidateCount = 4, maxCandidateCount = 16;
[[nodiscard]] constexpr bool validCandidateCount(uint32_t count) noexcept { return count >= 1 && count <= maxCandidateCount; }
// Explicit provenance association only. No filename, palette or tuning heuristics.
[[nodiscard]] std::span<const CandidateBand> candidateBandsFor(const StyleProfile& profile) noexcept;
[[nodiscard]] std::optional<uint32_t> recommendedCandidate(std::span<const CandidateScore> scores, bool ranked) noexcept;
// One live preparation plus the entire compact gallery and selection mask. Estimates, not RSS.
[[nodiscard]] std::optional<uint64_t> candidateWorkingBudget(uint64_t total, uint32_t count,
    uint64_t retainedBytes, uint64_t imageBytes) noexcept;
} // namespace rme::procedural
