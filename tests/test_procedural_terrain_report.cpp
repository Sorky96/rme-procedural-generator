// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/terrain_report.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

using namespace rme::procedural;

namespace {

void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

size_t blocking(const TerrainReport& report) {
    return size_t(std::ranges::count_if(report.warnings, [](const TerrainWarning& w) { return w.blocking; }));
}

bool hasWarning(const TerrainReport& report, bool block, std::string_view text) {
    return std::ranges::any_of(report.warnings, [&](const TerrainWarning& w) { return w.blocking == block && w.text.find(text) != std::string::npos; });
}

const std::string* row(const TerrainReport& report, std::string_view name) {
    for (const auto& [key, value] : report.rows) if (key == name) return &value;
    return nullptr;
}

struct Fixture {
    LevelPlan plan;
    LevelReachabilityReport reach;
    LevelRepairReport repair;
    Fixture() {
        plan.region = {100, 100, 7, 8, 8};
        plan.floors = {{7, {}, {}}, {6, {}, {}}};
        reach.start = 0;
        reach.walkable.assign(128, 1);
        reach.reached.assign(128, 1);
        reach.edges = reach.walkableEdges = 10;
        for (auto& category : reach.categories) category = {4, 4};
    }
    TerrainReportInput input(std::optional<ReachTolerance> tolerance) const {
        TerrainReportInput in;
        in.width = in.height = 8;
        in.plan = &plan; in.reach = &reach; in.repair = &repair; in.tolerance = tolerance;
        return in;
    }
    bool hardFail(std::optional<ReachTolerance> tolerance) const {
        return tolerance ? candidateHardFail(true, plan, reach, repair, *tolerance) : candidateHardFail(true, reach, repair);
    }
};

// Every cause, alone and combined at random, under both rules: Blocking warnings exist exactly on hardFail.
void parity() {
    std::mt19937 random(58);
    const std::optional<ReachTolerance> rules[] {std::nullopt, ReachTolerance {0, 0}, ReachTolerance {100, 50}, ReachTolerance {500, 1000}};
    for (int round = 0; round < 4000; ++round) {
        Fixture f;
        const auto pick = [&](int percent) { return int(random() % 100) < percent; };
        if (pick(5)) f.reach.start.reset();
        for (auto& category : f.reach.categories) if (pick(10)) category.reached = uint32_t(random() % (category.total + 1));
        if (pick(15)) f.reach.walkableEdges = uint64_t(random() % 11);
        if (pick(5)) f.repair.unrepaired = true;
        if (pick(5)) f.repair.removedPois = 1;
        if (pick(5)) f.repair.removedCaps = 2;
        if (pick(5)) f.repair.removedStoreys = 1;
        if (pick(5)) f.repair.removedDungeons = 1;
        if (pick(40)) for (int k = int(random() % 20); k > 0; --k) f.reach.reached[random() % 128] = 0;
        if (pick(10)) for (int k = int(random() % 10); k > 0; --k) f.reach.walkable[random() % 128] = 0;
        if (pick(20)) f.reach.pockets = {1, 2};
        for (const auto& rule : rules) {
            const auto report = terrainReportOf(f.input(rule));
            require((blocking(report) > 0) == f.hardFail(rule), "Blocking warnings differ from candidateHardFail in round " + std::to_string(round));
        }
    }
    // One message per cause.
    Fixture f;
    f.repair.removedPois = 1;
    f.reach.categories[size_t(LevelPoiKind::Door)].reached = 3;
    const auto report = terrainReportOf(f.input(std::nullopt));
    require(blocking(report) == 2 && hasWarning(report, true, "doors out of reach: 1/4") && hasWarning(report, true, "removed 1 place"),
        "each cause is named once");
}

void cells() {
    Fixture f;
    for (size_t i = 0; i < 6; ++i) f.reach.reached[i] = 0; // 6 of 64 ground cells = 93.75 per mille
    auto none = terrainReportOf(f.input(std::nullopt));
    require(blocking(none) == 0 && row(none, "Cells out of reach, ground floor") &&
        *row(none, "Cells out of reach, ground floor") == "6/64 (not limited)", "not limited without learned shares");
    auto tight = terrainReportOf(f.input(ReachTolerance {0, 90}));
    require(blocking(tight) == 1 && hasWarning(tight, true, "ground floor 6/64"), "over the allowance blocks");
    auto half = terrainReportOf(f.input(ReachTolerance {0, 150}));
    require(blocking(half) == 0 && hasWarning(half, false, "more than half"), "above half the allowance warns");
    auto wide = terrainReportOf(f.input(ReachTolerance {0, 200}));
    require(wide.warnings.empty(), "well within the allowance is silent");
    f.reach.pockets = {3};
    auto pockets = terrainReportOf(f.input(std::nullopt));
    require(blocking(pockets) == 0 && hasWarning(pockets, false, "Closed pockets: 1"), "pockets warn without blocking");
}

void shares() {
    TerrainReportInput in;
    in.width = 30; in.height = 10;
    in.surfaceCells[size_t(SurfaceKind::Grass)] = 101;
    in.surfaceCells[size_t(SurfaceKind::Dirt)] = 99;
    in.surfaceCells[size_t(SurfaceKind::Lake)] = 50;
    in.surfaceCells[size_t(SurfaceKind::River)] = 49;
    in.surfaceCells[size_t(SurfaceKind::Floor)] = 1;
    const auto report = terrainReportOf(in);
    uint32_t sum = 0, listed = 0;
    for (const char* name : {"Grass", "Dirt", "Rock", "Shore", "Water", "Path", "Building floor", "Bridge", "Left unchanged"})
        if (const auto* value = row(report, name)) { sum += uint32_t(std::stoul(*value)); ++listed; }
    require(listed == 4 && row(report, "Water") && row(report, "Water")->starts_with("330"), "water merges lakes and rivers");
    require(sum + listed >= 1000 && sum <= 1000 + listed, "shares add up to the area within rounding");
    require(!row(report, "Rock"), "absent kinds are not listed");
    require(!row(report, "Places reached") && report.warnings.empty(), "no levels, no reach rows or warnings");
}

void copies() {
    CopyAssessment copy;
    copy.state = CopyState::Copy;
    copy.shared = {20, 3, 40};
    copy.permille = {150, 60, 99};
    const std::string aliases[] {"home", "town", "coast"};
    TerrainReportInput in;
    in.width = in.height = 64;
    in.copy = &copy;
    in.aliases = aliases;
    auto report = terrainReportOf(in);
    require(blocking(report) == 1 && hasWarning(report, true, "Copies home"), "the copied reference is named");
    require(hasWarning(report, false, "Close to town") && hasWarning(report, false, "Close to coast"), "near references warn");
    copy.state = CopyState::Clear;
    copy.shared = {15, 3, 40};
    report = terrainReportOf(in);
    require(blocking(report) == 0 && row(report, "Similarity to home"), "clear never blocks");
    copy.state = CopyState::TooSmall;
    require(terrainReportOf(in).warnings.empty(), "a sample too small to judge gives no warning");
    in.memoryBudget = 400; in.memoryBytes = 301;
    require(hasWarning(terrainReportOf(in), false, "above 75%"), "memory above 75% warns");
    in.memoryBytes = 300;
    require(!hasWarning(terrainReportOf(in), false, "above 75%"), "memory at 75% is silent");
}

} // namespace

int main() try {
    parity();
    cells();
    shares();
    copies();
    std::cout << "TERRAIN_REPORT_PASS\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAILED " << error.what() << '\n';
    return 1;
}
