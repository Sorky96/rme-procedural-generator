// SPDX-License-Identifier: GPL-3.0-or-later
// style_render: draws the generator's surface for one style profile as a PNG, one pixel per tile, coloured by kind
// like the reference kind maps of the probe (kind-map-zNN.png). For a side-by-side look; nothing here is a measurement.
//
//   style_render PROFILE OUT_DIR SIDE [featureSize=N|transitionWidth=N|shoreWidth=N]... SEED [SEED...]    writes OUT_DIR/<seed>.png

#include "procedural/core/png_writer.h"
#include "procedural/core/reachability.h"
#include "procedural/core/level_repair.h"
#include "procedural/core/region_graph.h"
#include "procedural/core/underground.h"
#include "procedural/core/scale_stats.h"
#include "procedural/core/settlement_stats.h"
#include "procedural/core/style_profile.h"
#include "procedural/core/surface.h"
#include "procedural/core/water_shape.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

using namespace rme::procedural;
namespace fs = std::filesystem;

uint64_t levelFingerprint(const LevelPlan& plan) {
    uint64_t hash = 14695981039346656037ULL;
    const auto word = [&](uint64_t v) {
        for (int k = 0; k < 8; ++k) { hash = (hash ^ uint8_t(v)) * 1099511628211ULL; v >>= 8; }
    };
    word(plan.region.x); word(plan.region.y); word(plan.region.z); word(plan.region.width); word(plan.region.height);
    word(plan.floors.size());
    for (const auto& f : plan.floors) {
        word(f.z); word(f.walkable.size()); for (auto v : f.walkable) word(v);
        word(f.decor.size()); for (auto v : f.decor) word(uint8_t(v));
    }
    word(plan.edges.size());
    for (const auto& e : plan.edges) { word(e.a.floor); word(e.a.cell); word(e.b.floor); word(e.b.cell); }
    word(plan.pois.size());
    for (const auto& p : plan.pois) { word(uint8_t(p.kind)); word(p.at.floor); word(p.at.cell); }
    return hash;
}

// Independent measurement of final Floor components, multi-source BFS distances
// and door neighbourhoods (after narrowing and reachability repair).
std::string caveMeasurements(const LevelWorld& world) {
    if (!world.cave) return {};
    const auto& c = *world.cave;
    const auto n = c.cells.size(); const auto w = c.region.width;
    std::vector<uint32_t> labels(n), queue;
    uint32_t components = 0;
    const auto around = [&](uint32_t i, auto visit) {
        if (i >= w) visit(i - w);
        if (i % w) visit(i - 1);
        if (i % w + 1 < w) visit(i + 1);
        if (i + w < n) visit(i + w);
    };
    for (uint32_t i = 0; i < n; ++i) if (!labels[i] && c.cells[i] == CaveKind::Floor) {
        ++components; queue.assign(1, i); labels[i] = components;
        for (size_t head = 0; head < queue.size(); ++head) around(queue[head], [&](auto j) {
            if (!labels[j] && c.cells[j] == CaveKind::Floor) { labels[j] = components; queue.push_back(j); }
        });
    }
    std::vector<uint8_t> entered(components + 1), exited(components + 1);
    std::vector<int32_t> distance(n, -1); queue.clear();
    for (const auto& e : world.exits.exits) if (!e.distant && labels[e.cell]) {
        entered[labels[e.cell]] = 1; distance[e.cell] = 0; queue.push_back(e.cell);
    }
    for (size_t head = 0; head < queue.size(); ++head) around(queue[head], [&](auto j) {
        if (distance[j] < 0 && c.cells[j] == CaveKind::Floor) { distance[j] = distance[queue[head]] + 1; queue.push_back(j); }
    });
    uint64_t sum = 0; uint32_t count = 0, minimum = UINT32_MAX, invalid = 0;
    for (const auto& e : world.exits.exits) if (e.distant) {
        exited[labels[e.cell]] = 1; ++count;
        if (distance[e.cell] < 0) minimum = 0;
        else { sum += distance[e.cell]; minimum = std::min(minimum, uint32_t(distance[e.cell])); }
    }
    for (const auto& door : c.corridors) {
        uint32_t neighbours = 0; around(door.door, [&](auto j) { neighbours += c.cells[j] == CaveKind::Floor; });
        const auto step = door.horizontal ? 1u : w;
        invalid += neighbours != 2 || c.cells[door.door - step] != CaveKind::Floor || c.cells[door.door + step] != CaveKind::Floor;
    }
    uint32_t withExit = 0;
    for (size_t k = 1; k < entered.size(); ++k) withExit += entered[k] && exited[k];
    return std::format("level cave i41 components {} far {} exits {} distanceSum {} minDistance {} corridors {} invalidDoors {}\n",
        std::ranges::count(entered, uint8_t(1)), withExit, count, sum, count ? minimum : 0, c.corridors.size(), invalid);
}

// Iteration 48: entrances in a rock wall (Rock cell) and ordinary ones, and how many have a ramp right below them.
std::string caveEntranceMeasurements(const LevelWorld& world) {
    if (!world.cave) return {};
    const auto& entrances = world.surface.entrances;
    const auto rockFace = std::ranges::count_if(entrances, [&](uint32_t e) { return world.surface.cells[e] == SurfaceKind::Rock; });
    uint32_t withRamp = 0;
    for (size_t index = 0; index < entrances.size(); ++index)
        withRamp += std::ranges::any_of(world.exits.exits, [&](const CaveExit& exit) {
            return !exit.distant && exit.entrance == index && exit.cell == entrances[index];
        });
    return std::format("level cave i48 entrances {} rockFace {} ordinary {} withRamp {}\n",
        entrances.size(), rockFace, entrances.size() - size_t(rockFace), withRamp);
}

// Iteration 47: cave shape. Floor and cavern cells of the final cave; the generator's layout for the final surface (the
// same pure function): entrances outside the component of the first one, the longest straight axis piece, tunnels, loops,
// branches, caverns, and pairs of long tunnels side by side (rule below).
std::string caveShapeMeasurements(const LevelWorld& world, uint64_t seed) {
    if (!world.cave) return {};
    CaveLayout layout;
    const auto raw = generateCave(world.surface, seed, {}, &layout);
    if (!raw) return {};
    const auto w = raw->region.width, h = raw->region.height;
    uint32_t apart = 0;
    if (!world.surface.entrances.empty()) {
        std::vector<uint8_t> seen(raw->cells.size());
        std::vector<uint32_t> queue {world.surface.entrances.front()};
        seen[queue.front()] = 1;
        for (size_t head = 0; head < queue.size(); ++head) {
            const auto i = queue[head];
            for (const auto j : {i - w, i - 1, i + 1, i + w})
                if (j < raw->cells.size() && (j / w == i / w || j % w == i % w) && !seen[j] && raw->cells[j] == CaveKind::Floor) { seen[j] = 1; queue.push_back(j); }
        }
        for (const auto e : world.surface.entrances) apart += !seen[e];
    }
    uint32_t longest = 0;
    // Long tunnels: axes whose chord (first to last cell) is at least 40 % of the side.
    struct Chord { double ax, ay, ux, uy, length; const std::vector<uint32_t>* axis; };
    std::vector<Chord> chords;
    for (const auto& axis : layout.axes) {
        longest = std::max(longest, longestStraightAxis(axis, w));
        if (axis.empty()) continue;
        const double ax = axis.front() % w, ay = axis.front() / w, dx = double(axis.back() % w) - ax, dy = double(axis.back() / w) - ay;
        const double length = std::hypot(dx, dy);
        if (length >= std::min(w, h) * 2 / 5.0) chords.push_back({ax, ay, dx / length, dy / length, length, &axis});
    }
    // A pair is side by side when the chords are within 20 degrees, at least half of the shorter tunnel's axis lies along
    // the other chord, and those cells are on average less than 32 cells from it.
    uint32_t parallel = 0;
    for (size_t a = 0; a < chords.size(); ++a) for (size_t b = 0; b < chords.size(); ++b) {
        const auto& p = chords[a]; const auto& q = chords[b];
        if (a == b || q.length > p.length || (q.length == p.length && b < a) || std::abs(p.ux * q.ux + p.uy * q.uy) < 0.94) continue;
        double sum = 0; size_t along = 0;
        for (const auto i : *q.axis) {
            const double x = double(i % w) - p.ax, y = double(i / w) - p.ay, t = x * p.ux + y * p.uy;
            if (t < 0 || t > p.length) continue;
            sum += std::abs(x * p.uy - y * p.ux); ++along;
        }
        parallel += along * 2 >= q.axis->size() && sum / double(along) < 32;
    }
    const auto& c = *world.cave;
    return std::format("level cave i47 entrances {} apart {} floor {} cavern {} longestStraight {} tunnels {} loops {} branches {} caverns {} parallelPairs {}\n",
        world.surface.entrances.size(), apart, std::ranges::count(c.cells, CaveKind::Floor), caveCavernCells(c), longest,
        layout.links.size(), layout.loops, layout.branches, raw->rooms.size(), parallel);
}

