// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/level_repair.h"

#include <algorithm>
#include <cstdlib>
#include <new>
#include <queue>
#include <tuple>

namespace rme::procedural {
namespace {
constexpr int dx[] = {0, -1, 1, 0}, dy[] = {-1, 0, 0, 1};
template<class Visit> void around(uint32_t p, uint32_t w, uint32_t h, Visit visit) {
    const int x = int(p % w), y = int(p / w);
    for (uint32_t d = 0; d < 4; ++d) {
        const int xx = x + dx[d], yy = y + dy[d];
        if (xx >= 0 && yy >= 0 && xx < int(w) && yy < int(h)) visit(uint32_t(yy) * w + uint32_t(xx), d);
    }
}
bool land(SurfaceKind k) {
    return k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Shore || k == SurfaceKind::Path;
}

// Surface-only BFS: the same four-neighbour floor used by levelReachabilityOf.
uint32_t distance(std::span<const uint8_t> open, uint32_t w, uint32_t h, uint32_t a, uint32_t b) {
    if (!open[a] || !open[b]) return UINT32_MAX;
    std::vector<uint32_t> dist(open.size(), UINT32_MAX), queue{a}; dist[a] = 0;
    for (size_t head = 0; head < queue.size(); ++head) {
        const auto i = queue[head];
        if (i == b) return dist[i];
        around(i, w, h, [&](uint32_t j, uint32_t) {
            if (open[j] && dist[j] == UINT32_MAX) { dist[j] = dist[i] + 1; queue.push_back(j); }
        });
    }
    return UINT32_MAX;
}
}

std::expected<void, ReachabilityError> placeMountainPasses(
    LevelWorld& world, uint64_t seed, const SurfaceTuning& tuning,
    std::span<const ExitShape> shapes, std::stop_token stop) try {
    if (!tuning.passes) return {};
    if (stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
    auto& surface = world.surface;
    if (!surface.valid() || surface.cells.size() > UINT32_MAX / 12 || tuning.passes > 8 || world.decor.empty() ||
        !world.decor[0].valid() || world.decor[0].region != surface.region)
        return std::unexpected(ReachabilityError::InvalidGrid);
    const auto n = surface.cells.size(); const auto w = surface.region.width, h = surface.region.height;
    auto baseline = levelPlanOf(surface, true, false, nullptr, {}, {}, world.caps, {}, stop);
    if (!baseline) return std::unexpected(baseline.error());
    std::vector<uint32_t> labels(n), areas(1), queue;
    std::vector<uint8_t> heights(n), forbidden(n), cut(n), clearance(n);
    for (const auto& cap : world.caps) for (size_t i = 0; i < n; ++i) heights[i] += cap.cells[i] == PlateauKind::Top;
    for (const auto* poi : {&surface.doors, &surface.gaps, &surface.sites, &surface.fires})
        for (auto i : *poi) forbidden[i] = 1;
    for (uint32_t i = 0; i < n; ++i) if (surface.cells[i] == SurfaceKind::Rock && !labels[i]) {
        labels[i] = uint32_t(areas.size()); queue.assign(1, i);
        for (size_t q = 0; q < queue.size(); ++q) around(queue[q], w, h, [&](uint32_t j, uint32_t) {
            if (!labels[j] && surface.cells[j] == SurfaceKind::Rock) { labels[j] = labels[i]; queue.push_back(j); }
        });
        areas.push_back(uint32_t(queue.size()));
    }
    // Eligible axis cells admit a full three-wide stamp. Avoid POI, water and
    // protected seams, including at bends. Entrances are reselected after carving.
    std::vector<uint8_t> eligible(n);
    for (uint32_t y = 1; y + 1 < h; ++y) for (uint32_t x = 1; x + 1 < w; ++x) {
        bool ok = true;
        for (int yy = int(y)-1; yy <= int(y)+1; ++yy) for (int xx = int(x)-1; xx <= int(x)+1; ++xx) {
            const auto j = uint32_t(yy) * w + uint32_t(xx);
            ok &= !forbidden[j] && (land(surface.cells[j]) || surface.cells[j] == SurfaceKind::Rock);
        }
        eligible[y*w+x] = ok;
    }
    struct Candidate { uint32_t a, b, length, massif; };
    std::vector<Candidate> candidates;
    // Opposite ends of contiguous transects through each >=400-cell massif.
    // Transects are sampled every three cells in both axes; shortest ones
    // precede row-major ties. Sampling does not constrain the Dijkstra domain.
    for (uint32_t axis = 0; axis < 2; ++axis) {
        const auto rows = axis ? w : h, columns = axis ? h : w;
        const auto index = [=](uint32_t row, uint32_t col) { return axis ? col*w+row : row*w+col; };
        for (uint32_t row = 2; row + 2 < rows; row += 3) for (uint32_t col = 2; col + 2 < columns;) {
            const auto first = col, label = labels[index(row,col)];
            if (!label || areas[label] < 400) { ++col; continue; }
            while (col < columns && labels[index(row,col)] == label) ++col;
            if (col + 2 >= columns || col - first < 6) continue;
            const auto a = index(row,first-2), b = index(row,col+1);
            if (land(surface.cells[a]) && land(surface.cells[b]) && eligible[a] && eligible[b])
                candidates.push_back({a,b,col-first+3,label});
        }
    }
    std::ranges::sort(candidates, [](const auto& a, const auto& b) { return std::tie(a.length,a.a,a.b) < std::tie(b.length,b.a,b.b); });
    auto open = baseline->floors[0].walkable;
    for (size_t i = 0; i < n; ++i) if (world.decor[0].cells[i] != DecorKind::None) open[i] = 0;
    for (const auto& c : candidates) {
        if (stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
        if (world.passes.size() >= tuning.passes) break;
        // Do not make parallel duplicate passes in a single saddle.
        bool close = false;
        for (const auto& p : world.passes) for (const auto i : p.cells)
            close |= std::max(std::abs(int(i%w)-int(c.a%w)),std::abs(int(i/w)-int(c.a/w))) < 16;
        if (close) continue;
        if (!open[c.a] || !open[c.b]) continue;
        auto before = open;
        const auto bypass = distance(before,w,h,c.a,c.b);
        if (bypass == UINT32_MAX || uint64_t(c.length)*2 > bypass) continue;
        // Compare two actual BFS routes, not a Manhattan lower bound: one
        // around the closed mountain and one through the eligible massif.
        std::vector<uint8_t> throughOpen(n);
        for (uint32_t j = 0; j < n; ++j) {
            const auto near = [&](uint32_t endpoint) {
                return std::max(std::abs(int(j%w)-int(endpoint%w)),std::abs(int(j/w)-int(endpoint/w))) <= 2;
            };
            throughOpen[j] = eligible[j] && !clearance[j] && (labels[j] == c.massif || near(c.a) || near(c.b));
        }
        const auto crossing = distance(throughOpen,w,h,c.a,c.b);
        if (crossing == UINT32_MAX || uint64_t(crossing)*2 > bypass) continue;
        // Dijkstra state = cell, direction, straight run (1..3). A turn is
        // allowed only after three steps; reversal is forbidden. Thus no 1x1
        // staircase can win, even on equal-height terrain. Ties: (y,x), state.
        const auto states = n*12;
        std::vector<uint64_t> cost(states,UINT64_MAX);
        std::vector<uint32_t> previous(states,UINT32_MAX);
        using Entry = std::tuple<uint64_t,uint32_t,uint32_t>;
        std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> heap;
        for (uint32_t d=0; d<4; ++d) { const auto s=c.a*12+d*3+2; cost[s]=0; heap.emplace(0,c.a,s); }
        uint32_t finish=UINT32_MAX;
        size_t visits=0;
        while (!heap.empty()) {
            if (++visits%4096==0 && stop.stop_requested()) return std::unexpected(ReachabilityError::Cancelled);
            const auto [value,i,s]=heap.top(); heap.pop();
            if (cost[s]!=value) continue;
            if (i==c.b) { finish=s; break; }
            const auto direction=(s%12)/3, run=s%3+1;
            around(i,w,h,[&](uint32_t j,uint32_t d) {
                if (!throughOpen[j] || d==3-direction || (d!=direction && run<3)) return;
                // Search the entire massif, with short open approaches at its
                // opposite banks. An outside detour is not a mountain pass.
                const auto nr=d==direction ? std::min(3u,run+1) : 1u;
                const auto ns=j*12+d*3+nr-1;
                const auto nc=value+1+4*heights[j]+(d==direction ? 0u : 6u);
                if (nc<cost[ns]) { cost[ns]=nc; previous[ns]=s; heap.emplace(nc,j,ns); }
            });
        }
        if (finish==UINT32_MAX) continue;
        std::vector<uint32_t> path;
        for (auto s=finish; s!=UINT32_MAX; s=previous[s]) path.push_back(s/12);
        if (uint64_t(path.size()-1)*5 > uint64_t(bypass)*3) continue;
        MountainPass pass{c.a,c.b,{},std::move(before)};
        std::vector<uint8_t> band(n);
        bool mountain=false;
        for (auto i:path) {
            mountain |= heights[i]>0;
            const int x=int(i%w),y=int(i/w);
            for (int yy=y-1; yy<=y+1; ++yy) for (int xx=x-1; xx<=x+1; ++xx) band[size_t(yy)*w+xx]=1;
        }
        if (!mountain) continue;
        for (uint32_t i=0; i<n; ++i) if (band[i]) {
            pass.cells.push_back(i); cut[i]=1; surface.cells[i]=SurfaceKind::Path;
            world.decor[0].cells[i]=DecorKind::None; open[i]=1;
            if (!surface.bridgeAxes.empty()) surface.bridgeAxes[i]=BridgeAxis::None;
            const int x=int(i%w),y=int(i/w);
            for (int yy=std::max(0,y-1); yy<=std::min(int(h)-1,y+1); ++yy)
                for (int xx=std::max(0,x-1); xx<=std::min(int(w)-1,x+1); ++xx) clearance[size_t(yy)*w+xx]=1;
        }
        world.passes.push_back(std::move(pass));
        // Rebuild after each cut: the next bypass BFS must see the actual new
        // caps, clearance and ramp openings, rather than an obsolete footprint.
        std::erase_if(surface.entrances,[&](auto i){return cut[i]!=0;});
        auto entrances=reselectCaveEntrances(surface,{surface.region,seed,tuning},stop);
        if (!entrances) return std::unexpected(stop.stop_requested()?ReachabilityError::Cancelled:ReachabilityError::InvalidGrid);
        auto base=surface;
        for (size_t i=0;i<n;++i) if (clearance[i] && base.cells[i]==SurfaceKind::Rock) base.cells[i]=SurfaceKind::Dirt;
        world.caps.clear();
        if (tuning.plateauMinTop) {
            // Occupied prevents a newly selected ramp from occupying the valley.
            auto occupied=clearance;
            if (tuning.rockBlocks)
                if (auto outside=occupyOutsideStartLand(surface,occupied,stop); !outside) return std::unexpected(outside.error());
            auto cap=generatePlateau(base,seed,tuning.plateauMinTop,shapes,occupied,stop,tuning.plateauMaxPercent,true);
            if (!cap) return std::unexpected(stop.stop_requested()?ReachabilityError::Cancelled:ReachabilityError::InvalidGrid);
            world.caps.push_back(std::move(*cap));
            for (uint32_t k=2;k<=tuning.plateauLevels;++k) {
                auto next=generateNextPlateau(world.caps.back(),seed^(uint64_t(k)<<48),shapes,stop,tuning.plateauMaxPercent);
                if (!next) return std::unexpected(stop.stop_requested()?ReachabilityError::Cancelled:ReachabilityError::InvalidGrid);
                if (!next->components) break;
                world.caps.push_back(std::move(*next));
            }
        }
        baseline=levelPlanOf(surface,true,false,nullptr,{},{},world.caps,{},stop);
        if (!baseline) return std::unexpected(baseline.error());
        open=baseline->floors[0].walkable;
        std::fill(heights.begin(),heights.end(),0);
        for (const auto& cap:world.caps) for (size_t i=0;i<n;++i) heights[i]+=cap.cells[i]==PlateauKind::Top;
        for (size_t i=0;i<n;++i) if (world.decor[0].cells[i]!=DecorKind::None) open[i]=0;
    }
    return {};
} catch (const std::bad_alloc&) { return std::unexpected(ReachabilityError::AllocationFailed); }
} // namespace rme::procedural
