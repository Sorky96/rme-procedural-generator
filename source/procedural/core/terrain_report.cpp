// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/terrain_report.h"

#include <format>

namespace rme::procedural {

namespace {

// UTF-8 per mille sign; the core library is not compiled with /utf-8.
#define PERMILLE "\xE2\x80\xB0"

constexpr std::array<const char*, levelPoiKindCount> poiNames {
    "doors", "sites", "cave entrances", "cave exits", "mountain tops", "cave cells", "storeys", "dungeons"};

uint32_t permilleOf(uint64_t part, uint64_t whole) noexcept {
    return whole ? uint32_t((part * 1000 + whole / 2) / whole) : 0;
}

std::string mib(uint64_t bytes) {
    return std::format("{:.1f} MiB", double(bytes) / (1024.0 * 1024.0));
}

// {walkable, out of reach} on the ground floor with the caves below it, and on the floors above it; none when the
// report does not match the plan (candidateHardFail treats that as a failure).
std::optional<std::array<std::array<uint64_t, 2>, 2>> cellsOutOfReach(const LevelPlan& plan, const LevelReachabilityReport& reach) {
    const size_t cells = size_t(plan.region.width) * plan.region.height;
    if (!cells || reach.walkable.size() != cells * plan.floors.size() || reach.reached.size() != reach.walkable.size()) return std::nullopt;
    std::array<std::array<uint64_t, 2>, 2> counts {};
    for (size_t i = 0; i < reach.walkable.size(); ++i) {
        const size_t upper = plan.floors[i / cells].z < plan.region.z;
        counts[upper][0] += reach.walkable[i] != 0;
        counts[upper][1] += reach.walkable[i] && !reach.reached[i];
    }
    return counts;
}

void levelWarnings(const TerrainReportInput& in, std::vector<TerrainWarning>& out) {
    const auto& reach = *in.reach;
    const auto& repair = *in.repair;
    const auto block = [&](std::string text) { out.push_back({true, std::move(text)}); };
    if (!reach.start) block("Reachability has no start place.");
    if (repair.removedPois) block(std::format("Repair removed {} place(s).", repair.removedPois));
    if (repair.removedCaps) block(std::format("Repair removed {} mountain top(s).", repair.removedCaps));
    if (repair.removedStoreys) block(std::format("Repair removed {} storey(s).", repair.removedStoreys));
    if (repair.removedDungeons) block(std::format("Repair removed {} dungeon(s).", repair.removedDungeons));
    const auto& caps = reach.categories[size_t(LevelPoiKind::Cap)];
    if (!in.tolerance) {
        for (size_t k = 0; k < reach.categories.size(); ++k)
            if (reach.categories[k].reached != reach.categories[k].total)
                block(std::format("{} out of reach: {}/{}.", poiNames[k], reach.categories[k].total - reach.categories[k].reached, reach.categories[k].total));
        if (reach.walkableEdges != reach.edges)
            block(std::format("Unwalkable floor transitions: {}/{}.", reach.edges - std::min(reach.walkableEdges, reach.edges), reach.edges));
        if (repair.unrepaired) block("Repair gave up.");
        return;
    }
    const auto tolerance = *in.tolerance;
    const auto counts = in.plan ? cellsOutOfReach(*in.plan, reach) : std::nullopt;
    if (reach.walkableEdges > reach.edges || !counts) { block("The reachability report does not match the plan."); return; }
    if ((reach.edges - reach.walkableEdges) * 1000 > uint64_t(tolerance.capPermille) * reach.edges)
        block(std::format("Unwalkable floor transitions {}/{} exceed the allowed {}" PERMILLE ".", reach.edges - reach.walkableEdges, reach.edges, tolerance.capPermille));
    for (size_t k = 0; k < reach.categories.size(); ++k)
        if (k != size_t(LevelPoiKind::Cap) && reach.categories[k].reached != reach.categories[k].total)
            block(std::format("{} out of reach: {}/{}.", poiNames[k], reach.categories[k].total - reach.categories[k].reached, reach.categories[k].total));
    if ((caps.total - caps.reached) * 1000 > uint64_t(tolerance.capPermille) * caps.total)
        block(std::format("Mountain tops out of reach {}/{} exceed the allowed {}" PERMILLE ".", caps.total - caps.reached, caps.total, tolerance.capPermille));
    const std::array<uint32_t, 2> allowed {tolerance.walkPermille, tolerance.capPermille};
    constexpr std::array<const char*, 2> where {"on the ground floor", "above the ground floor"};
    for (size_t upper = 0; upper < 2; ++upper) {
        const auto [walkable, outside] = (*counts)[upper];
        if (outside * 1000 > uint64_t(allowed[upper]) * walkable)
            block(std::format("Cells out of reach {} {}/{} exceed the allowed {}" PERMILLE ".", where[upper], outside, walkable, allowed[upper]));
        else if (outside * 2000 > uint64_t(allowed[upper]) * walkable)
            out.push_back({false, std::format("Cells out of reach {} {}/{} use more than half of the allowed {}" PERMILLE ".", where[upper], outside, walkable, allowed[upper])});
    }
}

} // namespace

TerrainReport terrainReportOf(const TerrainReportInput& in) {
    TerrainReport report;
    auto& rows = report.rows;
    const uint64_t cells = uint64_t(in.width) * in.height;
    rows.emplace_back("Area", std::format("{} x {} ({} cells)", in.width, in.height, cells));

    uint64_t surface = 0;
    for (const auto count : in.surfaceCells) surface += count;
    if (surface) {
        const auto count = [&](SurfaceKind kind) { return in.surfaceCells[size_t(kind)]; };
        const std::pair<const char*, uint64_t> shares[] {
            {"Grass", count(SurfaceKind::Grass)}, {"Dirt", count(SurfaceKind::Dirt)}, {"Rock", count(SurfaceKind::Rock)},
            {"Shore", count(SurfaceKind::Shore)}, {"Water", count(SurfaceKind::Lake) + count(SurfaceKind::River)},
            {"Path", count(SurfaceKind::Path)}, {"Building floor", count(SurfaceKind::Floor)}, {"Bridge", count(SurfaceKind::Bridge)},
            {"Left unchanged", count(SurfaceKind::Preserve)}};
        for (const auto& [name, part] : shares)
            if (part) rows.emplace_back(name, std::format("{}" PERMILLE " ({} cells)", permilleOf(part, surface), part));
    }

    if (in.reach && in.repair) {
        const auto& reach = *in.reach;
        uint64_t reached = 0, total = 0;
        for (const auto& category : reach.categories) { reached += category.reached; total += category.total; }
        rows.emplace_back("Places reached", std::format("{}/{}", reached, total));
        for (size_t k = 0; k < reach.categories.size(); ++k)
            if (reach.categories[k].total)
                rows.emplace_back(std::format("Reached {}", poiNames[k]), std::format("{}/{}", reach.categories[k].reached, reach.categories[k].total));
        rows.emplace_back("Walkable floor transitions", std::format("{}/{}", reach.walkableEdges, reach.edges));
        if (const auto counts = in.plan ? cellsOutOfReach(*in.plan, reach) : std::nullopt) {
            const std::array<std::string, 2> limit = in.tolerance ?
                std::array<std::string, 2> {std::format("allowed {}" PERMILLE , in.tolerance->walkPermille), std::format("allowed {}" PERMILLE , in.tolerance->capPermille)} :
                std::array<std::string, 2> {"not limited", "not limited"};
            rows.emplace_back("Cells out of reach, ground floor", std::format("{}/{} ({})", (*counts)[0][1], (*counts)[0][0], limit[0]));
            rows.emplace_back("Cells out of reach, above", std::format("{}/{} ({})", (*counts)[1][1], (*counts)[1][0], limit[1]));
        }
        const auto& repair = *in.repair;
        rows.emplace_back("Repairs", std::format("{} repairs, {} bridges, removed {} tops and {} places", repair.repairs, repair.bridges,
            repair.removedCaps, repair.removedPois));
        levelWarnings(in, report.warnings);
        if (!reach.pockets.empty())
            report.warnings.push_back({false, std::format("Closed pockets: {} areas cannot be walked to.", reach.pockets.size())});
    }

    if (in.copy) {
        const auto& copy = *in.copy;
        rows.emplace_back("Similarity", copyStateName(copy.state));
        bool named = false;
        for (size_t r = 0; r < copy.shared.size() && r < copy.permille.size(); ++r) {
            const std::string alias = r < in.aliases.size() ? in.aliases[r] : std::format("reference {}", r + 1);
            rows.emplace_back(std::format("Similarity to {}", alias), std::format("{} shared windows, {}" PERMILLE , copy.shared[r], copy.permille[r]));
            if (copy.state != CopyState::Copy && copy.state != CopyState::Clear) continue;
            if (copy.shared[r] >= 16 && copy.permille[r] >= 100) {
                report.warnings.push_back({true, std::format("Copies {}: {} shared windows, {}" PERMILLE " (blocked from 16 and 100" PERMILLE ").", alias, copy.shared[r], copy.permille[r])});
                named = true;
            } else if (copy.permille[r] >= 50)
                report.warnings.push_back({false, std::format("Close to {}: {}" PERMILLE " of the copy threshold's 100" PERMILLE ".", alias, copy.permille[r])});
        }
        if (copy.hardFail() && !named) report.warnings.push_back({true, "Copy thresholds met."});
    }

    if (in.memoryBudget) {
        rows.emplace_back("Memory estimate", std::format("{} of {}", mib(in.memoryBytes), mib(in.memoryBudget)));
        if (in.memoryBytes * 4 > in.memoryBudget * 3)
            report.warnings.push_back({false, std::format("Memory estimate {} is above 75% of the budget {}.", mib(in.memoryBytes), mib(in.memoryBudget))});
    }
    if (in.elapsedMs) rows.emplace_back("Preparation time", std::format("{} ms", *in.elapsedMs));
    if (in.longestStepMs) rows.emplace_back("Longest GUI-thread step", std::format("{} ms", *in.longestStepMs));
    return report;
}

} // namespace rme::procedural
