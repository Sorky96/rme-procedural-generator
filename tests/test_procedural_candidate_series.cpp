// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/candidate_series.h"
#include <iostream>
#include <stdexcept>
using namespace rme::procedural;
void check(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
int main() try {
    check(defaultCandidateCount == 4 && !validCandidateCount(0) && !validCandidateCount(17), "count bounds");
    for (auto n : {1u, 4u, 16u}) {
        check(validCandidateCount(n), "valid count");
        const auto bytes = candidateWorkingBudget(1024*1024*1024, n, 100, 1000);
        check(bytes && *bytes < 1024*1024*1024, "joint budget");
        check(!candidateWorkingBudget(100, n, 0, 0), "fail closed, no N reduction");
        check(!candidateWorkingBudget(UINT64_MAX, n, UINT64_MAX, 0), "overflow safe");
        for (uint32_t i=0; i<n; ++i) check(candidateSeed(1729,i)==candidateSeed(1729,i), "seed repeatability");
    }
    check(candidateSeed(1729,0)==1729, "candidate zero");
    check(candidateWorkingBudget(512*1024,1,0,0).has_value() &&
        !candidateWorkingBudget(512*1024,4,0,0), "gallery shares one budget; refusing N=4 must not downgrade to N=1");
    StyleProfile profile;
    check(candidateBandsFor(profile).empty(), "no guessed policy");
    profile.sources = {{"M06",1000}};
    const auto m06 = candidateBandsFor(profile);
    check(m06.size()==26 && m06[0].reference==384 && m06[12].upperHundredths==8300, "m06 policy");
    profile.sources = {{"M19",1000}};
    const auto m19 = candidateBandsFor(profile);
    check(m19.size()==26 && m19[0].reference==302 && m19[11].upperHundredths==3500, "m19 policy");
    profile.sources.push_back({"M06",0});
    check(candidateBandsFor(profile).empty(), "ambiguous provenance has no policy");
    profile.sources={{"m06-like",1000}};
    check(candidateBandsFor(profile).empty(), "no fuzzy matching");
    const std::array scores {CandidateScore{true,26,0,0}, CandidateScore{false,8,100,1},
        CandidateScore{false,9,500,2}, CandidateScore{false,9,400,3}};
    check(recommendedCandidate(scores,true)==3, "A ordering and hardFail exclusion");
    check(recommendedCandidate(scores,false)==1, "unranked first admissible");
    check(!recommendedCandidate({},false), "empty result");
    const std::array failed {CandidateScore{true,26,0,0}};
    check(!recommendedCandidate(failed,true), "all hardFail");
    std::cout << "CANDIDATE_SERIES_PASS\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what(); return 1; }