// Iteration 43: storey placement counts plus independent checks of the final world (after repair): every storey
// has stairs (ground cell inside the footprint, arrival its 4-neighbour inside, both Floor below) and no footprint
// cell lies under a cap, landing, ramp opening or pass.
std::string storeyMeasurements(const LevelWorld& world, std::span<const ExitShape> shapes) {
    const auto w = world.surface.region.width;
    std::vector<uint8_t> taken(world.surface.cells.size());
    for (const auto& cap : world.caps) {
        for (size_t i = 0; i < taken.size(); ++i) taken[i] |= cap.cells[i] != PlateauKind::None || cap.landings[i];
        for (const auto& r : cap.ramps) taken[r.cell] = taken[r.destination] = 1;
    }
    for (const auto& pass : world.passes) for (const auto i : pass.cells) taken[i] = 1;
    constexpr int sx[] = {0, 0, 1, -1}, sy[] = {-1, 1, 0, 0};
    uint32_t withStairs = 0, overlaps = 0;
    for (const auto& s : world.storeys) {
        const auto inside = [&](uint32_t i) { return i % w > s.x0 && i % w < s.x1 && i / w > s.y0 && i / w < s.y1; };
        const auto d = s.shape < shapes.size() ? size_t(shapes[s.shape].direction) : 4;
        withStairs += d < 4 && inside(s.stairs) && inside(s.arrival) && world.surface.cells[s.stairs] == SurfaceKind::Floor &&
            int64_t(s.arrival) == int64_t(s.stairs) + sx[d] + int64_t(sy[d]) * w;
        for (uint32_t y = s.y0; y <= s.y1; ++y) for (uint32_t x = s.x0; x <= s.x1; ++x) overlaps += taken[size_t(y) * w + x];
    }
    const auto& r = world.storeyReport;
    return std::format("level storeys qualifying {} chosen {} placed {} collisions {} noStairs {} removed {} final {} withStairs {} overlaps {}\n",
        r.qualifying, r.chosen, r.placed, r.collisions, r.noStairs, world.repair.removedStoreys, world.storeys.size(), withStairs, overlaps);
}

// Iteration 45: dungeon placement counts plus independent checks of the final world (after repair): stairs (opening
// against the building wall, arrival inside, stairs, tail and landing in room 0), one 4-connected walkable set per
// dungeon, doors with exactly two opposite walkable neighbours, and the gaps between the wall rings (8-neighbour ring
// of the walkable cells) of a dungeon and the cave (Floor and its wall band) or another dungeon: at least two cells.
std::string dungeonMeasurements(const LevelWorld& world, std::span<const ExitShape> shapes) {
    const auto w = world.surface.region.width, h = world.surface.region.height;
    const auto n = size_t(w) * h;
    const auto dilate = [&](std::span<const uint8_t> cells, int radius) {
        std::vector<uint8_t> out(n);
        for (uint32_t i = 0; i < n; ++i) if (cells[i])
            for (int dy = -radius; dy <= radius; ++dy) for (int dx = -radius; dx <= radius; ++dx) {
                const int64_t x = int64_t(i % w) + dx, y = int64_t(i / w) + dy;
                if (x >= 0 && y >= 0 && x < w && y < h) out[size_t(y) * w + size_t(x)] = 1;
            }
        return out;
    };
    std::vector<uint8_t> caveFloor(n);
    if (world.cave) for (size_t i = 0; i < n; ++i) caveFloor[i] = world.cave->cells[i] == CaveKind::Floor;
    // Wall ring plus two empty cells: a dungeon wall cell inside this set breaks the gap.
    const auto caveNear = dilate(caveFloor, 3);
    constexpr int sx[] = {0, 0, 1, -1}, sy[] = {-1, 1, 0, 0};
    std::vector<uint8_t> earlier(n); // earlier dungeons' wall rings plus two cells
    uint32_t withStairs = 0, connected = 0, caveCollisions = 0, overlaps = 0, badDoors = 0;
    std::string rooms;
    for (size_t k = 0; k < world.dungeons.size(); ++k) {
        const auto& d = world.dungeons[k];
        std::vector<uint8_t> walk(n);
        if (!markDungeonCells(d, w, h, walk)) { ++overlaps; continue; }
        const auto footprint = dilate(walk, 1), near = dilate(footprint, 2);
        for (size_t i = 0; i < n; ++i) {
            caveCollisions += footprint[i] && caveNear[i];
            overlaps += footprint[i] && earlier[i];
        }
        for (size_t i = 0; i < n; ++i) earlier[i] |= near[i];
        rooms += std::format("{}{}", rooms.empty() ? "" : ",", d.rooms.size());
        const auto& b = d.building;
        const auto inBuilding = [&](int64_t x, int64_t y) { return x > b.x0 && x < b.x1 && y > b.y0 && y < b.y1; };
        const auto inRoom0 = [&](int64_t x, int64_t y) {
            const auto& r = d.rooms.front(); return x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1;
        };
        if (d.shape < shapes.size()) {
            const auto& shape = shapes[d.shape];
            const auto dir = size_t(shape.direction);
            const int64_t x = d.descent % w, y = d.descent / w;
            const int64_t tx = shape.hasTail ? x + shape.tailDx : x, ty = shape.hasTail ? y + shape.tailDy : y;
            withStairs += inBuilding(x, y) && !inBuilding(x - sx[dir], y - sy[dir]) && inBuilding(x + sx[dir], y + sy[dir]) &&
                int64_t(d.arrival) == (y + sy[dir]) * w + x + sx[dir] && world.surface.cells[d.descent] == SurfaceKind::Floor &&
                inRoom0(x, y) && inRoom0(tx, ty) && inRoom0(x - sx[dir], y - sy[dir]) && inRoom0(tx - sx[dir], ty - sy[dir]);
        }
        std::vector<uint32_t> queue{d.descent};
        std::vector<uint8_t> seen(n);
        seen[d.descent] = 1;
        for (size_t head = 0; head < queue.size(); ++head) {
            const auto i = queue[head];
            for (const auto j : {i % w ? i - 1 : i, i % w + 1 < w ? i + 1 : i, i >= w ? i - w : i, i + w < n ? i + w : i})
                if (walk[j] && !seen[j]) { seen[j] = 1; queue.push_back(j); }
        }
        connected += queue.size() == size_t(std::ranges::count(walk, uint8_t(1)));
        for (const auto& c : d.corridors) for (const auto door : {c.doorA, c.doorB}) {
            const bool west = walk[door - 1], east = walk[door + 1], north = walk[door - w], south = walk[door + w];
            badDoors += !((west && east && !north && !south) || (north && south && !west && !east));
        }
    }
    const auto& r = world.dungeonReport;
    return std::format("level dungeons qualifying {} chosen {} placed {} noStairs {} noSpace {} removed {} final {} withStairs {} "
        "connected {} caveCollisions {} overlaps {} badDoors {} rooms {}\n", r.qualifying, r.chosen, r.placed, r.noStairs, r.noSpace,
        world.repair.removedDungeons, world.dungeons.size(), withStairs, connected, caveCollisions, overlaps, badDoors, rooms.empty() ? "-" : rooms);
}

