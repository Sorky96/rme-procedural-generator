// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/candidate_score.h"
#include "procedural/core/style_profile.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace rme::procedural;
namespace {
uint64_t number(std::string_view text) {
    uint64_t result = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (ec != std::errc{} || end != text.data() + text.size()) throw std::runtime_error("invalid unsigned integer");
    return result;
}
std::vector<CandidateBand> readBands(const char* path) {
    std::ifstream in(path);
    std::string line;
    if (!std::getline(in, line) || line != "g6-ranking-bands/1") throw std::runtime_error("missing g6-ranking-bands/1 header");
    std::vector<CandidateBand> bands;
    while (std::getline(in, line)) {
        if (line.empty() || line.starts_with('#')) continue;
        std::istringstream row(line);
        std::string name, extra;
        CandidateBand band;
        if (!(row >> name >> band.reference >> band.lowerHundredths >> band.upperHundredths) || row >> extra)
            throw std::runtime_error("band row requires: metric reference lowerHundredths upperHundredths");
        const auto found = std::ranges::find(candidateMetricNames, name);
        if (found == candidateMetricNames.end()) throw std::runtime_error("unknown shape metric: " + name);
        band.metric = size_t(found - candidateMetricNames.begin());
        bands.push_back(band);
    }
    CandidateMetrics dummy;
    dummy.fill(1);
    if (!scoreCandidateMetrics(dummy, bands, false, 0)) throw std::runtime_error("invalid bands (empty, duplicate, negative unsigned reference or invalid bounds)");
    std::ranges::sort(bands, {}, &CandidateBand::metric);
    if (bands.size() != candidateRankingMetricCount) throw std::runtime_error("ranking requires all 26 I51 metrics");
    for (size_t i = 0; i < bands.size(); ++i)
        if (bands[i].metric != i) throw std::runtime_error("ranking requires exactly the frozen I51 metric set");
    return bands;
}

constexpr ExitShape offlineShapes[] = {{ExitDirection::North, 0, 1, true}, {ExitDirection::West, 1, 0, true},
    {ExitDirection::South, 0, -1, true}, {ExitDirection::East, -1, 0, true}};

void usage() {
    std::cout << "candidate_rank PROFILE SIDE FIRST_USER_SEED LAST_USER_SEED [--count 1..16] [--bands FILE]\n"
        "  [--path-length 0..400] [--building-density 0..100] [--rock-blocks 0|1] [--g5-levels]\n"
        "Default count=4. Without --bands: measurements only, never a claimed ranking/gate pass.\n"
        "Bands: g6-ranking-bands/1 then metric reference lowerHundredths upperHundredths.\n"
        "Relative band [0.67*ref,1.5*ref]: lowerHundredths=67*ref, upperHundredths=150*ref.\n"
        "--g5-levels uses the existing I47 geometry settings, no retry. Offline conservative blocking only.\n"
        "All 26 I51 bands are required. NA is definitional, with a capped ln(64) penalty.\n"
        "Metrics:\n";
    for (const auto name : candidateMetricNames) std::cout << name << '\n';
}
} // namespace

