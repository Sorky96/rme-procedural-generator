// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/level_repair.h"

#include <algorithm>
#include <cstdlib>
#include <new>

namespace rme::procedural {
namespace {
constexpr uint64_t dungeonStream = 0x64756e67656f3435ULL; // "dungeo45"
constexpr int stepX[] = {0, 0, 1, -1}, stepY[] = {-1, 1, 0, 0}; // ExitDirection order
constexpr uint32_t minRoomSide = 4, roomSides = 6;                // interiors 4..9
constexpr uint32_t minRooms = 2, roomCounts = 5;                  // 2..6 rooms
constexpr uint32_t minCorridor = 2, corridorLengths = 5;          // 2..6 cells between the doors
constexpr int64_t caveGap = 5, dungeonGap = 5, partGap = 3, edgeGap = 2;
constexpr uint32_t growthAttempts = 48, roomZeroAttempts = 12, descentAttempts = 6;

int64_t chebyshev(const DungeonRect& a, const DungeonRect& b) noexcept {
    const auto dx = std::max<int64_t>({0, int64_t(b.x0) - a.x1, int64_t(a.x0) - b.x1});
    const auto dy = std::max<int64_t>({0, int64_t(b.y0) - a.y1, int64_t(a.y0) - b.y1});
    return std::max(dx, dy);
}

// Cells a dungeon's walkable cell may not use, with an inclusive-rectangle query over a summed-area table.
class Forbidden {
public:
    Forbidden(uint32_t w, uint32_t h) : w_(w), h_(h), bad_(size_t(w) * h), sum_(size_t(w + 1) * (h + 1)) {}
    void mark(int64_t x0, int64_t y0, int64_t x1, int64_t y1) {
        for (auto y = std::max<int64_t>(0, y0); y <= std::min<int64_t>(h_ - 1, y1); ++y)
            for (auto x = std::max<int64_t>(0, x0); x <= std::min<int64_t>(w_ - 1, x1); ++x) bad_[size_t(y) * w_ + size_t(x)] = 1;
    }
    std::vector<uint8_t>& cells() noexcept { return bad_; }
    void rebuild() {
        for (uint32_t y = 0; y < h_; ++y) {
            uint32_t row = 0;
            for (uint32_t x = 0; x < w_; ++x) {
                row += bad_[size_t(y) * w_ + x];
                sum_[size_t(y + 1) * (w_ + 1) + x + 1] = sum_[size_t(y) * (w_ + 1) + x + 1] + row;
            }
        }
    }
    bool free(int64_t x0, int64_t y0, int64_t x1, int64_t y1) const noexcept {
        if (x0 < edgeGap || y0 < edgeGap || x1 + edgeGap >= int64_t(w_) || y1 + edgeGap >= int64_t(h_) || x0 > x1 || y0 > y1) return false;
        const auto at = [&](int64_t x, int64_t y) { return sum_[size_t(y) * (w_ + 1) + size_t(x)]; };
        return at(x1 + 1, y1 + 1) + at(x0, y0) - at(x0, y1 + 1) - at(x1 + 1, y0) == 0;
    }

private:
    uint32_t w_, h_;
    std::vector<uint8_t> bad_;
    std::vector<uint32_t> sum_;
};

// Marks cells within Chebyshev distance `radius` of any set cell (separable: rows, then columns).
std::vector<uint8_t> dilate(std::span<const uint8_t> cells, uint32_t w, uint32_t h, int64_t radius) {
    std::vector<uint8_t> rows(cells.size()), out(cells.size());
    for (uint32_t y = 0; y < h; ++y) {
        int64_t last = -(radius + 1) - 1;
        for (int64_t x = 0; x < int64_t(w) + radius; ++x) {
            if (x < int64_t(w) && cells[size_t(y) * w + size_t(x)]) last = x;
            if (x - radius >= 0 && x - radius < int64_t(w) && x - last <= 2 * radius) rows[size_t(y) * w + size_t(x - radius)] = 1;
        }
    }
    for (uint32_t x = 0; x < w; ++x) {
        int64_t last = -(radius + 1) - 1;
        for (int64_t y = 0; y < int64_t(h) + radius; ++y) {
            if (y < int64_t(h) && rows[size_t(y) * w + x]) last = y;
            if (y - radius >= 0 && y - radius < int64_t(h) && y - last <= 2 * radius) out[size_t(y - radius) * w + x] = 1;
        }
    }
    return out;
}
} // namespace

std::expected<void, ReachabilityError> placeBuildingDungeons(
    LevelWorld& world, uint64_t seed, const SurfaceTuning& tuning,
    std::span<const ExitShape> shapes, std::span<const ExitShape> storeyShapes, std::stop_token stop) try {
    if (!tuning.buildingDungeons || shapes.empty() || !world.cave) return {};
    if (stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
    const auto& surface = world.surface;
    const auto& cave = *world.cave;
    if (!surface.valid() || !cave.valid() || cave.cells.size() != surface.cells.size() || tuning.dungeonPercent > 100 || shapes.size() > 256 ||
        std::ranges::any_of(shapes, [](const ExitShape& s) { return size_t(s.direction) > 3 || std::abs(s.tailDx) + std::abs(s.tailDy) > 1; }))
        return std::unexpected(ReachabilityError::InvalidGrid);
    const auto w = surface.region.width, h = surface.region.height;
    const auto n = surface.cells.size();
    bool cancelled = false;
    const auto buildings = qualifyingBuildings(surface, stop, cancelled);
    if (cancelled) return std::unexpected(ReachabilityError::Cancelled);
    // Walkable cells stay caveGap away from cave Floor: two empty cells between the cave's and the dungeon's wall rings.
    std::vector<uint8_t> floor(n);
    for (size_t i = 0; i < n; ++i) floor[i] = cave.cells[i] == CaveKind::Floor;
    Forbidden forbidden(w, h);
    forbidden.cells() = dilate(floor, w, h, caveGap - 1);
    for (size_t i = 0; i < n; ++i) forbidden.cells()[i] |= surface.cells[i] == SurfaceKind::Preserve;
    // Cells the cave may not carve (iteration 49) take no dungeon either: walkable cells keep one cell from them, so the
    // wall ring stays off them too.
    if (!cave.blocked.empty()) {
        const auto blocked = dilate(cave.blocked, w, h, 1);
        for (size_t i = 0; i < n; ++i) forbidden.cells()[i] |= blocked[i];
    }
    forbidden.rebuild();
    StableRandom random(seed, dungeonStream);
    auto& report = world.dungeonReport;
    report = {};
    report.qualifying = uint32_t(buildings.size());
    std::vector<uint8_t> open;
    std::vector<uint32_t> queue, candidates;
    for (const auto& b : buildings) {
        if (stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
        if (random.bounded(100) >= tuning.dungeonPercent) continue;
        ++report.chosen;
        const auto inside = [&](int64_t x, int64_t y) { return x > b.x0 && x < b.x1 && y > b.y0 && y < b.y1; };
        const auto dx = int64_t(b.door % w), dy = int64_t(b.door / w);
        const auto innerX = dx == b.x0 ? dx + 1 : dx == b.x1 ? dx - 1 : dx;
        const auto innerY = dy == b.y0 ? dy + 1 : dy == b.y1 ? dy - 1 : dy;
        // Storey stairs of this building (iteration 43): the stairs and tail block the ground floor; neither they nor the
        // entry cell before them may hold the opening or the arrival.
        std::vector<uint32_t> storeyCells;
        for (const auto& s : world.storeys) if (s.x0 == b.x0 && s.y0 == b.y0 && s.shape < storeyShapes.size()) {
            const auto& shape = storeyShapes[s.shape];
            const auto tail = uint32_t(int64_t(s.stairs) + (shape.hasTail ? shape.tailDx + int64_t(shape.tailDy) * w : 0));
            const auto d = size_t(shape.direction);
            storeyCells = {s.stairs, tail, uint32_t(int64_t(tail) - stepX[d] - int64_t(stepY[d]) * w)};
        }
        const auto storeyCell = [&](int64_t x, int64_t y) { return std::ranges::find(storeyCells, uint32_t(y * w + x)) != storeyCells.end(); };
        const auto storeyBlocks = [&](int64_t x, int64_t y) {
            return !storeyCells.empty() && (uint32_t(y * w + x) == storeyCells[0] || uint32_t(y * w + x) == storeyCells[1]);
        };
        const auto bw = b.x1 - b.x0 + 1;
        const auto local = [&](int64_t xx, int64_t yy) { return uint32_t((yy - b.y0) * bw + (xx - b.x0)); };
        candidates.clear();
        for (uint32_t s = 0; s < shapes.size(); ++s) {
            // Iteration 45c: the stairs down on the surface and the stairs up below them both face north.
            if (shapes[s].direction != ExitDirection::North) continue;
            const auto d = size_t(shapes[s].direction);
            for (int64_t y = b.y0 + 1; y < b.y1; ++y) for (int64_t x = b.x0 + 1; x < b.x1; ++x) {
                // The opening stands against the wall; its stairs lead away from it.
                if (inside(x - stepX[d], y - stepY[d]) || !inside(x + stepX[d], y + stepY[d])) continue;
                if ((x == innerX && y == innerY) || storeyCell(x, y) || storeyCell(x + stepX[d], y + stepY[d])) continue;
                // The rest of the ground floor stays one component with the door (local footprint grid).
                open.assign(size_t(bw) * (b.y1 - b.y0 + 1), 0);
                uint64_t openCells = 0;
                for (int64_t yy = b.y0 + 1; yy < b.y1; ++yy) for (int64_t xx = b.x0 + 1; xx < b.x1; ++xx) {
                    open[local(xx, yy)] = !((xx == x && yy == y) || storeyBlocks(xx, yy));
                    openCells += open[local(xx, yy)];
                }
                const auto start = local(innerX, innerY);
                if (open[start] != 1) continue;
                queue.assign(1, start); open[start] = 2;
                for (size_t head = 0; head < queue.size(); ++head) {
                    const auto i = queue[head];
                    for (const auto j : {i - 1, i + 1, i - bw, i + bw}) if (open[j] == 1) { open[j] = 2; queue.push_back(j); }
                }
                if (queue.size() != openCells) continue;
                candidates.push_back(s);
                candidates.push_back(uint32_t(y * w + x));
            }
        }
        if (candidates.empty()) { ++report.noStairs; continue; }
        BuildingDungeon dungeon;
        dungeon.building = {b.x0, b.y0, b.x1, b.y1};
        // Room 0 holds the stairs, their tail, the landing and the cell beyond the tail.
        for (uint32_t attempt = 0; attempt < descentAttempts && dungeon.rooms.empty(); ++attempt) {
            const auto pick = size_t(random.bounded(candidates.size() / 2)) * 2;
            const auto& shape = shapes[candidates[pick]];
            const auto cell = candidates[pick + 1];
            const auto d = size_t(shape.direction);
            const int64_t x = cell % w, y = cell / w;
            const int64_t tx = shape.hasTail ? x + shape.tailDx : x, ty = shape.hasTail ? y + shape.tailDy : y;
            const int64_t kx0 = std::min({x, tx, x - stepX[d], tx - stepX[d]}), kx1 = std::max({x, tx, x - stepX[d], tx - stepX[d]});
            const int64_t ky0 = std::min({y, ty, y - stepY[d], ty - stepY[d]}), ky1 = std::max({y, ty, y - stepY[d], ty - stepY[d]});
            for (uint32_t r = 0; r < roomZeroAttempts; ++r) {
                const int64_t rw = minRoomSide + int64_t(random.bounded(roomSides)), rh = minRoomSide + int64_t(random.bounded(roomSides));
                if (kx1 - kx0 + 1 > rw || ky1 - ky0 + 1 > rh) continue;
                const int64_t x0 = kx1 - rw + 1 + int64_t(random.bounded(uint64_t(rw - (kx1 - kx0)))), y0 = ky1 - rh + 1 + int64_t(random.bounded(uint64_t(rh - (ky1 - ky0))));
                if (!forbidden.free(x0, y0, x0 + rw - 1, y0 + rh - 1)) continue;
                dungeon.rooms.push_back({uint32_t(x0), uint32_t(y0), uint32_t(x0 + rw - 1), uint32_t(y0 + rh - 1)});
                dungeon.descent = cell;
                dungeon.arrival = uint32_t((y + stepY[d]) * w + x + stepX[d]);
                dungeon.shape = uint8_t(candidates[pick]);
                break;
            }
        }
        if (dungeon.rooms.empty()) { ++report.noSpace; continue; }
        const auto& stair = shapes[dungeon.shape];
        const auto stairTail = stair.hasTail ? int64_t(dungeon.descent) + stair.tailDx + int64_t(stair.tailDy) * w : int64_t(dungeon.descent);
        // Further rooms hang off a random earlier room through a straight corridor, one corridor per room side.
        const auto target = minRooms + uint32_t(random.bounded(roomCounts));
        std::vector<uint8_t> sides(1);
        std::vector<DungeonRect> parts = dungeon.rooms;
        for (uint32_t attempt = 0; attempt < growthAttempts && dungeon.rooms.size() < target; ++attempt) {
            const auto parent = size_t(random.bounded(dungeon.rooms.size()));
            const auto side = uint32_t(random.bounded(4));
            const auto rw = int64_t(minRoomSide + random.bounded(roomSides)), rh = int64_t(minRoomSide + random.bounded(roomSides));
            const auto length = int64_t(minCorridor + random.bounded(corridorLengths));
            const auto width = int64_t(1 + random.bounded(2));
            if (sides[parent] & (1u << side)) continue;
            const auto& a = dungeon.rooms[parent];
            const bool horizontal = stepY[side] == 0;
            // Corridor rows (or columns) lie within both rooms' interiors.
            const int64_t lo = horizontal ? a.y0 : a.x0, hi = horizontal ? a.y1 : a.x1;
            if (hi - lo + 1 < width) continue;
            const int64_t c0 = lo + int64_t(random.bounded(uint64_t(hi - lo + 2 - width)));
            const int64_t across = horizontal ? rh : rw;
            const int64_t b0 = c0 + width - across + int64_t(random.bounded(uint64_t(across - width + 1)));
            const int64_t along = horizontal ? rw : rh;
            // Along the corridor: door A on the parent's wall, `length` corridor cells, door B, then the new room.
            const int64_t wall = side == 0 ? int64_t(a.y0) - 1 : side == 1 ? int64_t(a.y1) + 1 : side == 2 ? int64_t(a.x1) + 1 : int64_t(a.x0) - 1;
            const int64_t step = side == 1 || side == 2 ? 1 : -1;
            const int64_t doorB = wall + step * (length + 1);
            const int64_t near = doorB + step, far = doorB + step * along;
            const int64_t corridorNear = wall + step, corridorFar = wall + step * length;
            const auto rect = [&](int64_t a0, int64_t a1, int64_t s0, int64_t s1) {
                // a: along the corridor, s: across it
                return horizontal ? DungeonRect{uint32_t(std::min(a0, a1)), uint32_t(s0), uint32_t(std::max(a0, a1)), uint32_t(s1)}
                                  : DungeonRect{uint32_t(s0), uint32_t(std::min(a0, a1)), uint32_t(s1), uint32_t(std::max(a0, a1))};
            };
            if (std::min({near, far, corridorNear, corridorFar, b0}) < edgeGap) continue;
            const auto room = rect(near, far, b0, b0 + across - 1);
            const auto corridor = rect(corridorNear, corridorFar, c0, c0 + width - 1);
            const auto doorCell = [&](int64_t along) {
                return uint32_t(horizontal ? c0 * w + along : along * w + c0);
            };
            const auto innerA = horizontal ? c0 * w + (wall - step) : (wall - step) * w + c0;
            if (parent == 0 && (innerA == int64_t(dungeon.descent) || innerA == stairTail)) continue;
            const auto freeRect = [&](const DungeonRect& r) { return forbidden.free(r.x0, r.y0, r.x1, r.y1); };
            if (!freeRect(room) || !freeRect(corridor) || !forbidden.free(doorCell(wall) % w, doorCell(wall) / w, doorCell(wall) % w, doorCell(wall) / w) ||
                !forbidden.free(doorCell(doorB) % w, doorCell(doorB) / w, doorCell(doorB) % w, doorCell(doorB) / w)) continue;
            bool clear = true;
            for (size_t k = 0; k < parts.size() && clear; ++k)
                clear = chebyshev(room, parts[k]) >= partGap && (parts[k] == a || chebyshev(corridor, parts[k]) >= partGap);
            if (!clear) continue;
            dungeon.rooms.push_back(room);
            dungeon.corridors.push_back({corridor, doorCell(wall), doorCell(doorB)});
            parts.push_back(room);
            parts.push_back(corridor);
            sides[parent] |= uint8_t(1u << side);
            sides.push_back(uint8_t(1u << (side ^ 1u)));
        }
        if (dungeon.rooms.size() < minRooms) { ++report.noSpace; continue; }
        // Other dungeons keep dungeonGap from every walkable cell of this one.
        for (const auto& r : parts) forbidden.mark(int64_t(r.x0) - (dungeonGap - 1), int64_t(r.y0) - (dungeonGap - 1), int64_t(r.x1) + dungeonGap - 1, int64_t(r.y1) + dungeonGap - 1);
        forbidden.rebuild();
        world.dungeons.push_back(std::move(dungeon));
        ++report.placed;
    }
    return {};
} catch (const std::bad_alloc&) { return std::unexpected(ReachabilityError::AllocationFailed); }

} // namespace rme::procedural
