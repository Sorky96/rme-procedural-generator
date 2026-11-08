// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/decor_context.h"

#include "procedural/core/doodad_class.h"

#include <algorithm>
#include <map>
#include <charconv>
#include <new>
#include <unordered_map>

namespace rme::procedural {
namespace {

// Spreads marks to 4-neighbours `reach` times; absent cells are ordinary cells.
void growMarks(std::vector<uint8_t>& mark, std::vector<uint8_t>& scratch, size_t width, size_t height, uint32_t reach) {
	scratch.resize(mark.size());
	for (uint32_t step = 0; step < reach; ++step) {
		scratch = mark;
		for (size_t y = 0; y < height; ++y) {
			for (size_t x = 0; x < width; ++x) {
				const size_t i = y * width + x;
				if (mark[i]) continue;
				mark[i] = (x > 0 && scratch[i - 1]) || (x + 1 < width && scratch[i + 1]) ||
					(y > 0 && scratch[i - width]) || (y + 1 < height && scratch[i + width]);
			}
		}
	}
}

uint32_t rate(uint64_t count, uint64_t tiles) noexcept { return tiles ? static_cast<uint32_t>(count * 1000 / tiles) : 0; }

constexpr size_t kinds = 3;

uint64_t countOf(const DecorCell& c, size_t k) noexcept { return k == 0 ? c.trees : (k == 1 ? c.bushes : c.stones); }

DecorCell merged(const DecorContext& ctx, bool dirt, int water, int rock) noexcept {
	DecorCell total;
	for (int w = 0; w < 2; ++w) {
		for (int r = 0; r < 2; ++r) {
			if ((water >= 0 && w != water) || (rock >= 0 && r != rock)) continue;
			const DecorCell& c = ctx.cell[DecorContext::index(dirt, w != 0, r != 0)];
			total.tiles += c.tiles; total.trees += c.trees; total.bushes += c.bushes; total.stones += c.stones;
		}
	}
	return total;
}

} // namespace

std::string serializeDecorContext(const DecorContext& context) {
	std::string out(decorContextSchema);
	out += '\n';
	for (size_t i = 0; i < context.cell.size(); ++i) {
		const DecorCell& c = context.cell[i];
		out += "cell\t" + std::to_string(i) + '\t' + std::to_string(c.tiles) + '\t' + std::to_string(c.trees) + '\t' +
			std::to_string(c.bushes) + '\t' + std::to_string(c.stones) + '\n';
	}
	return out;
}

std::expected<DecorContext, ProfileError> parseDecorContext(std::string_view text) {
	DecorContext context;
	std::array<bool, 8> seen {};
	bool header = false;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		if (line.empty()) continue;
		if (!header) {
			if (line == decorContextSchema) { header = true; continue; }
			return std::unexpected(line.starts_with("g4-decorctx/") ? ProfileError::UnsupportedSchema : ProfileError::BadFormat);
		}
		std::array<uint64_t, 5> v {};
		std::string_view rest = line;
		if (!rest.starts_with("cell\t")) return std::unexpected(ProfileError::BadFormat);
		rest.remove_prefix(5);
		for (size_t i = 0; i < v.size(); ++i) {
			const size_t tab = rest.find('\t');
			const std::string_view field = rest.substr(0, tab);
			const auto r = std::from_chars(field.data(), field.data() + field.size(), v[i]);
			if (field.empty() || r.ec != std::errc {} || r.ptr != field.data() + field.size()) return std::unexpected(ProfileError::BadFormat);
			if ((tab == std::string_view::npos) != (i + 1 == v.size())) return std::unexpected(ProfileError::BadFormat);
			if (tab != std::string_view::npos) rest.remove_prefix(tab + 1);
		}
		if (v[0] >= 8 || seen[v[0]]) return std::unexpected(ProfileError::BadFormat);
		seen[v[0]] = true;
		context.cell[v[0]] = {v[1], v[2], v[3], v[4]};
	}
	if (!header || !std::all_of(seen.begin(), seen.end(), [](bool b) { return b; })) return std::unexpected(ProfileError::BadFormat);
	return context;
}

void add(DecorContext& total, const DecorContext& more) noexcept {
	for (size_t i = 0; i < total.cell.size(); ++i) {
		total.cell[i].tiles += more.cell[i].tiles;
		total.cell[i].trees += more.cell[i].trees;
		total.cell[i].bushes += more.cell[i].bushes;
		total.cell[i].stones += more.cell[i].stones;
	}
}

