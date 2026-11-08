// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/water_shape.h"

#include <algorithm>
#include <charconv>
#include <new>
#include <unordered_map>

namespace rme::procedural {
namespace {

// Zhang-Suen thinning of a bitmap with a one-cell empty border. Returns the number of skeleton cells.
size_t thin(std::vector<uint8_t>& bitmap, size_t width, size_t height) {
	std::vector<size_t> remove;
	bool changed = true;
	while (changed) {
		changed = false;
		for (int step = 0; step < 2; ++step) {
			remove.clear();
			for (size_t y = 1; y + 1 < height; ++y) {
				for (size_t x = 1; x + 1 < width; ++x) {
					const size_t i = y * width + x;
					if (!bitmap[i]) continue;
					const uint8_t p2 = bitmap[i - width], p3 = bitmap[i - width + 1], p4 = bitmap[i + 1], p5 = bitmap[i + width + 1],
						p6 = bitmap[i + width], p7 = bitmap[i + width - 1], p8 = bitmap[i - 1], p9 = bitmap[i - width - 1];
					const int neighbours = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
					if (neighbours < 2 || neighbours > 6) continue;
					const uint8_t ring[9] = {p2, p3, p4, p5, p6, p7, p8, p9, p2};
					int transitions = 0;
					for (int k = 0; k < 8; ++k) transitions += ring[k] == 0 && ring[k + 1] == 1;
					if (transitions != 1) continue;
					const bool first = step == 0 ? (p2 * p4 * p6 == 0 && p4 * p6 * p8 == 0) : (p2 * p4 * p8 == 0 && p2 * p6 * p8 == 0);
					if (first) remove.push_back(i);
				}
			}
			for (const size_t i : remove) bitmap[i] = 0;
			changed = changed || !remove.empty();
		}
	}
	return static_cast<size_t>(std::count(bitmap.begin(), bitmap.end(), uint8_t {1}));
}

// Skeleton cells that lie in a run of at least straightRunLength in one row or in one column.
size_t straightCells(const std::vector<uint8_t>& bitmap, size_t width, size_t height) {
	std::vector<uint8_t> mark(bitmap.size(), 0);
	for (size_t y = 0; y < height; ++y) {
		size_t x = 0;
		while (x < width) {
			if (!bitmap[y * width + x]) { ++x; continue; }
			size_t end = x;
			while (end < width && bitmap[y * width + end]) ++end;
			if (end - x >= straightRunLength) for (size_t k = x; k < end; ++k) mark[y * width + k] = 1;
			x = end;
		}
	}
	for (size_t x = 0; x < width; ++x) {
		size_t y = 0;
		while (y < height) {
			if (!bitmap[y * width + x]) { ++y; continue; }
			size_t end = y;
			while (end < height && bitmap[end * width + x]) ++end;
			if (end - y >= straightRunLength) for (size_t k = y; k < end; ++k) mark[k * width + x] = 1;
			y = end;
		}
	}
	return static_cast<size_t>(std::count(mark.begin(), mark.end(), uint8_t {1}));
}

uint32_t medianOf(std::vector<uint32_t> v) {
	if (v.empty()) return 0;
	std::sort(v.begin(), v.end());
	const size_t n = v.size();
	return n % 2 ? v[n / 2] : static_cast<uint32_t>((uint64_t(v[n / 2 - 1]) + v[n / 2]) / 2);
}

} // namespace

std::expected<void, StatsError> addWaterShapes(
	std::span<const uint8_t> water, uint32_t width, uint32_t height, WaterShapes& out, std::stop_token stop) {
	if (uint64_t(width) * height != water.size()) return std::unexpected(StatsError::InvalidRange);
	try {
		std::vector<uint8_t> seen(water.size(), 0);
		std::vector<size_t> stack, cells;
		std::vector<uint8_t> bitmap;
		for (size_t start = 0; start < water.size(); ++start) {
			if (seen[start] || !water[start]) continue;
			if (stop.stop_requested()) return std::unexpected(StatsError::Cancelled);
			cells.clear();
			stack.assign(1, start);
			seen[start] = 1;
			size_t x0 = start % width, x1 = x0, y0 = start / width, y1 = y0;
			while (!stack.empty()) {
				const size_t i = stack.back();
				stack.pop_back();
				cells.push_back(i);
				const size_t x = i % width, y = i / width;
				x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
				auto visit = [&](size_t j) { if (!seen[j] && water[j]) { seen[j] = 1; stack.push_back(j); } };
				if (x > 0) visit(i - 1);
				if (x + 1 < width) visit(i + 1);
				if (y > 0) visit(i - width);
				if (y + 1 < height) visit(i + width);
			}
			const uint32_t area = static_cast<uint32_t>(std::min<size_t>(cells.size(), UINT32_MAX));
			if (area >= seaMinArea) { out.seaTiles += area; continue; }
			const size_t bw = x1 - x0 + 3, bh = y1 - y0 + 3; // one empty cell around the body
			bitmap.assign(bw * bh, 0);
			for (const size_t i : cells) bitmap[(i / width - y0 + 1) * bw + (i % width - x0 + 1)] = 1;
			const size_t skeleton = area >= riverMinArea ? thin(bitmap, bw, bh) : 0;
			if (area >= riverMinArea && skeleton >= riverMinSkeleton && uint64_t(area) * 1000 / skeleton <= riverMaxWidthMilli) {
				out.rivers.push_back({area, static_cast<uint32_t>(skeleton), static_cast<uint32_t>(std::max(x1 - x0 + 1, y1 - y0 + 1)),
					static_cast<uint32_t>(straightCells(bitmap, bw, bh))});
			} else {
				out.lakeAreas.push_back(area);
			}
		}
	} catch (const std::bad_alloc&) {
		return std::unexpected(StatsError::AllocationFailed);
	}
	return {};
}

std::expected<WaterShapes, StatsError> extractWaterShapes(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop) {
	if (zMin > zMax) return std::unexpected(StatsError::InvalidRange);
	try {
		std::unordered_map<std::string_view, StyleKind> byKey;
		for (const auto& e : keymap) byKey.emplace(e.key, e.kind);
		const auto& names = map.groundKeys();
		std::vector<uint8_t> isWater(names.size());
		for (size_t i = 0; i < names.size(); ++i) {
			const auto it = byKey.find(names[i]);
			if (it != byKey.end()) isWater[i] = it->second == StyleKind::Water;
			else if (i == noGroundKey) isWater[i] = 0;
			else return std::unexpected(StatsError::KeyNotMapped);
		}
		WaterShapes shapes;
		std::vector<uint8_t> mask;
		for (const SemanticFloor& floor : map.floors()) {
			if (floor.bounds.z < zMin || floor.bounds.z > zMax) continue;
			mask.assign(floor.ground.size(), 0);
			for (size_t i = 0; i < mask.size(); ++i) mask[i] = (floor.traits[i] & bit(Trait::Present)) && isWater[floor.ground[i]];
			if (auto added = addWaterShapes(mask, static_cast<uint32_t>(floor.bounds.width), static_cast<uint32_t>(floor.bounds.height), shapes, stop); !added) {
				return std::unexpected(added.error());
			}
		}
		return shapes;
	} catch (const std::bad_alloc&) {
		return std::unexpected(StatsError::AllocationFailed);
	}
}

std::expected<WaterShapes, StatsError> waterShapesOf(const SurfaceGrid& grid, std::stop_token stop) {
	try {
		std::vector<uint8_t> mask(grid.cells.size());
		for (size_t i = 0; i < mask.size(); ++i) mask[i] = grid.cells[i] == SurfaceKind::Lake || grid.cells[i] == SurfaceKind::River;
		WaterShapes shapes;
		if (auto added = addWaterShapes(mask, grid.region.width, grid.region.height, shapes, stop); !added) return std::unexpected(added.error());
		return shapes;
	} catch (const std::bad_alloc&) {
		return std::unexpected(StatsError::AllocationFailed);
	}
}

RiverSummary summarizeRivers(std::span<const RiverComponent> rivers) {
	RiverSummary s;
	s.count = static_cast<uint32_t>(rivers.size());
	if (rivers.empty()) return s;
	std::vector<uint32_t> tortuosity, straight, width;
	for (const RiverComponent& r : rivers) {
		tortuosity.push_back(static_cast<uint32_t>(uint64_t(r.skeleton) * 1000 / std::max<uint32_t>(r.side, 1)));
		straight.push_back(static_cast<uint32_t>(uint64_t(r.straight) * 1000 / std::max<uint32_t>(r.skeleton, 1)));
		width.push_back(static_cast<uint32_t>(uint64_t(r.area) * 1000 / std::max<uint32_t>(r.skeleton, 1)));
	}
	s.tortuosityPermille = medianOf(std::move(tortuosity));
	s.straightPermille = medianOf(std::move(straight));
	s.widthMilli = medianOf(std::move(width));
	return s;
}

std::string serializeWaterShapes(const WaterShapes& shapes) {
	std::string out(waterShapesSchema);
	out += '\n';
	out += "sea\t" + std::to_string(shapes.seaTiles) + '\n';
	for (const RiverComponent& r : shapes.rivers) {
		out += "river\t" + std::to_string(r.area) + '\t' + std::to_string(r.skeleton) + '\t' + std::to_string(r.side) + '\t' + std::to_string(r.straight) + '\n';
	}
	for (const uint32_t a : shapes.lakeAreas) out += "lake\t" + std::to_string(a) + '\n';
	return out;
}

std::expected<WaterShapes, ProfileError> parseWaterShapes(std::string_view text) {
	WaterShapes shapes;
	bool header = false, haveSea = false;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		if (line.empty()) continue;
		if (!header) {
			if (line == waterShapesSchema) { header = true; continue; }
			return std::unexpected(line.starts_with("g4-water/") ? ProfileError::UnsupportedSchema : ProfileError::BadFormat);
		}
		std::vector<uint64_t> v;
		std::string_view rest = line;
		const size_t first = rest.find('\t');
		if (first == std::string_view::npos) return std::unexpected(ProfileError::BadFormat);
		const std::string_view type = rest.substr(0, first);
		rest.remove_prefix(first + 1);
		while (true) {
			const size_t tab = rest.find('\t');
			const std::string_view field = rest.substr(0, tab);
			uint64_t value = 0;
			const auto r = std::from_chars(field.data(), field.data() + field.size(), value);
			if (field.empty() || r.ec != std::errc {} || r.ptr != field.data() + field.size()) return std::unexpected(ProfileError::BadFormat);
			v.push_back(value);
			if (tab == std::string_view::npos) break;
			rest.remove_prefix(tab + 1);
		}
		if (type == "sea" && v.size() == 1 && !haveSea) { shapes.seaTiles = v[0]; haveSea = true; }
		else if (type == "river" && v.size() == 4 && v[0] <= UINT32_MAX && v[1] <= UINT32_MAX && v[2] <= UINT32_MAX && v[3] <= UINT32_MAX) {
			shapes.rivers.push_back({static_cast<uint32_t>(v[0]), static_cast<uint32_t>(v[1]), static_cast<uint32_t>(v[2]), static_cast<uint32_t>(v[3])});
		}
		else if (type == "lake" && v.size() == 1 && v[0] <= UINT32_MAX) shapes.lakeAreas.push_back(static_cast<uint32_t>(v[0]));
		else return std::unexpected(ProfileError::BadFormat);
	}
	if (!header || !haveSea) return std::unexpected(ProfileError::BadFormat);
	return shapes;
}

} // namespace rme::procedural