int main(int argc, char** argv) try {
    if (argc == 2 && std::string_view(argv[1]) == "--help") { usage(); return 0; }
    if (argc < 5) { usage(); return 2; }
    std::ifstream in(argv[1], std::ios::binary);
    const auto profile = parseProfile(std::string((std::istreambuf_iterator<char>(in)), {}));
    if (!profile || !profile->tuning) throw std::runtime_error("cannot read profile with tuning");
    const auto sideArg = number(argv[2]), first = number(argv[3]), last = number(argv[4]);
    // Explicit offline resource limit; only one candidate world is retained at a time.
    if (sideArg < 1 || sideArg > 1024 || first > last || last - first > 4095) throw std::runtime_error("side must be 1..1024, seed range at most 4096");
    const auto side = uint32_t(sideArg);
    auto tuning = *profile->tuning;
    uint32_t count = 4;
    std::vector<CandidateBand> bands;
    bool g5 = false;
    for (int i = 5; i < argc; ++i) {
        const std::string_view key = argv[i];
        if (key == "--g5-levels") { g5 = true; continue; }
        if (++i == argc) throw std::runtime_error("missing option value");
        if (key == "--bands") { bands = readBands(argv[i]); continue; }
        const auto value = number(argv[i]);
        if (key == "--count" && value >= 1 && value <= 16) count = uint32_t(value);
        else if (key == "--path-length" && value <= 400) tuning.pathLength = uint32_t(value);
        else if (key == "--building-density" && value <= 100) tuning.buildingDensity = uint32_t(value);
        else if (key == "--rock-blocks" && value <= 1) tuning.rockBlocks = value != 0;
        else throw std::runtime_error("unknown option or out-of-range value: " + std::string(key));
    }
    if (g5) {
        tuning.ruinPercent = 20; tuning.campCount = 4; tuning.fordLength = 48; tuning.repairReach = 1;
        tuning.caveEntrances = 16; tuning.plateauMinTop = 64; tuning.plateauMaxPercent = 10; tuning.plateauLevels = 3;
        tuning.caveFarExits = true; tuning.caveDoorCorridors = true; tuning.passes = 4;
        tuning.buildingStoreys = true; tuning.buildingDungeons = true;
    }
    const uint64_t cells = uint64_t(side) * side;
    const SurfaceBudget budget {cells, cells * 8, cells * 96 + (1u << 20)};
    std::cout << "g6-candidate-report/1\n";
    std::cout << std::format("settings\tside={}\tcount={}\tpathLength={}\tbuildingDensity={}\trockBlocks={}\tg5={}\tmode={}\n",
        side, count, tuning.pathLength, tuning.buildingDensity, tuning.rockBlocks, g5, bands.empty() ? "measure-only" : "ranking");
    for (const auto& band : bands) std::cout << std::format("band\t{}\t{}\t{}\t{}\n", candidateMetricNames[band.metric],
        band.reference, band.lowerHundredths, band.upperHundredths);
    std::cout << "userSeed\tindex\tseed\tsurfaceFingerprint\thardFail\tinBand\tdistance";
    for (const auto name : candidateMetricNames) std::cout << '\t' << name;
    std::cout << '\n';
    bool scoringFailed = false;
    for (uint64_t userSeed = first;; ++userSeed) {
        std::optional<CandidateScore> best;
        bool complete = true;
        for (uint32_t index = 0; index < count; ++index) {
            const auto seed = candidateSeed(userSeed, index);
            const SurfaceConfig config {{100, 100, 7, side, side}, seed, tuning};
            auto surface = generateSurface(config, {65001, 65001, 16}, budget);
            if (!surface) throw std::runtime_error("surface generation failed");
            const auto fingerprint = surface->fingerprint();
            const auto decorTuning = profile->decorTuning.value_or(DecorTuning{});
            auto decor = generateDecoration(*surface, seed, decorTuning);
            if (!decor) throw std::runtime_error("decoration generation failed");
            auto world = buildLevelWorld(std::move(*surface), std::move(*decor), seed, tuning, decorTuning,
                offlineShapes, offlineShapes, true, {}, offlineShapes, offlineShapes);
            if (!world) throw std::runtime_error("level generation failed");
            if (tuning.repairReach) {
                auto repaired = repairLevelReach(*world, true, tuning.rockBlocks, offlineShapes, {true, true, true}, tuning.fordLength, true);
                if (!repaired) throw std::runtime_error("level repair failed");
                world->repair = *repaired;
            }
            const auto plan = levelPlanOf(world->surface, true, tuning.rockBlocks, world->cave ? &*world->cave : nullptr,
                world->exits, offlineShapes, world->caps, world->decor, {}, world->storeys, world->dungeons);
            if (!plan) throw std::runtime_error("invalid level plan");
            const auto reach = levelReachabilityOf(*plan, {true, true, true});
            if (!reach) throw std::runtime_error("reachability measurement failed");
            const auto metrics = candidateMetricsOf(world->surface);
            if (!metrics) throw std::runtime_error("shape measurement failed");
            const bool hard = candidateHardFail(world->surface.valid(), *reach, world->repair);
            std::cout << std::format("{}\t{}\t{}\t{}\t{}\t", userSeed, index, seed, fingerprint, hard);
            if (!bands.empty()) {
                const auto score = scoreCandidateMetrics(*metrics, bands, hard, index);
                if (!score) {
                    std::cout << std::format("score-error:{}\tNA", uint32_t(score.error()));
                    complete = false; scoringFailed = true;
                } else {
                    std::cout << std::format("{}\t{}", score->inBand, score->distance);
                    if (!best || betterCandidate(*score, *best)) best = *score;
                }
            } else std::cout << "NA\tNA";
            for (const auto& value : *metrics) std::cout << '\t' << (value ? std::to_string(*value) : "NA");
            std::cout << '\n';
        }
        if (!bands.empty() && complete && best) std::cout << std::format("winner\t{}\t{}\t{}\t{}\t{}\n", userSeed, best->index, best->hardFail, best->inBand, best->distance);
        if (userSeed == last) break; // UINT64_MAX is a valid user seed.
    }
    return scoringFailed ? 4 : 0;
} catch (const std::exception& error) {
    std::cerr << "candidate_rank: " << error.what() << '\n';
    return 2;
}