std::expected<DecorContext, StatsError> extractDecorContext(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop) {
	if (zMin > zMax) return std::unexpected(StatsError::InvalidRange);
	try {
		std::unordered_map<std::string_view, StyleKind> byKey;
		for (const auto& e : keymap) byKey.emplace(e.key, e.kind);
		const auto& names = map.groundKeys();
		std::vector<StyleKind> kindOf(names.size());
		for (size_t i = 0; i < names.size(); ++i) {
			const auto it = byKey.find(names[i]);
			if (it != byKey.end()) kindOf[i] = it->second;
			else if (i == noGroundKey) kindOf[i] = StyleKind::NoGround;
			else return std::unexpected(StatsError::KeyNotMapped);
		}
		DecorContext total;
		std::vector<uint8_t> water, rock, scratch;
		for (const SemanticFloor& floor : map.floors()) {
			if (floor.bounds.z < zMin || floor.bounds.z > zMax) continue;
			if (stop.stop_requested()) return std::unexpected(StatsError::Cancelled);
			const size_t w = static_cast<size_t>(floor.bounds.width), h = static_cast<size_t>(floor.bounds.height);
			water.assign(w * h, 0);
			rock.assign(w * h, 0);
			for (size_t i = 0; i < w * h; ++i) {
				if (!(floor.traits[i] & bit(Trait::Present))) continue;
				const StyleKind k = kindOf[floor.ground[i]];
				water[i] = k == StyleKind::Water || k == StyleKind::Shore;
				rock[i] = k == StyleKind::Rock;
			}
			growMarks(water, scratch, w, h, decorReach);
			growMarks(rock, scratch, w, h, decorReach);
			for (size_t y = 0; y < h; ++y) {
				if (stop.stop_requested()) return std::unexpected(StatsError::Cancelled);
				for (size_t x = 0; x < w; ++x) {
					const size_t i = y * w + x;
					if (!(floor.traits[i] & bit(Trait::Present))) continue;
					const StyleKind k = kindOf[floor.ground[i]];
					if (k != StyleKind::Grass && k != StyleKind::Dirt) continue;
					DecorCell& c = total.cell[DecorContext::index(k == StyleKind::Dirt, water[i] != 0, rock[i] != 0)];
					++c.tiles;
					c.trees += (floor.traits[i] & bit(Trait::TreeDoodad)) != 0;
					c.bushes += (floor.traits[i] & bit(Trait::BushDoodad)) != 0;
					c.stones += (floor.traits[i] & bit(Trait::StoneDoodad)) != 0;
				}
			}
		}
		return total;
	} catch (const std::bad_alloc&) {
		return std::unexpected(StatsError::AllocationFailed);
	}
}

DecorContext decorContextOf(const SurfaceGrid& grid, const DecorGrid& decor) {
	const size_t w = grid.region.width, h = grid.region.height;
	std::vector<uint8_t> water(grid.cells.size(), 0), rock(grid.cells.size(), 0), scratch;
	for (size_t i = 0; i < grid.cells.size(); ++i) {
		const SurfaceKind k = grid.cells[i];
		water[i] = k == SurfaceKind::Lake || k == SurfaceKind::Shore || k == SurfaceKind::River;
		rock[i] = k == SurfaceKind::Rock;
	}
	growMarks(water, scratch, w, h, decorReach);
	growMarks(rock, scratch, w, h, decorReach);
	DecorContext total;
	for (size_t i = 0; i < grid.cells.size(); ++i) {
		const SurfaceKind k = grid.cells[i];
		if (k != SurfaceKind::Grass && k != SurfaceKind::Dirt) continue;
		DecorCell& c = total.cell[DecorContext::index(k == SurfaceKind::Dirt, water[i] != 0, rock[i] != 0)];
		++c.tiles;
		c.trees += decor.cells[i] == DecorKind::Tree;
		c.bushes += decor.cells[i] == DecorKind::Bush;
		c.stones += decor.cells[i] == DecorKind::Stone;
	}
	return total;
}

