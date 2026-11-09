// SPDX-License-Identifier: GPL-3.0-or-later
// style_render: draws the generator's surface for one style profile as a PNG, one pixel per tile, coloured by kind
// like the reference kind maps of the probe (kind-map-zNN.png). For a side-by-side look; nothing here is a measurement.
//
//   style_render PROFILE OUT_DIR SIDE [featureSize=N|transitionWidth=N|shoreWidth=N]... SEED [SEED...]    writes OUT_DIR/<seed>.png

#include "procedural/core/png_writer.h"
#include "procedural/core/reachability.h"
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
	const SurfaceBudget budget {cells, cells * 8, cells * 64 + (1u << 20)};
	SurfaceTuning tuning = *profile->tuning;
	int firstSeed = 4;
	// Sweep helper: leading name=value arguments override featureSize, transitionWidth or shoreWidth.
	for (; firstSeed < argc && std::string_view(argv[firstSeed]).find('=') != std::string_view::npos; ++firstSeed) {
		const std::string_view arg = argv[firstSeed];
		const std::string_view name = arg.substr(0, arg.find('='));
		const uint32_t value = static_cast<uint32_t>(std::strtoul(argv[firstSeed] + name.size() + 1, nullptr, 10));
		if (name == "featureSize") tuning.featureSize = value;
		else if (name == "transitionWidth") tuning.transitionWidth = value;
		else if (name == "shoreWidth") tuning.shoreWidth = value;
		else if (name == "pathLength") tuning.pathLength = value;
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
		if (const auto reach = reachabilityOf(*grid, true); reach && reach->walkable) {
			std::cout << std::format("reach seed {}: walkable {} components {} largest {} largestPermille {} cutOff {} cutOffPer64k {} pockets {} buildings {} sealed {} doorBlocked {}\n", seed,
				reach->walkable, reach->components, reach->largest, reach->largest * 1000 / reach->walkable, reach->cutOff, reach->cutOff * 65536 / reach->cells, reach->pockets,
				reach->buildings, reach->sealed, doorBlocked);
		}
		std::ofstream file(outDir / std::format("{}.png", seed), std::ios::binary);
		file.write(reinterpret_cast<const char*>(png->data()), static_cast<std::streamsize>(png->size()));
	}
	return 0;
}
