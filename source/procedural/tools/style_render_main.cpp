// SPDX-License-Identifier: GPL-3.0-or-later
// style_render: draws the generator's surface for one style profile as a PNG, one pixel per tile, coloured by kind
// like the reference kind maps of the probe (kind-map-zNN.png). For a side-by-side look; nothing here is a measurement.
//
//   style_render PROFILE OUT_DIR SIDE [featureSize=N|transitionWidth=N|shoreWidth=N]... SEED [SEED...]    writes OUT_DIR/<seed>.png

#include "procedural/core/png_writer.h"
#include "procedural/core/reachability.h"
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
	int firstSeed = 4;
	// Sweep helper: leading name=value arguments override featureSize, transitionWidth or shoreWidth.
	for (; firstSeed < argc && std::string_view(argv[firstSeed]).find('=') != std::string_view::npos; ++firstSeed) {
		const std::string_view arg = argv[firstSeed];
		const std::string_view name = arg.substr(0, arg.find('='));
		const uint32_t value = static_cast<uint32_t>(std::strtoul(argv[firstSeed] + name.size() + 1, nullptr, 10));
		if (name == "caveExitShapes") caveExitShapes = arg.substr(name.size() + 1) == "ramp";
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
		else if (name == "ruinPercent") tuning.ruinPercent = value;
		else if (name == "campCount") tuning.campCount = value;
        else if (name == "plateauMinTop") {
            tuning.plateauMinTop = value;
        } else if (name == "caveEntrances")
            tuning.caveEntrances = value;
        else if (name == "buildingDensity") tuning.buildingDensity = value;
		else { std::cerr << "unknown override " << name << '\n'; return 2; }
	}
	for (int i = firstSeed; i < argc; ++i) {
		const uint64_t seed = std::strtoull(argv[i], nullptr, 10);
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
				for (const auto e : grid->entrances) ruleOk += caveEntranceAllowed(grid->cells, side, side, e, tuning.caveEntranceStyle);
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
            auto p = generatePlateau(*grid, seed, tuning.plateauMinTop, ramp, occupied);
            if (!p) {
                return 8;
            }
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
                    auto k = grid->cells[i];
                    if (occupied[i]) return false;
                    for (const auto* forbidden : {&grid->doors, &grid->gaps, &grid->fires, &grid->entrances})
                        if (std::ranges::find(*forbidden, i) != forbidden->end()) return false;
                    return k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Path;
                };
                for (const auto& other : p->ramps) if (&other != &r)
                    if (std::max(std::abs(int(other.cell % side) - int(r.cell % side)),
                                 std::abs(int(other.cell / side) - int(r.cell / side))) < 16) return 9;
                auto tail = size_t(int64_t(r.cell) + s.tailDx + int64_t(s.tailDy) * side);
                ruleOk += open(r.cell) && (!s.hasTail || open(tail)) && dest == r.destination
                    && (p->cells[dest] == PlateauKind::Top || (p->landings[dest] &&
                        p->cells[size_t(int64_t(dest) + dx[d] + int64_t(dy[d]) * side)] == PlateauKind::Top));
            }
            for (size_t i = 0; i < q.size(); ++i) {
                int x = int(q[i] % side), y = int(q[i] / side);
                for (size_t d = 0; d < 4; ++d) {
                    int xx = x + dx[d], yy = y + dy[d];
                    if (xx < 0 || yy < 0 || xx >= int(side) || yy >= int(side)) {
                        continue;
                    }
                    auto j = size_t(yy) * side + xx;
                    if (!reached[j] && (p->cells[j] == PlateauKind::Top || p->landings[j])) {
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
                "plateau seed {}: plateaus {} top {} ramps {} reached {} ruleOk {}\n", seed, p->components, top, p->ramps.size(),
                accessible, ruleOk
            );
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