std::expected<DecorContext, FitError> evaluateDecor(
	const SurfaceTuning& surface, const DecorTuning& decor, const EvalSpec& spec, std::stop_token stop) {
	if (spec.side < 8 || spec.side > 2048 || spec.seeds.empty()) return std::unexpected(FitError::InvalidSpec);
	const uint64_t cells = static_cast<uint64_t>(spec.side) * spec.side;
	const SurfaceBudget budget {cells, cells * 8, cells * 64 + (1u << 20)};
	DecorContext total;
	try {
		for (const uint64_t seed : spec.seeds) {
			if (stop.stop_requested()) return std::unexpected(FitError::Cancelled);
			SurfaceConfig config {{100, 100, 7, spec.side, spec.side}, seed, surface};
			const auto grid = generateSurface(config, MapExtent {65001, 65001, 16}, budget, {}, {}, stop);
			if (!grid) return std::unexpected(grid.error() == SurfaceError::Cancelled ? FitError::Cancelled : FitError::Generator);
			const auto placed = generateDecoration(*grid, seed, decor, stop);
			if (!placed) return std::unexpected(placed.error() == SurfaceError::Cancelled ? FitError::Cancelled : FitError::Generator);
			add(total, decorContextOf(*grid, *placed));
		}
	} catch (const std::bad_alloc&) {
		return std::unexpected(FitError::AllocationFailed);
	}
	return total;
}

DecorContext buildDecorTarget(std::span<const MapDecor> maps, uint64_t minTiles) {
	DecorContext target;
	constexpr uint64_t virtualTiles = 1'000'000;
	for (size_t cell = 0; cell < target.cell.size(); ++cell) {
		std::map<std::string, std::vector<const DecorCell*>> byFamily;
		for (const MapDecor& m : maps) {
			if (m.context.cell[cell].tiles >= minTiles) byFamily[m.family].push_back(&m.context.cell[cell]);
		}
		if (byFamily.empty()) continue;
		// Rates in ppm of 1e6 weights keep the arithmetic exact enough and integer.
		std::array<uint64_t, 3> sum {};
		uint64_t weightSum = 0;
		for (const auto& [family, members] : byFamily) {
			const uint64_t weight = 1'000'000 / (byFamily.size() * members.size());
			for (const DecorCell* c : members) {
				weightSum += weight;
				sum[0] += weight * (c->trees * virtualTiles / c->tiles);
				sum[1] += weight * (c->bushes * virtualTiles / c->tiles);
				sum[2] += weight * (c->stones * virtualTiles / c->tiles);
			}
		}
		target.cell[cell] = {virtualTiles, sum[0] / weightSum, sum[1] / weightSum, sum[2] / weightSum};
	}
	return target;
}

DecorDistance decorDistance(const DecorContext& reference, const DecorContext& generated, uint64_t minTiles) noexcept {
	DecorDistance d;
	for (size_t i = 0; i < reference.cell.size(); ++i) {
		const DecorCell& ref = reference.cell[i];
		if (ref.tiles < minTiles) continue;
		const DecorCell& got = generated.cell[i];
		for (size_t k = 0; k < kinds; ++k) {
			const uint32_t a = rate(countOf(ref, k), ref.tiles), b = rate(countOf(got, k), got.tiles);
			d.sumPermille += a > b ? a - b : b - a;
			++d.terms;
		}
	}
	return d;
}

std::expected<DecorTuning, FitError> fitDecorTuning(
	const DecorContext& target, const SurfaceTuning& surface, const EvalSpec& spec, std::stop_token stop) {
	constexpr uint64_t minTiles = 200;
	DecorTuning tuning;
	const DecorCell grassFar = merged(target, false, 0, -1), grassWater = merged(target, false, 1, -1);
	const DecorCell dirtNearRock = merged(target, true, -1, 1), dirtFarRock = merged(target, true, -1, 0);
	if (grassFar.tiles >= minTiles) {
		tuning.bushPermille = rate(grassFar.bushes, grassFar.tiles);
		const auto built = evaluateDecor(surface, DecorTuning {}, spec, stop);
		if (!built) return std::unexpected(built.error());
		const DecorCell builtFar = merged(*built, false, 0, -1);
		const uint32_t builtTree = rate(builtFar.trees, builtFar.tiles), wanted = rate(grassFar.trees, grassFar.tiles);
		if (builtTree > 0) tuning.treeScalePermille = std::min<uint32_t>(maxTreeScalePermille, static_cast<uint32_t>(static_cast<uint64_t>(wanted) * 1000 / builtTree));
	}
	if (grassWater.tiles >= minTiles) {
		tuning.waterBushPermille = rate(grassWater.bushes, grassWater.tiles);
		tuning.waterTreePermille = rate(grassWater.trees, grassWater.tiles);
	}
	if (dirtFarRock.tiles >= minTiles) tuning.dirtStonePermille = rate(dirtFarRock.stones, dirtFarRock.tiles);
	if (dirtNearRock.tiles >= minTiles) tuning.rockStonePermille = rate(dirtNearRock.stones, dirtNearRock.tiles);
	return tuning;
}

} // namespace rme::procedural
