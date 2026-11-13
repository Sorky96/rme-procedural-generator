// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/level_repair.h"

#include <algorithm>
#include <cstdlib>
#include <new>

namespace rme::procedural {
namespace {
constexpr uint64_t storeyStream = 0x73746f7265793433ULL; // "storey43"
constexpr uint32_t minInterior = 16;
constexpr int stepX[] = {0, 0, 1, -1}, stepY[] = {-1, 1, 0, 0}; // ExitDirection order
} // namespace

// Rectangular Floor components holding exactly one door on their outline, in row-major order of their first cell.
std::vector<BuildingFootprint> qualifyingBuildings(const SurfaceGrid& s, std::stop_token stop, bool& cancelled) {
    const auto w = s.region.width, h = s.region.height;
    const auto n = s.cells.size();
    std::vector<uint8_t> seen(n), door(n), other(n);
    for (const auto i : s.doors) door[i] = 1;
    for (const auto* list : {&s.gaps, &s.sites, &s.fires}) for (const auto i : *list) other[i] = 1;
    std::vector<BuildingFootprint> result;
    std::vector<uint32_t> queue;
    for (uint32_t first = 0; first < n; ++first) {
        if (first % 4096 == 0 && stop.stop_requested()) { cancelled = true; return {}; }
        if (seen[first] || s.cells[first] != SurfaceKind::Floor) continue;
        queue.assign(1, first);
        seen[first] = 1;
        uint32_t x0 = w, y0 = h, x1 = 0, y1 = 0, doors = 0, doorCell = 0;
        bool clean = true;
        for (size_t head = 0; head < queue.size(); ++head) {
            const auto i = queue[head], x = i % w, y = i / w;
            x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
            if (door[i]) { ++doors; doorCell = i; }
            clean &= !other[i];
            for (const auto j : {x ? i - 1 : i, x + 1 < w ? i + 1 : i, y ? i - w : i, y + 1 < h ? i + w : i})
                if (!seen[j] && s.cells[j] == SurfaceKind::Floor) { seen[j] = 1; queue.push_back(j); }
        }
        if (!clean || doors != 1 || x0 + 2 > x1 || y0 + 2 > y1) continue;
        if (uint64_t(x1 - x0 + 1) * (y1 - y0 + 1) != queue.size()) continue;
        if (uint64_t(x1 - x0 - 1) * (y1 - y0 - 1) < minInterior) continue;
        result.push_back({x0, y0, x1, y1, doorCell});
    }
    return result;
}

std::expected<void, ReachabilityError> placeBuildingStoreys(
    LevelWorld& world, uint64_t seed, const SurfaceTuning& tuning,
    std::span<const ExitShape> shapes, std::stop_token stop) try {
    if (!tuning.buildingStoreys || shapes.empty()) return {};
    if (stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
    const auto& surface = world.surface;
    if (!surface.valid() || surface.region.z < 1 || tuning.upperFloorPercent > 100 || shapes.size() > 256 ||
        world.decor.size() != 1 + size_t(world.cave.has_value()) + world.caps.size() ||
        std::ranges::any_of(shapes, [](const ExitShape& s) { return size_t(s.direction) > 3 || std::abs(s.tailDx) + std::abs(s.tailDy) > 1; }))
        return std::unexpected(ReachabilityError::InvalidGrid);
    const auto w = surface.region.width, h = surface.region.height;
    const auto n = surface.cells.size();
    bool cancelled = false;
    const auto buildings = qualifyingBuildings(surface, stop, cancelled);
    if (cancelled) return std::unexpected(ReachabilityError::Cancelled);
    // Everything on z - 1 that a storey must keep one cell away from.
    std::vector<uint8_t> taken(n);
    if (!world.caps.empty()) {
        const auto& cap = world.caps.front();
        for (size_t i = 0; i < n; ++i) taken[i] = cap.cells[i] != PlateauKind::None || cap.landings[i];
        for (const auto& r : cap.ramps) taken[r.cell] = taken[r.destination] = 1;
    }
    for (const auto& pass : world.passes) for (const auto i : pass.cells) taken[i] = 1;
    StableRandom random(seed, storeyStream);
    auto& report = world.storeyReport;
    report = {};
    report.qualifying = uint32_t(buildings.size());
    std::vector<uint8_t> open;
    std::vector<uint32_t> queue, candidates;
    for (const auto& b : buildings) {
        if (stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
        if (random.bounded(100) >= tuning.upperFloorPercent) continue;
        ++report.chosen;
        bool collides = false;
        for (uint32_t y = b.y0 ? b.y0 - 1 : 0; y <= std::min(h - 1, b.y1 + 1) && !collides; ++y)
            for (uint32_t x = b.x0 ? b.x0 - 1 : 0; x <= std::min(w - 1, b.x1 + 1); ++x)
                if (taken[size_t(y) * w + x]) { collides = true; break; }
        if (collides) { ++report.collisions; continue; }
        const auto inside = [&](int64_t x, int64_t y) { return x > b.x0 && x < b.x1 && y > b.y0 && y < b.y1; };
        const auto dx = int64_t(b.door % w), dy = int64_t(b.door / w);
        const auto innerX = dx == b.x0 ? dx + 1 : dx == b.x1 ? dx - 1 : dx;
        const auto innerY = dy == b.y0 ? dy + 1 : dy == b.y1 ? dy - 1 : dy;
        if (!inside(innerX, innerY)) { ++report.noStairs; continue; }
        const auto interior = uint64_t(b.x1 - b.x0 - 1) * (b.y1 - b.y0 - 1);
        candidates.clear();
        for (uint32_t s = 0; s < shapes.size(); ++s) {
            const auto& shape = shapes[s];
            const auto d = size_t(shape.direction);
            const bool vertical = stepX[d] == 0;
            for (int64_t y = b.y0 + 1; y < b.y1; ++y) for (int64_t x = b.x0 + 1; x < b.x1; ++x) {
                // Beside a wall that runs along the stair direction.
                if (vertical ? x != b.x0 + 1 && x != b.x1 - 1 : y != b.y0 + 1 && y != b.y1 - 1) continue;
                const int64_t tx = shape.hasTail ? x + shape.tailDx : x, ty = shape.hasTail ? y + shape.tailDy : y;
                const int64_t ex = tx - stepX[d], ey = ty - stepY[d];
                if (!inside(x + stepX[d], y + stepY[d]) || !inside(tx, ty) || !inside(ex, ey) ||
                    (ex == x && ey == y) || (ex == tx && ey == ty) || (x + stepX[d] == tx && y + stepY[d] == ty)) continue;
                if ((innerX == x && innerY == y) || (innerX == tx && innerY == ty)) continue;
                // The rest of the ground floor stays one component with the door (local footprint grid).
                const auto bw = b.x1 - b.x0 + 1;
                open.assign(size_t(bw) * (b.y1 - b.y0 + 1), 0);
                const auto local = [&](int64_t xx, int64_t yy) { return uint32_t((yy - b.y0) * bw + (xx - b.x0)); };
                for (int64_t yy = b.y0 + 1; yy < b.y1; ++yy) for (int64_t xx = b.x0 + 1; xx < b.x1; ++xx)
                    open[local(xx, yy)] = !((xx == x && yy == y) || (xx == tx && yy == ty));
                const auto start = local(innerX, innerY);
                queue.assign(1, start); open[start] = 2;
                for (size_t head = 0; head < queue.size(); ++head) {
                    const auto i = queue[head];
                    for (const auto j : {i - 1, i + 1, i - bw, i + bw}) if (open[j] == 1) { open[j] = 2; queue.push_back(j); }
                }
                if (queue.size() + (shape.hasTail ? 2 : 1) != interior) continue;
                candidates.push_back(s);
                candidates.push_back(uint32_t(y * w + x));
            }
        }
        if (candidates.empty()) { ++report.noStairs; continue; }
        const auto pick = size_t(random.bounded(candidates.size() / 2)) * 2;
        const auto shape = candidates[pick], cell = candidates[pick + 1];
        const auto d = size_t(shapes[shape].direction);
        const auto arrival = uint32_t(int64_t(cell) + stepX[d] + int64_t(stepY[d]) * w);
        world.storeys.push_back({b.x0, b.y0, b.x1, b.y1, cell, arrival, uint8_t(shape)});
        for (uint32_t y = b.y0 ? b.y0 - 1 : 0; y <= std::min(h - 1, b.y1 + 1); ++y)
            for (uint32_t x = b.x0 ? b.x0 - 1 : 0; x <= std::min(w - 1, b.x1 + 1); ++x) taken[size_t(y) * w + x] = 1;
        ++report.placed;
    }
    if (!world.storeys.empty() && world.caps.empty()) world.decor.push_back({});
    return {};
} catch (const std::bad_alloc&) { return std::unexpected(ReachabilityError::AllocationFailed); }

} // namespace rme::procedural