// Offline 10.98 model: rocky ground is open (rockBlocks=1 models an impassable rock brush); trees, bushes and small rocks
// each have at least one Unpassable variant. No client IDs are embedded here.
std::expected<LevelPlan, ReachabilityError> offlineLevels(const SurfaceConfig& config,
    const SurfaceBudget& budget, const DecorTuning& decorTuning, LevelRepairReport& repair, std::string& vegetation) {
    auto surface = generateSurface(config, {65001, 65001, 16}, budget);
    if (!surface) return std::unexpected(ReachabilityError::InvalidGrid);
    const auto n = surface->cells.size();
    auto decor = generateDecoration(*surface, config.seed, decorTuning);
    if (!decor) return std::unexpected(ReachabilityError::InvalidGrid);
    constexpr ExitShape shapes[] = {{ExitDirection::North, 0, 1, true}, {ExitDirection::West, 1, 0, true},
        {ExitDirection::South, 0, -1, true}, {ExitDirection::East, -1, 0, true}};
    if (config.tuning.repairReach) {
        // Stairs use the same offline ramp shapes as caves and caps.
        auto world = buildLevelWorld(std::move(*surface), std::move(*decor), config.seed, config.tuning,
            decorTuning, shapes, shapes, true, {}, shapes, shapes);
        if (!world) return std::unexpected(world.error());
        auto repaired = repairLevelReach(*world, true, config.tuning.rockBlocks, shapes, {true, true, true}, config.tuning.fordLength, true);
        if (!repaired) return std::unexpected(repaired.error());
        repair = *repaired;
        if (config.tuning.passes) {
            auto plan = levelPlanOf(world->surface, true, config.tuning.rockBlocks, world->cave ? &*world->cave : nullptr,
                world->exits, shapes, world->caps, world->decor, {}, world->storeys, world->dungeons);
            if (!plan) return std::unexpected(plan.error());
            auto reach = levelReachabilityOf(*plan, {true,true,true});
            if (!reach) return std::unexpected(reach.error());
            const auto w = config.region.width;
            const auto bfs = [&](std::span<const uint8_t> open, uint32_t a, uint32_t b) {
                std::vector<int32_t> d(n,-1); std::vector<uint32_t> q{a}; d[a]=0;
                for (size_t head=0; head<q.size(); ++head) {
                    const auto i=q[head]; if (i==b) return d[i];
                    for (auto j : {i%w ? i-1 : uint32_t(n), i%w+1<w ? i+1 : uint32_t(n),
                        i>=w ? i-w : uint32_t(n), i+w<n ? i+w : uint32_t(n)})
                        if (j<n && open[j] && d[j]<0) { d[j]=d[i]+1; q.push_back(j); }
                }
                return -1;
            };
            uint64_t blocked=0, covered=0, small=0, invalid=0;
            for (const auto& pass : world->passes) {
                const auto through=bfs(std::span(reach->walkable).first(n),pass.from,pass.to);
                const auto bypass=bfs(pass.before,pass.from,pass.to);
                invalid += through<0 || bypass<0 || int64_t(through)*5>int64_t(bypass)*3;
                vegetation += std::format("level pass route through {} bypass {}\n",through,bypass);
                for (auto i:pass.cells) {
                    blocked += !reach->walkable[i];
                    for (const auto& cap:world->caps) covered += cap.cells[i]!=PlateauKind::None;
                }
            }
            for (size_t k=0;k<world->caps.size();++k) {
                const auto& cells=world->caps[k].cells;
                std::vector<uint8_t> seen(n);
                for (uint32_t i=0;i<n;++i) if (!seen[i] && cells[i]==PlateauKind::Top) {
                    std::vector<uint32_t> q{i}; seen[i]=1;
                    for (size_t head=0;head<q.size();++head) {
                        const auto p=q[head];
                        for (auto j:{p%w?p-1:uint32_t(n),p%w+1<w?p+1:uint32_t(n),p>=w?p-w:uint32_t(n),p+w<n?p+w:uint32_t(n)})
                            if (j<n && !seen[j] && cells[j]==PlateauKind::Top) {seen[j]=1;q.push_back(j);}
                    }
                    small += q.size()<(k ? 30 : config.tuning.plateauMinTop);
                }
            }
            vegetation += std::format("level passes count {} blocked {} covered {} smallCaps {} invalidRoutes {}\n",
                world->passes.size(),blocked,covered,small,invalid);
        }
        vegetation += caveMeasurements(*world);
        vegetation += caveShapeMeasurements(*world, config.seed);
        vegetation += caveEntranceMeasurements(*world);
        const auto islands = sandIslandsOf(world->surface);
        if (!islands) return std::unexpected(ReachabilityError::InvalidGrid);
        vegetation += std::format("level islands total {} nonShoreIslands {} nonShoreCells {}\n",
            islands->islands, islands->nonShoreIslands, islands->nonShoreCells);
        uint64_t surfaceGrass = 0, surfaceFlora = 0;
        for (size_t i = 0; i < n; ++i) if (world->surface.cells[i] == SurfaceKind::Grass) {
            ++surfaceGrass; surfaceFlora += world->decor[0].cells[i] != DecorKind::None;
        }
        for (size_t k = 0; k < world->caps.size(); ++k) {
            uint64_t grass = 0, flora = 0;
            for (size_t i = 0; i < n; ++i) if (world->caps[k].cells[i] == PlateauKind::Top &&
                (k + 1 == world->caps.size() || world->caps[k + 1].cells[i] == PlateauKind::None)) {
                ++grass; flora += world->decor[(world->cave ? 2 : 1) + k].cells[i] != DecorKind::None;
            }
            vegetation += std::format("level flora level {} grass {} flora {} surfaceGrass {} surfaceFlora {}\n",
                k + 1, grass, flora, surfaceGrass, surfaceFlora);
        }
        if (config.tuning.buildingStoreys) vegetation += storeyMeasurements(*world, shapes);
        if (config.tuning.buildingDungeons) vegetation += dungeonMeasurements(*world, shapes);
        return levelPlanOf(world->surface, true, config.tuning.rockBlocks, world->cave ? &*world->cave : nullptr,
            world->exits, shapes, world->caps, world->decor, {}, world->storeys, world->dungeons);
    }
    std::optional<CaveGrid> cave;
    CaveExitPlan exits;
    std::vector<uint8_t> occupied(n);
    std::vector<DecorGrid> decoration {*decor};
    if (config.tuning.caveEntrances) {
        auto generated = generateCave(*surface, config.seed);
        if (!generated) return std::unexpected(ReachabilityError::InvalidGrid);
        cave = std::move(*generated);
        for (size_t i = 0; i < n; ++i) occupied[i] = decor->cells[i] != DecorKind::None;
        auto placed = placeCaveExits(*surface, *cave, shapes, occupied);
        if (!placed) return std::unexpected(ReachabilityError::InvalidGrid);
        exits = std::move(*placed);
        if (config.tuning.caveFarExits && !placeFarCaveExits(*surface, *cave, exits, shapes, config.seed, occupied))
            return std::unexpected(ReachabilityError::InvalidGrid);
        if (config.tuning.caveDoorCorridors && !placeCaveDoorCorridors(*cave, exits, shapes))
            return std::unexpected(ReachabilityError::InvalidGrid);
        decoration.push_back({});
    }
    constexpr int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
    for (size_t i = 0; i < n; ++i) occupied[i] = decor->cells[i] != DecorKind::None;
    for (const auto& e : exits.exits) {
        if (!e.distant) {
            const auto entry = surface->entrances[e.entrance];
            decoration[0].cells[entry] = DecorKind::None;
            occupied[entry] = 1;
        }
        const auto d = size_t(shapes[e.shape].direction);
        occupied[size_t(int64_t(e.cell) + dx[d] + int64_t(dy[d]) * config.region.width)] = 1;
    }
    std::vector<PlateauGrid> caps;
    if (config.tuning.plateauMinTop) {
        auto first = generatePlateau(*surface, config.seed, config.tuning.plateauMinTop, shapes,
            occupied, {}, config.tuning.plateauMaxPercent);
        if (!first) return std::unexpected(ReachabilityError::InvalidGrid);
        caps.push_back(std::move(*first));
        for (uint32_t k = 2; k <= config.tuning.plateauLevels; ++k) {
            auto next = generateNextPlateau(caps.back(), config.seed ^ (uint64_t(k) << 48), shapes, {}, config.tuning.plateauMaxPercent);
            if (!next) return std::unexpected(ReachabilityError::InvalidGrid);
            if (!next->components) break;
            caps.push_back(std::move(*next));
        }
        for (size_t k = 0; k < caps.size(); ++k) {
            auto d = generatePlateauDecoration(caps[k], config.seed ^ (uint64_t(k) << 48), decorTuning);
            if (!d) return std::unexpected(ReachabilityError::InvalidGrid);
            for (const auto& r : caps[k].ramps) d->cells[r.cell] = DecorKind::None;
            if (k + 1 < caps.size()) for (const auto& r : caps[k + 1].ramps) {
                const auto x = int(r.cell % config.region.width), y = int(r.cell / config.region.width);
                for (int yy = std::max(0, y - 1); yy <= std::min(int(config.region.height) - 1, y + 1); ++yy)
                    for (int xx = std::max(0, x - 1); xx <= std::min(int(config.region.width) - 1, x + 1); ++xx)
                        d->cells[size_t(yy) * config.region.width + xx] = DecorKind::None;
            }
            decoration.push_back(std::move(*d));
        }
    }
    return levelPlanOf(*surface, true, config.tuning.rockBlocks, cave ? &*cave : nullptr, exits, shapes, caps, decoration);
}

