// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/level_repair.h"
#include <algorithm>
#include <limits>
#include <new>
#include <queue>
#include <tuple>

namespace rme::procedural {
namespace {
template<class Visit> void neighbours(uint32_t i, uint32_t w, uint32_t h, Visit visit) {
    const auto x = i % w, y = i / w;
    if (y) visit(i - w);
    if (x) visit(i - 1);
    if (x + 1 < w) visit(i + 1);
    if (y + 1 < h) visit(i + w);
}
std::vector<uint8_t> component(std::span<const uint8_t> open, uint32_t start, uint32_t w, uint32_t h) {
    std::vector<uint8_t> seen(open.size());
    std::vector<uint32_t> q {start};
    seen[start] = 1;
    if (!open[start]) return seen;
    for (size_t head = 0; head < q.size(); ++head)
        neighbours(q[head], w, h, [&](uint32_t j) {
            if (open[j] && !seen[j]) { seen[j] = 1; q.push_back(j); }
        });
    return seen;
}
}

std::expected<LevelRepairPath, ReachabilityError> cheapestLevelPath(
    std::span<const uint8_t> costs, uint32_t w, uint32_t h,
    std::span<const uint8_t> sources, std::span<const uint8_t> destinations,
    uint32_t fordLength, std::stop_token stop) try {
    if (stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
    const auto n = costs.size();
    if (!w || !h || uint64_t(w) * h != n || sources.size() != n || destinations.size() != n || fordLength > 256)
        return std::unexpected(ReachabilityError::InvalidGrid);
    std::vector<size_t> base(n);
    size_t states = 0;
    for (size_t i = 0; i < n; ++i) {
        if (costs[i] != 0 && costs[i] != 1 && costs[i] != 3 && costs[i] != 6)
            return std::unexpected(ReachabilityError::InvalidGrid);
        base[i] = states;
        states += costs[i] == 6 ? fordLength : 1;
    }
    constexpr auto infinity = std::numeric_limits<uint64_t>::max();
    std::vector<uint64_t> distance(states, infinity);
    std::vector<size_t> previous(states, SIZE_MAX);
    // Row-major cell precedes water run in ties, independent of insertion order.
    using Entry = std::tuple<uint64_t, uint32_t, uint32_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    for (uint32_t i = 0; i < n; ++i) if (sources[i] && costs[i] && (costs[i] != 6 || fordLength)) {
        const auto run = costs[i] == 6 ? 1u : 0u;
        const auto s = base[i];
        distance[s] = costs[i] == 3 ? 3 : costs[i] == 6 ? 6 : 0;
        queue.emplace(distance[s], i, run);
    }
    size_t visits = 0;
    while (!queue.empty()) {
        if (++visits % 4096 == 0 && stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
        const auto [d, i, run] = queue.top(); queue.pop();
        const auto state = base[i] + (run ? run - 1 : 0);
        if (distance[state] != d) continue;
        if (destinations[i]) {
            LevelRepairPath result {{i}, d};
            auto p = previous[state];
            while (p != SIZE_MAX) {
                // Bases are nondecreasing, including zero-sized forbidden water states.
                const auto cell = size_t(std::upper_bound(base.begin(), base.end(), p) - base.begin() - 1);
                result.cells.push_back(uint32_t(cell));
                p = previous[p];
            }
            std::ranges::reverse(result.cells);
            return result;
        }
        neighbours(i, w, h, [&](uint32_t j) {
            if (!costs[j]) return;
            const auto nextRun = costs[j] == 6 ? run + 1 : 0;
            if (nextRun > fordLength) return;
            const auto next = base[j] + (nextRun ? nextRun - 1 : 0);
            const auto nd = d + costs[j];
            if (nd < distance[next]) {
                distance[next] = nd; previous[next] = state;
                queue.emplace(nd, j, nextRun);
            }
        });
    }
    return LevelRepairPath {};
} catch (const std::bad_alloc&) { return std::unexpected(ReachabilityError::AllocationFailed); }

std::expected<LevelRepairReport, ReachabilityError> repairLevelReach(
    const LevelPlan& plan, std::span<uint8_t> mask,
    uint32_t remainingRepairs, std::stop_token stop) try {
    const auto w = plan.region.width, h = plan.region.height;
    const size_t n = size_t(w) * h;
    if (remainingRepairs > 64 || mask.empty() ||
        std::ranges::any_of(mask, [](uint8_t value) { return value > 2; }))
        return std::unexpected(ReachabilityError::InvalidGrid);
    LevelRepairReport result;
    for (;;) {
        auto reach = levelReachabilityOf(plan, {}, mask, stop);
        if (!reach) return std::unexpected(reach.error());
        if (reach->allReached() && reach->walkableEdges == reach->edges) return result;
        if (!reach->start || result.repairs == remainingRepairs) {
            result.unrepaired = true;
            return result;
        }
        std::optional<LevelCell> target;
        const auto consider = [&](LevelCell c) {
            if (!reach->reached[c.floor * n + c.cell] &&
                (!target || std::pair{c.floor, c.cell} < std::pair{target->floor, target->cell})) target = c;
        };
        for (const auto& edge : plan.edges) { consider(edge.a); consider(edge.b); }
        for (const auto& poi : plan.pois)
            if (poi.kind != LevelPoiKind::Cap || reach->walkable[poi.at.floor * n + poi.at.cell]) consider(poi.at);
        if (!target) { result.unrepaired = true; return result; }
        const auto [f, cell] = *target;
        auto sources = component(std::span(reach->walkable).subspan(f * n, n), cell, w, h);
        std::vector<uint8_t> costs(n), destinations(n);
        for (size_t i = 0; i < n; ++i) {
            destinations[i] = reach->reached[f * n + i];
            if (plan.floors[f].walkable[i])
                costs[i] = !mask[f * n + i] ? 1 : mask[f * n + i] == 1 ? 3 : 0;
        }
        for (const auto& edge : plan.edges)
            for (const auto pair : {std::pair{edge.a, edge.b}, std::pair{edge.b, edge.a}})
                if (pair.first.floor == f && reach->reached[pair.second.floor * n + pair.second.cell])
                    destinations[pair.first.cell] = 1;
        auto path = cheapestLevelPath(costs, w, h, sources, destinations, 0, stop);
        if (!path) return std::unexpected(path.error());
        uint32_t cleared = 0;
        for (const auto i : path->cells) if (mask[f * n + i] == 1) {
            mask[f * n + i] = 0;
            ++cleared;
        }
        if (!cleared) { result.unrepaired = true; return result; }
        ++result.repairs;
        result.removedDecor += cleared;
    }
} catch (const std::bad_alloc&) { return std::unexpected(ReachabilityError::AllocationFailed); }

std::expected<LevelWorld, ReachabilityError> buildLevelWorld(
    SurfaceGrid surface, DecorGrid decor, uint64_t seed, const SurfaceTuning& tuning,
    const DecorTuning& decorTuning, std::span<const ExitShape> caveShapes,
    std::span<const ExitShape> capShapes, bool vegetation, std::stop_token stop) try {
    if (!surface.valid()) return std::unexpected(ReachabilityError::InvalidGrid);
    LevelWorld world;
    world.surface = std::move(surface);
    const auto n = world.surface.cells.size();
    if (decor.cells.empty()) { decor.region = world.surface.region; decor.cells.resize(n); }
    if (!decor.valid() || decor.region != world.surface.region) return std::unexpected(ReachabilityError::InvalidGrid);
    world.decor.push_back(std::move(decor));
    std::vector<uint8_t> occupied(n);
    for (size_t i = 0; i < n; ++i) occupied[i] = world.decor[0].cells[i] != DecorKind::None;
    if (tuning.plateauMinTop) {
        auto cap = generatePlateau(world.surface, seed, tuning.plateauMinTop, capShapes, occupied, stop, tuning.plateauMaxPercent, true);
        if (!cap) return std::unexpected(stop.stop_requested() ? ReachabilityError::Cancelled : ReachabilityError::InvalidGrid);
        world.caps.push_back(std::move(*cap));
        for (uint32_t k = 2; k <= tuning.plateauLevels; ++k) {
            auto next = generateNextPlateau(world.caps.back(), seed ^ (uint64_t(k) << 48), capShapes, stop, tuning.plateauMaxPercent);
            if (!next) return std::unexpected(stop.stop_requested() ? ReachabilityError::Cancelled : ReachabilityError::InvalidGrid);
            if (!next->components) break;
            world.caps.push_back(std::move(*next));
        }
    }
    if (tuning.caveEntrances) {
        auto cave = generateCave(world.surface, seed, stop);
        if (!cave) return std::unexpected(stop.stop_requested() ? ReachabilityError::Cancelled : ReachabilityError::InvalidGrid);
        // Vegetation at a ramp's surface destination is removable. Keep the
        // geometric cap reservation, and clear selected endpoints below.
        std::fill(occupied.begin(), occupied.end(), 0);
        if (!world.caps.empty()) for (size_t i = 0; i < n; ++i)
            occupied[i] = world.caps[0].cells[i] == PlateauKind::Top;
        auto exits = placeCaveExits(world.surface, *cave, caveShapes, occupied, stop);
        if (!exits) return std::unexpected(stop.stop_requested() ? ReachabilityError::Cancelled : ReachabilityError::InvalidGrid);
        auto removed = removeUnenteredCaveComponents(*cave, *exits, stop);
        if (!removed) return std::unexpected(stop.stop_requested() ? ReachabilityError::Cancelled : ReachabilityError::InvalidGrid);
        world.repair.removedCaveComponents = *removed;
        world.cave = std::move(*cave); world.exits = std::move(*exits);
        world.decor.push_back({});
    }
    for (size_t k = 0; k < world.caps.size(); ++k) {
        DecorGrid d;
        if (vegetation) {
            auto generated = generatePlateauDecoration(world.caps[k], seed ^ (uint64_t(k) << 48), decorTuning, stop);
            if (!generated) return std::unexpected(stop.stop_requested() ? ReachabilityError::Cancelled : ReachabilityError::InvalidGrid);
            d = std::move(*generated);
        } else { d.region = world.caps[k].region; d.cells.resize(n); }
        world.decor.push_back(std::move(d));
    }
    // Clear all transition endpoints before testing reachability. These are
    // placement reservations, not repair paths and do not consume the repair limit.
    auto plan = levelPlanOf(world.surface, true, false, world.cave ? &*world.cave : nullptr,
        world.exits, caveShapes, world.caps, world.decor, stop);
    if (!plan) return std::unexpected(plan.error());
    for (const auto& edge : plan->edges) for (const auto c : {edge.a, edge.b})
        if (!world.decor[c.floor].cells.empty()) world.decor[c.floor].cells[c.cell] = DecorKind::None;
    return world;
} catch (const std::bad_alloc&) { return std::unexpected(ReachabilityError::AllocationFailed); }

std::expected<LevelRepairReport, ReachabilityError> repairLevelReach(
    LevelWorld& world, bool walls, bool rockBlocks, std::span<const ExitShape> shapes,
    std::array<bool, decorKindCount> blockingClasses, uint32_t fordLength, bool bridges,
    std::span<const uint8_t> blockingMask, std::stop_token stop) try {
    if (world.decor.size() != 1 + size_t(world.cave.has_value()) + world.caps.size() || fordLength > 256)
        return std::unexpected(ReachabilityError::InvalidGrid);
    const auto n = world.surface.cells.size();
    const auto w = world.surface.region.width, h = world.surface.region.height;
    if (!blockingMask.empty() && blockingMask.size() != n * world.decor.size())
        return std::unexpected(ReachabilityError::InvalidGrid);
    std::vector<uint8_t> mask(blockingMask.begin(), blockingMask.end());
    LevelRepairReport result;
    result.removedCaveComponents = world.repair.removedCaveComponents;
    // Start with the undecorated semantic floor (higher caps close their support).
    // Accept blockers in row-major order only when removal cannot split it.
    auto undecorated = levelPlanOf(world.surface, walls, rockBlocks, world.cave ? &*world.cave : nullptr,
        world.exits, shapes, world.caps, world.decor, stop);
    if (!undecorated) return std::unexpected(undecorated.error());
    const size_t capBase = world.cave ? 2 : 1;
    for (size_t f = capBase; f < world.decor.size(); ++f) {
        auto floorMask = mask.empty() ? std::span<uint8_t>{} : std::span(mask).subspan(f * n, n);
        auto filtered = preserveDecorationConnectivity(world.decor[f], undecorated->floors[f].walkable,
            blockingClasses, floorMask, stop);
        if (!filtered) return std::unexpected(filtered.error() == SurfaceError::Cancelled ? ReachabilityError::Cancelled :
            filtered.error() == SurfaceError::AllocationFailed ? ReachabilityError::AllocationFailed : ReachabilityError::InvalidGrid);
        result.removedDecor += *filtered;
    }
    const auto clearDecor = [&](size_t f, uint32_t i) {
        if (!world.decor[f].cells.empty() && world.decor[f].cells[i] != DecorKind::None) {
            world.decor[f].cells[i] = DecorKind::None; ++result.removedDecor;
        }
        if (!mask.empty()) mask[f * n + i] = 0;
    };
    for (;;) {
        auto plan = levelPlanOf(world.surface, walls, rockBlocks, world.cave ? &*world.cave : nullptr,
            world.exits, shapes, world.caps, world.decor, stop);
        if (!plan) return std::unexpected(plan.error());
        auto reach = levelReachabilityOf(*plan, blockingClasses, mask, stop);
        if (!reach) return std::unexpected(reach.error());
        if (reach->allReached() && reach->walkableEdges == reach->edges) break;
        if (!reach->start || result.repairs == 64) { result.unrepaired = true; break; }
        std::optional<LevelPoi> target;
        // Visit lower-floor transition endpoints too: an isolated cap must not
        // be removed just because the path to its surface ramp needs clearing.
        for (const auto& edge : plan->edges) for (const auto c : {edge.a, edge.b})
            if (!reach->reached[c.floor * n + c.cell] && (!target ||
                std::pair{c.floor, c.cell} < std::pair{target->at.floor, target->at.cell}))
                target = LevelPoi{c.floor == 0 ? LevelPoiKind::CaveEntrance : LevelPoiKind::Cap, c};
        for (const auto& poi : plan->pois) if (!reach->reached[poi.at.floor * n + poi.at.cell] &&
            (poi.kind != LevelPoiKind::Cap || reach->walkable[poi.at.floor * n + poi.at.cell]) &&
            (!target || std::pair{poi.at.floor, poi.at.cell} < std::pair{target->at.floor, target->at.cell})) target = poi;
        if (!target) { result.unrepaired = true; break; }
        const auto f = target->at.floor, cell = target->at.cell;
        auto sources = component(std::span(reach->walkable).subspan(f * n, n), cell, w, h);
        std::vector<uint8_t> costs(n), destinations(n);
        for (size_t i = 0; i < n; ++i) {
            destinations[i] = reach->reached[f * n + i];
            const auto kind = world.decor[f].cells.empty() ? DecorKind::None : world.decor[f].cells[i];
            if (plan->floors[f].walkable[i]) {
                if (reach->walkable[f * n + i]) costs[i] = 1;
                else if (kind != DecorKind::None && (mask.empty() || mask[f * n + i] != 2)) costs[i] = 3;
            } else if (f == 0 && (world.surface.cells[i] == SurfaceKind::Lake || world.surface.cells[i] == SurfaceKind::River)) costs[i] = 6;
        }
        // A newly cleared endpoint may itself open the Z link to the start.
        for (const auto& edge : plan->edges) for (const auto pair : {std::pair{edge.a, edge.b}, std::pair{edge.b, edge.a}})
            if (pair.first.floor == f && reach->reached[pair.second.floor * n + pair.second.cell]) destinations[pair.first.cell] = 1;
        auto path = cheapestLevelPath(costs, w, h, sources, destinations, fordLength, stop);
        if (!path) return std::unexpected(path.error());
        if (!path->cells.empty()) {
            ++result.repairs;
            for (size_t step = 0; step < path->cells.size(); ++step) {
                const auto i = path->cells[step];
                if (costs[i] == 3) clearDecor(f, i);
                if (costs[i] == 6 && (world.surface.cells[i] == SurfaceKind::River || world.surface.cells[i] == SurfaceKind::Lake)) {
                    const bool horizontal = (step && path->cells[step - 1] / w == i / w) ||
                        (step + 1 < path->cells.size() && path->cells[step + 1] / w == i / w);
                    const auto axis = horizontal ? BridgeAxis::Horizontal : BridgeAxis::Vertical;
                    if (bridges && world.surface.bridgeAxes.empty()) world.surface.bridgeAxes.resize(n);
                    const auto stamp = [&](uint32_t j) {
                        if (world.surface.cells[j] != SurfaceKind::Lake && world.surface.cells[j] != SurfaceKind::River) return;
                        world.surface.cells[j] = bridges ? SurfaceKind::Bridge : SurfaceKind::Shore;
                        if (bridges) world.surface.bridgeAxes[j] = axis;
                        result.bridges += bridges;
                        clearDecor(f, j);
                    };
                    // Water's diagonal outer borders block the two banks of a
                    // one-tile crossing. A one-cell shoulder on either side
                    // keeps the axis open through the ordinary autoborder pass.
                    stamp(i);
                    if (horizontal) {
                        if (i / w) stamp(i - w);
                        if (i / w + 1 < h) stamp(i + w);
                    } else {
                        if (i % w) stamp(i - 1);
                        if (i % w + 1 < w) stamp(i + 1);
                    }
                }
            }
            continue;
        }
        const size_t capBase = world.cave ? 2 : 1;
        if (f >= capBase) {
            // Remove the complete geometric cap, including upper components
            // supported by it. A remaining sibling keeps its own ramps.
            std::vector<uint8_t> removed(n); removed[cell] = 1;
            for (size_t k = f - capBase; k < world.caps.size(); ++k) {
                auto& cap = world.caps[k];
                std::vector<uint8_t> open(n), erase(n);
                for (size_t i = 0; i < n; ++i) open[i] = cap.cells[i] == PlateauKind::Top || cap.landings[i];
                for (const auto& ramp : cap.ramps) open[ramp.cell] = 1;
                for (uint32_t i = 0; i < n; ++i) if (removed[i] && open[i] && !erase[i]) {
                    const auto part = component(open, i, w, h);
                    for (size_t j = 0; j < n; ++j) erase[j] |= part[j];
                    ++result.removedCaps;
                    if (cap.components) --cap.components;
                }
                std::erase_if(cap.ramps, [&](const auto& r) { return erase[r.cell] || erase[r.destination]; });
                for (uint32_t i = 0; i < n; ++i) if (erase[i]) {
                    cap.cells[i] = PlateauKind::None; cap.landings[i] = 0;
                    clearDecor(k + capBase, i);
                    if (!mask.empty()) mask[(k ? k + capBase - 1 : 0) * n + i] = 0;
                }
                removed = std::move(erase);
            }
            ++result.repairs;
        } else if (f == 0) {
            std::vector<uint8_t> erase(n);
            if (world.surface.cells[cell] == SurfaceKind::Floor) {
                std::vector<uint8_t> floor(n);
                for (size_t i = 0; i < n; ++i) floor[i] = world.surface.cells[i] == SurfaceKind::Floor;
                erase = component(floor, cell, w, h);
                uint32_t minX = w, minY = h, maxX = 0, maxY = 0;
                for (uint32_t i = 0; i < n; ++i) if (erase[i]) {
                    minX = std::min(minX, i % w); maxX = std::max(maxX, i % w);
                    minY = std::min(minY, i / w); maxY = std::max(maxY, i / w);
                }
                std::erase_if(world.surface.fires, [&](uint32_t i) {
                    const bool inside = i % w >= minX && i % w <= maxX && i / w >= minY && i / w <= maxY;
                    if (inside) { world.surface.cells[i] = SurfaceKind::Grass; clearDecor(0, i); }
                    return inside;
                });
                for (uint32_t i = 0; i < n; ++i) if (erase[i]) { world.surface.cells[i] = SurfaceKind::Grass; clearDecor(0, i); }
                result.removedPois += uint32_t(std::erase_if(world.surface.doors, [&](auto i) { return erase[i]; }));
                result.removedPois += uint32_t(std::erase_if(world.surface.sites, [&](auto i) { return erase[i]; }));
                std::erase_if(world.surface.gaps, [&](auto i) { return erase[i]; });
            } else if (const auto entry = std::ranges::find(world.surface.entrances, cell); entry != world.surface.entrances.end()) {
                const auto index = uint32_t(entry - world.surface.entrances.begin());
                world.surface.entrances.erase(entry);
                std::erase_if(world.exits.exits, [&](const auto& e) { return e.entrance == index; });
                for (auto& e : world.exits.exits) if (e.entrance > index) --e.entrance;
                world.exits.skipped.clear();
                if (world.cave) {
                    auto removed = removeUnenteredCaveComponents(*world.cave, world.exits, stop);
                    if (!removed) return std::unexpected(stop.stop_requested() ? ReachabilityError::Cancelled : ReachabilityError::InvalidGrid);
                    result.removedCaveComponents += *removed;
                }
                ++result.removedPois;
            } else { result.unrepaired = true; break; }
            ++result.repairs;
        } else { result.unrepaired = true; break; }
    }
    world.repair = result;
    return result;
} catch (const std::bad_alloc&) { return std::unexpected(ReachabilityError::AllocationFailed); }
} // namespace rme::procedural
