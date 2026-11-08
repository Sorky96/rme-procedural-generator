// SPDX-License-Identifier: GPL-3.0-or-later
// style_render: draws the generator's surface for one style profile as a PNG, one pixel per tile, coloured by kind
// like the reference kind maps of the probe (kind-map-zNN.png). For a side-by-side look; nothing here is a measurement.
//
//   style_render PROFILE OUT_DIR SIDE [featureSize=N|transitionWidth=N|shoreWidth=N]... SEED [SEED...]    writes OUT_DIR/<seed>.png

#include "procedural/core/png_writer.h"
#include "procedural/core/scale_stats.h"
#include "procedural/core/style_profile.h"
#include "procedural/core/surface.h"
#include "procedural/core/water_shape.h"

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
	constexpr PngColor colors[] = {{0, 0, 0}, {70, 150, 40}, {120, 80, 40}, {140, 140, 140}, {235, 215, 150}, {40, 90, 200}, {40, 90, 200}, {200, 170, 120}};
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
			std::cout << std::format("patch seed {}: D {} F {} P {} RB {} RF {} RS {} shares {} {} {} {} {}\n", seed, opt(ps.boundary), opt(ps.compact), opt(ps.beach), opt(ps.rockBoundary), opt(ps.rockCompact), opt(ps.rockDustShare), ps.sharePermille[0], ps.sharePermille[1], ps.sharePermille[2], ps.sharePermille[3], ps.sharePermille[4]);
		}
		if (const auto paths = pathShapesOf(*grid)) {
			const ChannelSummary p = summarizeChannels(*paths);
			std::cout << std::format("paths seed {}: count {} L {} W {} B {} cells {}\n", seed, p.count, p.lengthPer64k, p.widthMilli, p.branchesPer100Milli, paths->maskCells);
		}
		std::ofstream file(outDir / std::format("{}.png", seed), std::ios::binary);
		file.write(reinterpret_cast<const char*>(png->data()), static_cast<std::streamsize>(png->size()));
	}
	return 0;
}