int main(int argc, char** argv) {
	if (argc < 5) { std::cerr << "usage: style_render PROFILE OUT_DIR SIDE SEED...\n"; return 2; }
	std::ifstream in(argv[1], std::ios::binary);
	const std::string text((std::istreambuf_iterator<char>(in)), {});
	const auto profile = parseProfile(text);
	if (!profile || !profile->tuning) { std::cerr << "cannot read the profile\n"; return 3; }
	const fs::path outDir = argv[2];
	fs::create_directories(outDir);
	const uint32_t side = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10));
	constexpr PngColor colors[] = {{0, 0, 0}, {70, 150, 40}, {120, 80, 40}, {140, 140, 140}, {235, 215, 150}, {40, 90, 200}, {40, 90, 200}, {200, 170, 120}, {120, 120, 150}};
	const uint64_t cells = uint64_t(side) * side;
	const SurfaceBudget budget {cells, cells * 8, cells * 96 + (1u << 20)};
	SurfaceTuning tuning = *profile->tuning;
	bool caveExitShapes = false;
    bool levelReach = false;
    uint32_t retrySeeds = 0;
	int firstSeed = 4;
	// Sweep helper: leading name=value arguments override featureSize, transitionWidth or shoreWidth.
	for (; firstSeed < argc && std::string_view(argv[firstSeed]).find('=') != std::string_view::npos; ++firstSeed) {
		const std::string_view arg = argv[firstSeed];
		const std::string_view name = arg.substr(0, arg.find('='));
		const uint32_t value = static_cast<uint32_t>(std::strtoul(argv[firstSeed] + name.size() + 1, nullptr, 10));
		if (name == "levelReach") levelReach = value != 0;
        else if (name == "retrySeeds") { if (value > 16) return 2; retrySeeds = value; }
        else if (name == "caveExitShapes") caveExitShapes = arg.substr(name.size() + 1) == "ramp";
		else if (name == "caveEntranceStyle") {
			const auto style = arg.substr(name.size() + 1);
			if (style != "rockface" && style != "floorhole") return 2;
			tuning.caveEntranceStyle = style == "floorhole" ? CaveEntranceStyle::FloorHole : CaveEntranceStyle::RockFace;
		}
		else if (name == "featureSize") tuning.featureSize = value;
		else if (name == "transitionWidth") tuning.transitionWidth = value;
		else if (name == "shoreWidth") tuning.shoreWidth = value;
		else if (name == "pathLength") tuning.pathLength = value;
		else if (name == "fordLength") tuning.fordLength = value;
		else if (name == "rockPasses") tuning.rockPasses = value;
		else if (name == "shoreSmoothing") tuning.shoreSmoothing = value;
		else if (name == "shoreRadius") tuning.shoreRadius = value;
		else if (name == "repairReach") tuning.repairReach = value;
        else if (name == "caveFarExits") tuning.caveFarExits = value != 0;
        else if (name == "caveDoorCorridors") tuning.caveDoorCorridors = value != 0;
        else if (name == "passes") tuning.passes = value;
        else if (name == "buildingStoreys") tuning.buildingStoreys = value != 0;
        else if (name == "rockBlocks") tuning.rockBlocks = value != 0; // iteration 44: rock brush ground is impassable
        else if (name == "upperFloorPercent") tuning.upperFloorPercent = value;
        else if (name == "buildingDungeons") tuning.buildingDungeons = value != 0;
        else if (name == "dungeonPercent") tuning.dungeonPercent = value;
		else if (name == "ruinPercent") tuning.ruinPercent = value;
		else if (name == "campCount") tuning.campCount = value;
        else if (name == "plateauLevels") {
            tuning.plateauLevels = value;
        } else if (name == "plateauMaxPercent") {
            tuning.plateauMaxPercent = value;
        } else if (name == "plateauMinTop") {
            tuning.plateauMinTop = value;
        } else if (name == "caveEntrances") {
            tuning.caveEntrances = value;
        } else if (name == "buildingDensity") {
            tuning.buildingDensity = value;
        } else {
            std::cerr << "unknown override " << name << '\n';
            return 2;
        }
    }
    // Retry windows overlap between requested seeds. Keep a bounded cache of
    // complete deterministic attempts; each seed still uses the same ordered
    // retry selection and first-attempt fallback.
    struct CachedAttempt {
        uint64_t seed;
        LevelPlan plan;
        LevelReachabilityReport reach;
        LevelRepairReport repair;
        std::string vegetation;
    };
    std::array<std::optional<CachedAttempt>, 16> retryCache;
    for (int i = firstSeed; i < argc; ++i) {
		const uint64_t seed = std::strtoull(argv[i], nullptr, 10);
        if (levelReach) {
            std::optional<LevelReachabilityReport> first, chosen;
            std::optional<LevelPlan> firstPlan, chosenPlan;
            uint64_t used = seed;
            uint32_t attempts = 0;
            LevelRepairReport firstRepair, chosenRepair;
            std::string firstVegetation, chosenVegetation;
            for (uint32_t a = 0; a < std::max(1u, retrySeeds); ++a) {
                LevelRepairReport repair;
                std::string vegetation;
                const auto candidate = seed + a;
                auto& cached = retryCache[candidate % retryCache.size()];
                std::expected<LevelPlan, ReachabilityError> plan = std::unexpected(ReachabilityError::InvalidGrid);
                std::expected<LevelReachabilityReport, ReachabilityError> reach = std::unexpected(ReachabilityError::InvalidGrid);
                if (retrySeeds > 1 && cached && cached->seed == candidate) {
                    plan = cached->plan; reach = cached->reach; repair = cached->repair; vegetation = cached->vegetation;
                } else {
                    plan = offlineLevels({{100, 100, 7, side, side}, candidate, tuning}, budget,
                        profile->decorTuning.value_or(DecorTuning{}), repair, vegetation);
                    if (plan) reach = levelReachabilityOf(*plan, {true, true, true});
                    if (retrySeeds > 1 && reach) cached = CachedAttempt{candidate, *plan, *reach, repair, vegetation};
                }
                if (!reach) { std::cerr << "level generation failed\n"; return 10; }
                ++attempts;
                if (reach->allReached()) { used = seed + a; chosen = std::move(*reach); chosenPlan = std::move(*plan); chosenRepair = repair; chosenVegetation = std::move(vegetation); break; }
                if (!first) { first = std::move(*reach); firstPlan = std::move(*plan); firstRepair = repair; firstVegetation = std::move(vegetation); }
            }
            if (!chosen) { chosen = std::move(first); chosenPlan = std::move(firstPlan); chosenRepair = firstRepair; chosenVegetation = std::move(firstVegetation); }
            if (tuning.repairReach) std::cout << std::format("level repair seed {} repairs {} bridges {} removedCaps {} removedPois {} removedDecor {} unrepaired {}\n",
                seed, chosenRepair.repairs, chosenRepair.bridges, chosenRepair.removedCaps, chosenRepair.removedPois, chosenRepair.removedDecor, chosenRepair.unrepaired);
            if (tuning.repairReach) std::cout << std::format("level cave pruning seed {} removedComponents {}\n", seed, chosenRepair.removedCaveComponents) << chosenVegetation;
            std::cout << std::format("level seed {} used {} attempts {} all {} edges {}/{} model conservative\n",
                seed, used, attempts, chosen->allReached(), chosen->walkableEdges, chosen->edges);
            if (tuning.caveFarExits || tuning.caveDoorCorridors)
                std::cout << std::format("level fingerprint seed {} value {}\n", seed, levelFingerprint(*chosenPlan));
            constexpr std::string_view names[] = {"doors", "sites", "caveEntrances", "caveExits", "caps", "caveCells", "storeys", "dungeons"};
            for (size_t c = 0; c < levelPoiKindCount; ++c)
                std::cout << std::format("level category seed {} {} {}/{}\n", seed, names[c], chosen->categories[c].reached, chosen->categories[c].total);
            for (size_t f = 0; f < chosen->pockets.size(); ++f)
                std::cout << std::format("level pockets seed {} floor {} count {}\n", seed, f, chosen->pockets[f]);
            for (const auto& p : chosen->firstUnreachable)
                std::cout << std::format("level first seed {} kind {} at {},{},{}\n", seed, names[size_t(p.kind)], p.x, p.y, p.z);
            for (size_t c = 0; c < levelPoiKindCount; ++c) {
                for (const auto& poi : chosenPlan->pois) {
                    const auto i = size_t(poi.at.floor) * cells + poi.at.cell;
                    if (size_t(poi.kind) != c || chosen->reached[i] ||
                        (poi.kind == LevelPoiKind::Cap && !chosen->walkable[i])) continue;
                    std::cout << std::format("level cause seed {} kind {} at {},{},{} reason {}\n", seed, names[c],
                        chosenPlan->region.x + int(poi.at.cell % side), chosenPlan->region.y + int(poi.at.cell / side),
                        chosenPlan->floors[poi.at.floor].z, chosen->walkable[i] ? "disconnected" : "blocked-cell");
                    break;
                }
            }
            // Measurement-only mode: leave the historical renderer and its output unchanged.
            continue;
        }
		const SurfaceConfig config {{100, 100, 7, side, side}, seed, tuning};
		const auto grid = generateSurface(config, MapExtent {65001, 65001, 16}, budget);
		if (!grid) { std::cerr << "generation failed for seed " << seed << '\n'; return 4; }
		std::vector<uint8_t> pixels(grid->cells.size());
		for (size_t c = 0; c < pixels.size(); ++c) pixels[c] = static_cast<uint8_t>(grid->cells[c]);
		const auto png = encodeIndexedPng(side, side, colors, pixels);
		if (!png) { std::cerr << "cannot encode\n"; return 5; }
		if (const auto shapes = waterShapesOf(*grid)) {
			const RiverSummary r = summarizeRivers(shapes->rivers);
			uint64_t skeleton = 0;
			for (const auto& river : shapes->rivers) skeleton += river.skeleton;
			std::cout << std::format("seed {}: rivers {} skeleton {} T {} S {} W {} lakes {}\n", seed, r.count, skeleton, r.tortuosityPermille, r.straightPermille, r.widthMilli, shapes->lakeAreas.size());
		}
		if (const auto channels = channelShapesOf(*grid)) {
			const ChannelSummary c = summarizeChannels(*channels);
			std::cout << std::format("channels seed {}: count {} L {} W {} B {} C {}\n", seed, c.count, c.lengthPer64k, c.widthMilli, c.branchesPer100Milli, c.connectedPermille);
		}
		{
			const auto excess = lagExcessPermille(lagStatsOf(*grid));
			std::cout << std::format("scale seed {}: E4 {} E16 {} E64 {}\n", seed, excess[0], excess[1], excess[2]);
		}
		{
			const PatchStats raw = patchStatsOf(*grid);
			const PatchSummary ps = summarizePatches(raw);
			auto opt = [](const std::optional<uint32_t>& v) { return v ? std::to_string(*v) : std::string("NA"); };
			auto comp = [](const std::optional<std::array<uint32_t, 4>>& v) { return v ? std::format("{} {} {} {}", (*v)[0], (*v)[1], (*v)[2], (*v)[3]) : std::string("NA"); };
			std::cout << std::format("trans seed {}: R {} | D {}\n", seed, comp(ps.rockComposition), comp(ps.dirtComposition));
			std::cout << std::format("patch seed {}: D {} F {} P {} RB {} RF {} RS {} shares {} {} {} {} {}\n", seed, opt(ps.boundary), opt(ps.compact), opt(ps.beach), opt(ps.rockBoundary), opt(ps.rockCompact), opt(ps.rockDustShare), ps.sharePermille[0], ps.sharePermille[1], ps.sharePermille[2], ps.sharePermille[3], ps.sharePermille[4]);
		}
		if (const auto paths = pathShapesOf(*grid)) {
			const ChannelSummary p = summarizeChannels(*paths);
			uint64_t nextToRock = 0, nearRock = 0;
			for (size_t c = 0; c < grid->cells.size(); ++c) {
				if (grid->cells[c] != SurfaceKind::Path) continue;
				const int32_t x = int32_t(c % side), y = int32_t(c / side);
				bool near = false, adjacent = false;
				for (int32_t dy = -3; dy <= 3 && !near; ++dy) {
					for (int32_t dx = -3; dx <= 3; ++dx) {
						const int32_t px = x + dx, py = y + dy;
						if (px < 0 || py < 0 || px >= int32_t(side) || py >= int32_t(side)) continue;
						if (grid->cells[size_t(py) * side + size_t(px)] != SurfaceKind::Rock) continue;
						near = true;
						if (std::abs(dx) + std::abs(dy) == 1) adjacent = true;
					}
				}
				nextToRock += adjacent;
				nearRock += near;
			}
			std::cout << std::format("paths seed {}: count {} L {} W {} B {} cells {} rockadj {} rocknear {}\n", seed, p.count, p.lengthPer64k, p.widthMilli, p.branchesPer100Milli, paths->maskCells, paths->maskCells ? nextToRock * 1000 / paths->maskCells : 0, paths->maskCells ? nearRock * 1000 / paths->maskCells : 0);
			std::cout << std::format("path junctions seed {}: count {} per_100_milli {}\n", seed, paths->pathJunctions, p.junctionsPer100Milli);
		}
		{
			const BuildingShapes foundations = floorBuildingsOf(*grid);
			const BuildingSummary b = summarizeBuildings(foundations);
			std::vector<uint32_t> outlines;
			for (const Building& f : foundations.buildings) outlines.push_back(2 * (f.longSide + f.shortSide) - 4);
			std::sort(outlines.begin(), outlines.end());
			std::cout << std::format("outline seed {}: median {}\n", seed, outlines.empty() ? 0u : outlines[outlines.size() / 2]);
			std::cout << std::format("build seed {}: count {} per64k {} long {} short {}\n", seed, b.count, b.per64k, b.longMedian, b.shortMedian);
		}
		{
			const SettlementSummary t = summarizeSettlements(settlementStatsOf(*grid));
			auto opt = [](const std::optional<uint32_t>& v) { return v ? std::to_string(*v) : std::string("NA"); };
			std::cout << std::format("settle seed {}: buildings {} G {} Kn {} Ks {} Ki {} Kb {} Bp {} Cp {} Bv {} Cv {} Dh {} Dn {} Db {} Dp {} Dp0 {} S {}\n", seed, t.buildings,
				opt(t.gapMedian), opt(t.clustersPer64kMilli), opt(t.clusterSizeMedian), opt(t.aloneP), opt(t.inLargeP), opt(t.nearPathP), opt(t.pathControlP),
				opt(t.nearWaterP), opt(t.waterControlP), opt(t.withDoorP), opt(t.doorsMedian), opt(t.boundaryDoorP), opt(t.facingP), opt(t.facingChanceP), t.facingSet);
		}
		if (!grid->doors.empty()) {
			// A door is "isolated" when the Path component (8-connected) beside it holds no other door.
			std::vector<int32_t> label(grid->cells.size(), -1);
			std::vector<uint32_t> doorsIn;
			std::vector<size_t> stack;
			const size_t w = side;
			auto stoopOf = [&](uint32_t door) -> int64_t {
				for (const int64_t step : {int64_t(-1), int64_t(1), -int64_t(w), int64_t(w)}) {
					const int64_t j = int64_t(door) + step;
					if (j >= 0 && size_t(j) < grid->cells.size() && grid->cells[size_t(j)] == SurfaceKind::Path) return j;
				}
				return -1;
			};
			for (size_t start = 0; start < label.size(); ++start) {
				if (label[start] >= 0 || grid->cells[start] != SurfaceKind::Path) continue;
				const int32_t id = int32_t(doorsIn.size());
				doorsIn.push_back(0);
				stack.assign(1, start);
				label[start] = id;
				while (!stack.empty()) {
					const size_t i = stack.back();
					stack.pop_back();
					const int64_t x = int64_t(i % w), y = int64_t(i / w);
					for (int64_t dy = -1; dy <= 1; ++dy) for (int64_t dx = -1; dx <= 1; ++dx) {
						const int64_t nx = x + dx, ny = y + dy;
						if (nx < 0 || ny < 0 || nx >= int64_t(w) || ny >= int64_t(w)) continue;
						const size_t j = size_t(ny) * w + size_t(nx);
						if (label[j] < 0 && grid->cells[j] == SurfaceKind::Path) { label[j] = id; stack.push_back(j); }
					}
				}
			}
			uint32_t bare = 0, stub = 0;
			for (const uint32_t door : grid->doors) if (const int64_t s = stoopOf(door); s >= 0) ++doorsIn[size_t(label[size_t(s)])];
			for (const uint32_t door : grid->doors) {
				const int64_t s = stoopOf(door);
				if (s < 0) ++bare;
				else if (doorsIn[size_t(label[size_t(s)])] == 1) ++stub;
			}
			std::cout << std::format("access seed {}: doors {} bare {} stub {} barePermille {} stubPermille {}\n", seed, grid->doors.size(), bare, stub,
				bare * 1000 / grid->doors.size(), stub * 1000 / grid->doors.size());
		}
		// Path cells that exist only because of fords: the same surface generated without them.
		uint64_t fordCells = 0;
		if (tuning.fordLength) {
			SurfaceTuning plain = tuning;
			plain.fordLength = 0;
			const auto without = generateSurface(SurfaceConfig {config.region, seed, plain}, MapExtent {65001, 65001, 16}, budget);
			if (!without) { std::cerr << "generation failed for seed " << seed << '\n'; return 4; }
			for (size_t c = 0; c < grid->cells.size(); ++c) fordCells += grid->cells[c] != without->cells[c];
		}
		// Post-hoc diagnostic (not in the iteration 13 criteria): doors with no walkable non-Floor neighbour.
		uint32_t doorBlocked = 0;
		for (const uint32_t door : grid->doors) {
			bool open = false;
			for (const int64_t step : {int64_t(-1), int64_t(1), -int64_t(side), int64_t(side)}) {
				const int64_t j = int64_t(door) + step;
				if (j < 0 || size_t(j) >= grid->cells.size()) continue;
				const SurfaceKind kind = grid->cells[size_t(j)];
				open = open || kind == SurfaceKind::Grass || kind == SurfaceKind::Dirt || kind == SurfaceKind::Rock || kind == SurfaceKind::Shore || kind == SurfaceKind::Path;
			}
			doorBlocked += !open;
		}
		if (!grid->entrances.empty()) {
			// Iteration 27: the cave below the entrances and its reachability from the start.
			const auto cave = generateCave(*grid, seed);
			if (cave) {
				// Iteration 47: the cave floor for review (rock, floor, entrance), next to the surface image.
				constexpr PngColor caveColors[] = {{45, 45, 50}, {205, 175, 125}, {220, 40, 40}};
				std::vector<uint8_t> floor(cave->cells.size());
				for (size_t c = 0; c < floor.size(); ++c) floor[c] = cave->cells[c] == CaveKind::Floor;
				for (const auto e : grid->entrances) floor[e] = 2;
				if (const auto image = encodeIndexedPng(side, side, caveColors, floor)) {
					std::ofstream file(outDir / std::format("{}-cave.png", seed), std::ios::binary);
					file.write(reinterpret_cast<const char*>(image->data()), static_cast<std::streamsize>(image->size()));
				}
			}
			const auto under = cave ? undergroundReachabilityOf(*grid, *cave, true) : std::unexpected(UndergroundError::InvalidGrid);
			if (under) std::cout << std::format("under seed {}: entrances {} entrancesReached {} caveCells {} caveCellsPer64k {} components {} pockets {} reachedPermille {}", seed, under->entrances, under->entrancesReached,
				under->caveCells, under->caveCells * 65536 / grid->cells.size(), under->caveComponents, under->pockets, under->caveCells ? under->caveReachedCells * 1000 / under->caveCells : 0);
			if (caveExitShapes) {
				constexpr ExitShape ramp[] = {
					{ExitDirection::North, 0, 1, true}, {ExitDirection::West, 1, 0, true},
					{ExitDirection::South, 0, -1, true}, {ExitDirection::East, -1, 0, true}};
				// This tool does not generate decoration, so no occupied mask is available.
				const auto exits = cave ? placeCaveExits(*grid, *cave, ramp) : std::unexpected(UndergroundError::InvalidGrid);
				if (!exits) { std::cerr << "cave exit placement failed\n"; return 6; }
				size_t ruleOk = 0, exitsUnderEntrance = 0, destinationsOpen = 0, landingsFloor = 0;
				for (const auto e : grid->entrances) ruleOk += caveEntranceFits(grid->cells, side, side, e, tuning.caveEntranceStyle);
				constexpr int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
				for (const auto& exit : exits->exits) {
					const auto d = size_t(ramp[exit.shape].direction);
					const auto dest = size_t(int64_t(exit.cell) + dx[d] + int64_t(dy[d]) * side);
					const auto landing = size_t(int64_t(exit.cell) - dx[d] - int64_t(dy[d]) * side);
					exitsUnderEntrance += exit.cell == grid->entrances[exit.entrance];
					const auto kind = grid->cells[dest];
					destinationsOpen += kind == SurfaceKind::Grass || kind == SurfaceKind::Dirt || kind == SurfaceKind::Shore || kind == SurfaceKind::Path;
					landingsFloor += cave->cells[landing] == CaveKind::Floor;
				}
				if (ruleOk != grid->entrances.size() || exitsUnderEntrance != exits->exits.size() || destinationsOpen != exits->exits.size() || landingsFloor != exits->exits.size()) return 7;
				if (under) std::cout << std::format(" exits {} skipped {} ruleOk {} exitsUnderEntrance {} destinationsOpen {} landingsFloor {}", exits->exits.size(), exits->skipped.size(), ruleOk, exitsUnderEntrance, destinationsOpen, landingsFloor);
			}
			if (under) std::cout << '\n';
		}

        if (tuning.plateauMinTop) {
            constexpr ExitShape ramp[]
                = {{ExitDirection::North, 0, 1, true},
                   {ExitDirection::West, 1, 0, true},
                   {ExitDirection::South, 0, -1, true},
                   {ExitDirection::East, -1, 0, true}};
            std::vector<uint8_t> occupied(grid->cells.size());
            if (caveExitShapes && !grid->entrances.empty()) {
                auto cave = generateCave(*grid, seed);
                if (!cave) {
                    return 8;
                }
                auto exits = placeCaveExits(*grid, *cave, ramp);
                if (!exits) {
                    return 8;
                }
                constexpr int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
                for (const auto& e : exits->exits) {
                    auto d = size_t(ramp[e.shape].direction);
                    occupied[size_t(int64_t(e.cell) + dx[d] + int64_t(dy[d]) * side)] = 1;
                }
            }
            auto p = generatePlateau(*grid, seed, tuning.plateauMinTop, ramp, occupied, {}, tuning.plateauMaxPercent);
            if (!p) {
                return 8;
            }
            uint64_t lowerCovered = uint64_t(side) * side;
            std::vector<PlateauKind> lowerCells;
            std::vector<uint8_t> lowerLandings, lowerReached(grid->cells.size(), 1), lowerForbidden;
            std::ofstream mountainData(outDir / std::format("{}.mountains", seed), std::ios::binary);
            for (uint32_t level = 1; level <= tuning.plateauLevels; ++level) {
                std::vector<uint8_t> reached(grid->cells.size());
                std::vector<uint32_t> q, counts(p->components);
                size_t ruleOk = 0;
                constexpr int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
                for (const auto& r : p->ramps) {
                    ++counts[r.component];
                    q.push_back(r.destination);
                    reached[r.destination] = 1;
                    auto s = ramp[r.shape];
                    auto d = size_t(s.direction);
                    auto dest = size_t(int64_t(r.cell) + dx[d] + int64_t(dy[d]) * side);
                    auto open = [&](size_t i) {
                        if (level > 1) {
                            return lowerCells[i] != PlateauKind::None && !lowerLandings[i] && !lowerForbidden[i];
                        }
                        auto k = grid->cells[i];
                        if (occupied[i]) {
                            return false;
                        }
                        for (const auto* forbidden : {&grid->doors, &grid->gaps, &grid->fires, &grid->entrances}) {
                            if (std::ranges::find(*forbidden, i) != forbidden->end()) {
                                return false;
                            }
                        }
                        return k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Path;
                    };
                    for (const auto& other : p->ramps) {
                        if (&other != &r) {
                            if (std::max(
                                    std::abs(int(other.cell % side) - int(r.cell % side)),
                                    std::abs(int(other.cell / side) - int(r.cell / side))
                                )
                                < 16) {
                                return 9;
                            }
                        }
                    }
                    auto tail = size_t(int64_t(r.cell) + s.tailDx + int64_t(s.tailDy) * side);
                    const bool supported = level == 1 ? grid->cells[dest] == SurfaceKind::Rock : lowerCells[dest] == PlateauKind::Top;
                    const bool lowerRule = level == 1
                        || (lowerCells[r.cell] == PlateauKind::Top && p->cells[r.cell] == PlateauKind::None && lowerReached[r.cell]
                            && (!s.hasTail || lowerReached[tail]));
                    ruleOk += supported && lowerRule && open(r.cell) && (!s.hasTail || open(tail)) && dest == r.destination
                        && (p->cells[dest] == PlateauKind::Top || p->landings[dest]);
                }
                for (size_t i = 0; i < q.size(); ++i) {
                    int x = int(q[i] % side), y = int(q[i] / side);
                    for (size_t d = 0; d < 4; ++d) {
                        int xx = x + dx[d], yy = y + dy[d];
                        if (xx < 0 || yy < 0 || xx >= int(side) || yy >= int(side)) {
                            continue;
                        }
                        auto j = size_t(yy) * side + xx;
                        if (!reached[j] && (p->cells[j] != PlateauKind::None)) {
                            reached[j] = 1;
                            q.push_back(uint32_t(j));
                        }
                    }
                }
                size_t top = 0, accessible = 0;
                for (size_t i = 0; i < reached.size(); ++i) {
                    if (p->cells[i] == PlateauKind::Top) {
                        ++top;
                        accessible += reached[i];
                    }
                }
                if (ruleOk != p->ramps.size() || accessible != top || std::ranges::find(counts, 0) != counts.end()) {
                    return 9;
                }
                std::cout << std::format(
                    "plateau seed {} level {}: plateaus {} top {} ramps {} reached {} ruleOk {}\n", seed, level, p->components, top,
                    p->ramps.size(), accessible, ruleOk
                );
                const auto covered = std::ranges::count_if(p->cells, [](auto k) { return k != PlateauKind::None; });
                if (tuning.plateauMaxPercent && uint64_t(covered) * 100 > lowerCovered * tuning.plateauMaxPercent) {
                    return 9;
                }
                size_t reachedAll = 0;
                std::vector<uint8_t> seen(p->cells.size());
                for (size_t i = 0; i < p->cells.size(); ++i) {
                    if (p->cells[i] != PlateauKind::None) {
                        reachedAll += reached[i];
                    }
                    if (p->cells[i] == PlateauKind::Top && !seen[i]) {
                        std::vector<size_t> component {i};
                        seen[i] = 1;
                        for (size_t k = 0; k < component.size(); ++k) {
                            int x = int(component[k] % side), y = int(component[k] / side);
                            for (size_t d = 0; d < 4; ++d) {
                                int xx = x + dx[d], yy = y + dy[d];
                                if (xx < 0 || yy < 0 || xx >= int(side) || yy >= int(side)) {
                                    continue;
                                }
                                auto j = size_t(yy) * side + xx;
                                if (!seen[j] && p->cells[j] == PlateauKind::Top) {
                                    seen[j] = 1;
                                    component.push_back(j);
                                }
                            }
                        }
                        if (level > 1 && component.size() < 30) {
                            return 9;
                        }
                    }
                }
                if (reachedAll != size_t(covered)) {
                    return 9;
                }
                std::cout << std::format(
                    "plateau area seed {} level {}: cells {} reachedAll {} inset {}\n", seed, level, covered, reachedAll, p->inset
                );
                auto next = level < tuning.plateauLevels
                    ? generateNextPlateau(*p, seed ^ (uint64_t(level + 1) << 48), ramp, {}, tuning.plateauMaxPercent)
                    : std::expected<PlateauGrid, SurfaceError>(PlateauGrid{});
                if (!next) return 8;
                auto capDecor = generatePlateauDecoration(*p, seed ^ (uint64_t(level - 1) << 48), profile->decorTuning.value_or(DecorTuning{}));
                auto surfaceDecor = generateDecoration(*grid, seed, profile->decorTuning.value_or(DecorTuning{}));
                if (!capDecor || !surfaceDecor) return 8;
                // Mirror the adapter's vegetation clearance for ramps on the next terrace.
                for (const auto& r : next->ramps) {
                    int x = int(r.cell % side), y = int(r.cell / side);
                    for (int yy = std::max(0, y - 1); yy <= std::min(int(side) - 1, y + 1); ++yy)
                        for (int xx = std::max(0, x - 1); xx <= std::min(int(side) - 1, x + 1); ++xx)
                            capDecor->cells[size_t(yy) * side + xx] = DecorKind::None;
                }
                size_t grass = 0, flora = 0, surfaceGrass = 0, surfaceFlora = 0;
                for (size_t i = 0; i < p->cells.size(); ++i) {
                    // Explicit semantic bytes, with no structure padding or platform endianness.
                    mountainData.put(char(p->cells[i]));
                    mountainData.put(char(p->landings[i]));
                    const bool cliff = !next->cells.empty() && next->cells[i] == PlateauKind::Top;
                    mountainData.put(char(cliff));
                    mountainData.put(char(cliff ? DecorKind::None : capDecor->cells[i]));
                    if (p->cells[i] == PlateauKind::Top && (next->cells.empty() || next->cells[i] == PlateauKind::None)) {
                        ++grass; flora += capDecor->cells[i] != DecorKind::None;
                    }
                    if (grid->cells[i] == SurfaceKind::Grass) { ++surfaceGrass; surfaceFlora += surfaceDecor->cells[i] != DecorKind::None; }
                }
                for (const auto& r : p->ramps)
                    mountainData << std::format("{} {} {} {}\n", r.cell, r.destination, r.component, r.shape);
                if (!mountainData) return 5;
                std::cout << std::format("plateau flora seed {} level {}: grass {} flora {} surfaceGrass {} surfaceFlora {}\n", seed, level, grass, flora, surfaceGrass, surfaceFlora);
                if (level < tuning.plateauLevels) {
                    lowerCovered = uint64_t(covered);
                    lowerCells = p->cells;
                    lowerLandings = p->landings;
                    lowerReached = reached;
                    lowerForbidden.assign(p->cells.size(), 0);
                    for (const auto& r : p->ramps) {
                        lowerForbidden[r.destination] = 1;
                        lowerForbidden[r.cell] = 1;
                        const auto tail = 2 * int64_t(r.cell) - int64_t(r.destination);
                        if (tail >= 0 && size_t(tail) < lowerForbidden.size()) {
                            lowerForbidden[size_t(tail)] = 1;
                        }
                    }
                    p = std::move(next);
                    if (!p) {
                        return 8;
                    }
                }
            }
        }
        if (const auto graph = buildRegionGraph(*grid, true)) {
			// Iteration 24: size of the graph, POIs without a route from the start, travel distances and whether the adjacency graph is connected.
			uint32_t land = 0, rock = 0, water = 0, unreached = 0, maxExit = 0, maxEntrance = 0;
			for (const auto& node : graph->regions) { land += node.kind == RegionClass::Land; rock += node.kind == RegionClass::Rock; water += node.kind == RegionClass::Water; }
			for (const auto& poi : graph->pois) {
				if (poi.distance < 0) { ++unreached; continue; }
				if (poi.kind == PoiKind::Exit) maxExit = std::max<uint32_t>(maxExit, uint32_t(poi.distance));
				if (poi.kind == PoiKind::Entrance) maxEntrance = std::max<uint32_t>(maxEntrance, uint32_t(poi.distance));
			}
			std::vector<size_t> parent(graph->regions.size());
			for (size_t i = 0; i < parent.size(); ++i) parent[i] = i;
			const auto find = [&](size_t i) { while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
			for (const auto& edge : graph->edges) parent[find(edge.a)] = find(edge.b);
			size_t components = 0;
			for (size_t i = 0; i < parent.size(); ++i) components += find(i) == i;
			std::cout << std::format("graph seed {}: regions {} land {} rock {} water {} edges {} fragments {} unreached {} maxExit {} maxEntrance {} connected {}\n", seed, graph->regions.size(), land, rock, water,
				graph->edges.size(), graph->smallFragments, unreached, maxExit, maxEntrance, components <= 1 ? 1 : 0);
		}
		if (!grid->sites.empty() || !grid->gaps.empty()) {
			// Iteration 25: ruins (sites inside a foundation larger than 3 x 3) and camps (3 x 3 tents), gaps per ruin, sites reached from the start.
			const std::vector<BuildingBox> boxes = floorBoxesOf(*grid);
			uint32_t camps = 0, ruins = 0;
			for (const uint32_t site : grid->sites) {
				const uint32_t sx = site % side, sy = site / side;
				for (const BuildingBox& box : boxes) if (sx >= box.x0 && sx <= box.x1 && sy >= box.y0 && sy <= box.y1) { (box.x1 - box.x0 == 2 && box.y1 - box.y0 == 2 ? camps : ruins) += 1; break; }
			}
			const auto poi = poiReachabilityOf(*grid, true);
			std::cout << std::format("ruin seed {}: buildings {} doors {} ruins {} camps {} gaps {} gapsPerRuin {} sites {} sitesReached {}\n", seed, boxes.size() - camps, grid->doors.size(), ruins, camps, grid->gaps.size(),
				ruins ? (grid->gaps.size() - 8ull * camps) / ruins : 0, grid->sites.size(), poi ? poi->sitesReached : 0);
		}
		if (const auto poi = poiReachabilityOf(*grid, true); poi && poi->hasStart) {
			std::cout << std::format("poi seed {}: entrances {} entrancesReached {} exits {} exitsReached {} startPermille {} allReached {}\n", seed, poi->entrances, poi->entrancesReached,
				poi->exits, poi->exitsReached, poi->startComponent * 1000 / std::max<uint64_t>(poi->walkable, 1), poi->entrances == poi->entrancesReached && poi->exits == poi->exitsReached ? 1 : 0);
		}
		if (const auto reach = reachabilityOf(*grid, true); reach && reach->walkable) {
			std::cout << std::format("reach seed {}: walkable {} components {} largest {} largestPermille {} cutOff {} cutOffPer64k {} pockets {} buildings {} sealed {} doorBlocked {} fordCells {}\n", seed,
				reach->walkable, reach->components, reach->largest, reach->largest * 1000 / reach->walkable, reach->cutOff, reach->cutOff * 65536 / reach->cells, reach->pockets,
				reach->buildings, reach->sealed, doorBlocked, fordCells);
		}
		{
			// Iteration 15: Path cells with Rock within 3 cells on both sides along one axis (a trail cutting through a rock band).
			uint64_t cut = 0;
			const auto rockWithin = [&](int64_t x, int64_t y, int64_t dx, int64_t dy) {
				for (int64_t k = 1; k <= 3; ++k) {
					const int64_t px = x + dx * k, py = y + dy * k;
					if (px < 0 || py < 0 || px >= int64_t(side) || py >= int64_t(side)) return false;
					if (grid->cells[size_t(py) * side + size_t(px)] == SurfaceKind::Rock) return true;
				}
				return false;
			};
			for (size_t c = 0; c < grid->cells.size(); ++c) {
				if (grid->cells[c] != SurfaceKind::Path) continue;
				const int64_t x = int64_t(c % side), y = int64_t(c / side);
				if ((rockWithin(x, y, -1, 0) && rockWithin(x, y, 1, 0)) || (rockWithin(x, y, 0, -1) && rockWithin(x, y, 0, 1))) ++cut;
			}
			std::cout << std::format("cut seed {}: cut {} cutPer64k {}\n", seed, cut, cut * 65536 / grid->cells.size());
		}
		{
			// Iteration 17: trail tips (a Path cell with at most 3 Path cells among its 8 neighbours) lying in rock or at water.
			uint64_t tipRock = 0, tipWater = 0;
			const int64_t w = int64_t(side);
			for (size_t c = 0; c < grid->cells.size(); ++c) {
				if (grid->cells[c] != SurfaceKind::Path) continue;
				const int64_t x = int64_t(c % side), y = int64_t(c / side);
				int paths = 0, rocks = 0, waters = 0;
				for (int64_t dy = -1; dy <= 1; ++dy) for (int64_t dx = -1; dx <= 1; ++dx) {
					if (!dx && !dy) continue;
					const int64_t nx = x + dx, ny = y + dy;
					if (nx < 0 || ny < 0 || nx >= w || ny >= w) continue;
					const SurfaceKind kind = grid->cells[size_t(ny) * side + size_t(nx)];
					paths += kind == SurfaceKind::Path;
					rocks += kind == SurfaceKind::Rock;
					waters += kind == SurfaceKind::River || kind == SurfaceKind::Lake;
				}
				if (paths > 3) continue;
				tipRock += rocks >= 3;
				tipWater += waters >= 3;
			}
			std::cout << std::format("tips seed {}: tipRock {} tipWater {}\n", seed, tipRock, tipWater);
		}
		{
			// Iteration 18: roughness of the sand / grass edge, per 65 536 cells.
			const int64_t w = int64_t(side);
			const auto at = [&](int64_t x, int64_t y) { return x < 0 || y < 0 || x >= w || y >= w ? SurfaceKind::Preserve : grid->cells[size_t(y) * side + size_t(x)]; };
			const auto sandLike = [](SurfaceKind k) { return k == SurfaceKind::Shore || k == SurfaceKind::Lake; };
			const auto landLike = [](SurfaceKind k) { return k == SurfaceKind::Grass || k == SurfaceKind::Dirt; };
			uint64_t spurs = 0, notches = 0, checker = 0, boundary = 0, sand = 0, spit = 0, corners = 0;
			const auto waterLike = [](SurfaceKind k) { return k == SurfaceKind::Lake || k == SurfaceKind::River; };
			for (int64_t y = 0; y < w; ++y) for (int64_t x = 0; x < w; ++x) {
				const SurfaceKind own = at(x, y);
				int around = 0;
				for (int64_t dy = -1; dy <= 1; ++dy) for (int64_t dx = -1; dx <= 1; ++dx) if ((dx || dy) && sandLike(at(x + dx, y + dy))) ++around;
				if (own == SurfaceKind::Shore) {
					++sand;
					spurs += around <= 2;
					for (int axis = 0; axis < 2; ++axis) {
						const int64_t ax = axis == 0 ? 1 : 0, ay = axis == 0 ? 0 : 1;
						SurfaceKind ends[2] = {SurfaceKind::Preserve, SurfaceKind::Preserve};
						int length = 1;
						bool ok = true;
						for (int sd = 0; sd < 2 && ok; ++sd) for (int64_t k = 1;; ++k) {
							const SurfaceKind kind = at(x + (sd ? -ax : ax) * k, y + (sd ? -ay : ay) * k);
							if (kind == SurfaceKind::Shore) { if (++length > 2) { ok = false; break; } continue; }
							ends[sd] = kind;
							break;
						}
						if (ok && length <= 2 && waterLike(ends[0]) && waterLike(ends[1])) { ++spit; break; }
					}
					boundary += landLike(at(x - 1, y)) || landLike(at(x + 1, y)) || landLike(at(x, y - 1)) || landLike(at(x, y + 1));
				} else if (landLike(own)) {
					notches += around >= 6;
				}
				const SurfaceKind a = at(x, y), b = at(x + 1, y), c = at(x, y + 1), d = at(x + 1, y + 1);
				{
					const SurfaceKind quad[4] = {a, b, c, d};
					int sandCells = 0, landCells = 0;
					for (const SurfaceKind kind : quad) { sandCells += kind == SurfaceKind::Shore; landCells += landLike(kind); }
					corners += sandCells + landCells == 4 && (sandCells == 1 || sandCells == 3);
				}
				checker += (a == SurfaceKind::Shore && d == SurfaceKind::Shore && landLike(b) && landLike(c)) || (b == SurfaceKind::Shore && c == SurfaceKind::Shore && landLike(a) && landLike(d));
			}
			const auto per = [&](uint64_t v) { return v * 65536 / grid->cells.size(); };
			std::cout << std::format("shore seed {}: spurs {} notches {} checker {} boundary {} sand {} spit {} corners {}\n", seed, per(spurs), per(notches), per(checker), per(boundary), per(sand), per(spit), per(corners));
		}
		std::ofstream file(outDir / std::format("{}.png", seed), std::ios::binary);
		file.write(reinterpret_cast<const char*>(png->data()), static_cast<std::streamsize>(png->size()));
	}
	return 0;
}
