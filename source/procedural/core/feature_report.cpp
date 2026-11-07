// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/feature_report.h"

#include <algorithm>
#include <format>

namespace rme::procedural {

namespace {

[[nodiscard]] uint64_t permille(uint64_t part, uint64_t whole) noexcept {
	return whole == 0 ? 0 : part * 1000 / whole;
}

[[nodiscard]] std::string percent(uint64_t part, uint64_t whole) {
	const uint64_t p = permille(part, whole);
	return std::format("{}.{}%", p / 10, p % 10);
}

[[nodiscard]] std::string jsonEscape(std::string_view text) {
	std::string out;
	for (const char c : text) {
		switch (c) {
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\t': out += "\\t"; break;
			default:
				if (static_cast<unsigned char>(c) < 0x20) out += std::format("\\u{:04x}", static_cast<unsigned>(c));
				else out += c;
		}
	}
	return out;
}

[[nodiscard]] std::string csvField(std::string_view text) {
	if (text.find_first_of(",\"\n") == std::string_view::npos) return std::string(text);
	std::string out = "\"";
	for (const char c : text) {
		if (c == '"') out += '"';
		out += c;
	}
	return out + '"';
}

[[nodiscard]] std::string mdCell(std::string_view text) {
	std::string out;
	for (const char c : text) {
		if (c == '|') out += "\\|";
		else out += c;
	}
	return out;
}

[[nodiscard]] const char* distanceLabel(size_t bin) noexcept {
	static constexpr const char* labels[distanceBinCount] = {"1", "2", "3-4", "5-8", "9-16", "17+ or none"};
	return labels[bin];
}

template <size_t N>
[[nodiscard]] std::string jsonArray(const std::array<uint64_t, N>& values) {
	std::string out = "[";
	for (size_t i = 0; i < N; ++i) out += std::format("{}{}", i ? ", " : "", values[i]);
	return out + "]";
}

// Ground keys sorted by tiles descending, then key index, for readable tables.
[[nodiscard]] std::vector<size_t> keysByTiles(const ReportInput& in) {
	std::vector<size_t> order(in.map->groundKeys().size());
	for (size_t i = 0; i < order.size(); ++i) order[i] = i;
	std::ranges::stable_sort(order, [&](size_t a, size_t b) {
		return in.semantic->groundKeyStats[a].tiles > in.semantic->groundKeyStats[b].tiles;
	});
	return order;
}

[[nodiscard]] std::string keyLabel(const ReportInput& in, size_t key) {
	return key == noGroundKey ? "(no brush)" : in.map->groundKeys()[key];
}

} // namespace

const char* csvTableName(CsvTable table) noexcept {
	switch (table) {
		case CsvTable::GroundKeys: return "ground_keys";
		case CsvTable::Floors: return "floors";
		case CsvTable::Adjacency: return "adjacency";
		case CsvTable::Scales: return "scales";
		case CsvTable::Distance: return "distance";
	}
	return "";
}

std::string renderJson(const ReportInput& in) {
	const auto& keys = in.map->groundKeys();
	std::string out = "{\n  \"schema\": \"g3-reference-analysis/1\",\n";
	out += std::format("  \"source\": {{\"sha256\": \"{}\", \"client\": \"{}\", \"compatibility\": \"{}\", \"otbm_version\": {}, \"width\": {}, \"height\": {}, \"items_major\": {}, \"items_minor\": {}, \"compressed\": {}, \"file_bytes\": {}}},\n",
		in.sha256, jsonEscape(in.clientId), in.compatibility, in.header.otbmVersion, in.header.width, in.header.height,
		in.header.itemsMajor, in.header.itemsMinor, in.header.compressed ? "true" : "false", in.header.fileBytes);
	out += std::format("  \"import\": {{\"tiles\": {}, \"items\": {}, \"unknown_ground\": {}, \"unknown_items\": {}, \"loader_warnings\": {}, \"unknown_ids\": [",
		in.tiles, in.items, in.unknownGround, in.unknownItems, in.loaderWarnings);
	for (size_t i = 0; i < in.unknownIds.size(); ++i) {
		out += std::format("{}{{\"id\": {}, \"count\": {}}}", i ? ", " : "", in.unknownIds[i].id, in.unknownIds[i].count);
	}
	out += "]},\n";
	out += std::format("  \"options\": {{\"impassable_percent\": {}, \"small_component_cells\": {}}},\n",
		in.topologyOptions.impassablePercent, in.topologyOptions.smallComponentCells);

	out += "  \"traits\": {";
	for (size_t t = 0; t < traitCount; ++t) {
		out += std::format("{}\"{}\": {}", t ? ", " : "", traitName(t), in.semantic->tilesPerTrait[t]);
	}
	out += "},\n  \"ground_keys\": [\n";
	for (size_t k = 0; k < keys.size(); ++k) {
		const auto& stat = in.semantic->groundKeyStats[k];
		const auto& region = in.topology->keys[k];
		out += std::format("    {{\"index\": {}, \"key\": \"{}\", \"tiles\": {}, \"blocking_tiles\": {}, \"impassable\": {}, \"regions\": {}, \"largest_region\": {}, \"enclosed_regions\": {}}}{}\n",
			k, jsonEscape(keys[k]), stat.tiles, stat.blockingTiles, region.impassable ? "true" : "false", region.regions,
			region.largest, region.enclosed, k + 1 < keys.size() ? "," : "");
	}
	out += "  ],\n  \"floors\": [\n";
	for (size_t i = 0; i < in.topology->floors.size(); ++i) {
		const auto& f = in.topology->floors[i];
		const auto& feature = in.features->floors[i];
		const auto& w = f.walkable;
		out += std::format("    {{\"z\": {}, \"width\": {}, \"height\": {}, \"tiles\": {}, \"walkable_cells\": {}, \"unknown_cells\": {}, \"floor_change_cells\": {}, \"teleport_cells\": {}, \"walkable_components\": {}, \"largest_component\": {}, \"small_components\": {}, \"small_cells\": {}, \"top_components\": {}, \"component_size_buckets\": [",
			f.bounds.z, f.bounds.width, f.bounds.height, f.tiles, f.walkableCells, f.unknownCells, f.floorChangeCells,
			f.teleportCells, w.count, w.largest, w.smallCount, w.smallCells, jsonArray(w.top));
		for (size_t b = 0; b < w.buckets.size(); ++b) out += std::format("{}{}", b ? ", " : "", w.buckets[b]);
		out += "], \"trait_tiles\": {";
		for (size_t t = 0; t < traitCount; ++t) out += std::format("{}\"{}\": {}", t ? ", " : "", traitName(t), feature.traitTiles[t]);
		out += std::format("}}}}{}\n", i + 1 < in.topology->floors.size() ? "," : "");
	}
	out += "  ],\n  \"scales\": [\n";
	for (size_t s = 0; s < in.features->scales.size(); ++s) {
		const auto& sc = in.features->scales[s];
		out += std::format("    {{\"scale\": {}, \"blocks\": {}, \"occupancy_deciles\": {}, \"walkable_share_deciles\": {}, \"impassable_share_deciles\": {}}}{}\n",
			sc.scale, sc.blocks, jsonArray(sc.occupancy), jsonArray(sc.walkableShare), jsonArray(sc.impassableShare),
			s + 1 < in.features->scales.size() ? "," : "");
	}
	const auto& d = in.features->distance;
	out += std::format("  ],\n  \"distance_to_impassable_ground\": {{\"bins\": [\"1\", \"2\", \"3-4\", \"5-8\", \"9-16\", \"17+ or none\"], \"tiles\": {}, \"doodad\": {}, \"wall\": {}, \"border\": {}}},\n",
		jsonArray(d.tiles), jsonArray(d.doodad), jsonArray(d.wall), jsonArray(d.border));
	out += "  \"adjacency\": {\"note\": \"[a, b, east, south]: ground key a with b to its east / south; west and north are transposes\", \"pairs\": [";
	const auto& adj = in.features->adjacency;
	bool first = true;
	for (size_t a = 0; a < adj.keyCount; ++a) {
		for (size_t b = 0; b < adj.keyCount; ++b) {
			const uint64_t e = adj.east[a * adj.keyCount + b], s = adj.south[a * adj.keyCount + b];
			if (e == 0 && s == 0) continue;
			out += std::format("{}\n    [{}, {}, {}, {}]", first ? "" : ",", a, b, e, s);
			first = false;
		}
	}
	out += "\n  ]}\n}\n";
	return out;
}

std::string renderCsv(const ReportInput& in, CsvTable table) {
	std::string out;
	const auto& keys = in.map->groundKeys();
	switch (table) {
		case CsvTable::GroundKeys:
			out = "index,key,tiles,blocking_tiles,impassable,regions,largest_region,enclosed_regions\n";
			for (size_t k = 0; k < keys.size(); ++k) {
				const auto& stat = in.semantic->groundKeyStats[k];
				const auto& r = in.topology->keys[k];
				out += std::format("{},{},{},{},{},{},{},{}\n", k, csvField(keys[k]), stat.tiles, stat.blockingTiles,
					r.impassable ? 1 : 0, r.regions, r.largest, r.enclosed);
			}
			break;
		case CsvTable::Floors:
			out = "z,width,height,tiles,walkable_cells,unknown_cells,floor_change_cells,teleport_cells,walkable_components,largest_component,small_components,small_cells";
			for (size_t t = 0; t < traitCount; ++t) out += std::format(",trait_{}", traitName(t));
			out += "\n";
			for (size_t i = 0; i < in.topology->floors.size(); ++i) {
				const auto& f = in.topology->floors[i];
				out += std::format("{},{},{},{},{},{},{},{},{},{},{},{}", f.bounds.z, f.bounds.width, f.bounds.height, f.tiles,
					f.walkableCells, f.unknownCells, f.floorChangeCells, f.teleportCells, f.walkable.count, f.walkable.largest,
					f.walkable.smallCount, f.walkable.smallCells);
				for (size_t t = 0; t < traitCount; ++t) out += std::format(",{}", in.features->floors[i].traitTiles[t]);
				out += "\n";
			}
			break;
		case CsvTable::Adjacency: {
			const auto& adj = in.features->adjacency;
			std::vector<uint64_t> eastRow(adj.keyCount, 0), southRow(adj.keyCount, 0);
			for (size_t a = 0; a < adj.keyCount; ++a) {
				for (size_t b = 0; b < adj.keyCount; ++b) {
					eastRow[a] += adj.east[a * adj.keyCount + b];
					southRow[a] += adj.south[a * adj.keyCount + b];
				}
			}
			out = "key_a,key_b,east,south,east_permille_of_a,south_permille_of_a\n";
			for (size_t a = 0; a < adj.keyCount; ++a) {
				for (size_t b = 0; b < adj.keyCount; ++b) {
					const uint64_t e = adj.east[a * adj.keyCount + b], s = adj.south[a * adj.keyCount + b];
					if (e == 0 && s == 0) continue;
					out += std::format("{},{},{},{},{},{}\n", csvField(keyLabel(in, a)), csvField(keyLabel(in, b)), e, s,
						permille(e, eastRow[a]), permille(s, southRow[a]));
				}
			}
			break;
		}
		case CsvTable::Scales:
			out = "scale,histogram,bin,blocks\n";
			for (const auto& sc : in.features->scales) {
				for (size_t b = 0; b < fractionBins; ++b) out += std::format("{},occupancy,{},{}\n", sc.scale, b, sc.occupancy[b]);
				for (size_t b = 0; b < fractionBins; ++b) out += std::format("{},walkable_share,{},{}\n", sc.scale, b, sc.walkableShare[b]);
				for (size_t b = 0; b < fractionBins; ++b) out += std::format("{},impassable_share,{},{}\n", sc.scale, b, sc.impassableShare[b]);
			}
			break;
		case CsvTable::Distance: {
			const auto& d = in.features->distance;
			out = "distance,tiles,doodad,wall,border,doodad_permille,wall_permille,border_permille\n";
			for (size_t b = 0; b < distanceBinCount; ++b) {
				out += std::format("{},{},{},{},{},{},{},{}\n", csvField(distanceLabel(b)), d.tiles[b], d.doodad[b], d.wall[b], d.border[b],
					permille(d.doodad[b], d.tiles[b]), permille(d.wall[b], d.tiles[b]), permille(d.border[b], d.tiles[b]));
			}
			break;
		}
	}
	return out;
}

std::string renderMarkdown(const ReportInput& in) {
	std::string out = "# Reference map analysis (G3)\n\n";
	out += std::format("- Source SHA-256: `{}`\n- Client: `{}`, compatibility: **{}**\n- OTBM {}, {}x{}, items {}.{}, {} bytes{}\n",
		in.sha256, in.clientId, in.compatibility, in.header.otbmVersion, in.header.width, in.header.height, in.header.itemsMajor,
		in.header.itemsMinor, in.header.fileBytes, in.header.compressed ? ", compressed" : "");
	out += std::format("- Tiles {}, items {}, loader warnings {}\n", in.tiles, in.items, in.loaderWarnings);
	out += std::format("- Unknown content: {} ground, {} stacked, {} distinct ids\n", in.unknownGround, in.unknownItems, in.unknownIds.size());
	const uint64_t coveredTiles = in.tiles;
	const uint64_t unknownTiles = in.semantic->tilesPerTrait[15];
	out += std::format("- Analysis coverage: {} of {} tiles have no unknown content ({})\n", coveredTiles - unknownTiles, coveredTiles,
		percent(coveredTiles - unknownTiles, coveredTiles));
	out += std::format("- Options: impassable ground at >= {}% blocking tiles, small pocket < {} cells\n\n",
		in.topologyOptions.impassablePercent, in.topologyOptions.smallComponentCells);

	out += "## Floors\n\n| z | tiles | walkable | components | largest | small pockets | floor changes | teleports |\n|---|---|---|---|---|---|---|---|\n";
	for (const auto& f : in.topology->floors) {
		out += std::format("| {} | {} | {} | {} | {} | {} | {} | {} |\n", f.bounds.z, f.tiles, f.walkableCells, f.walkable.count,
			f.walkable.largest, f.walkable.smallCount, f.floorChangeCells, f.teleportCells);
	}

	out += "\n## Ground keys (top 20 by tiles)\n\n| key | tiles | blocking | impassable | regions | largest | enclosed |\n|---|---|---|---|---|---|---|\n";
	const auto order = keysByTiles(in);
	for (size_t n = 0; n < std::min<size_t>(20, order.size()); ++n) {
		const size_t k = order[n];
		const auto& stat = in.semantic->groundKeyStats[k];
		const auto& r = in.topology->keys[k];
		if (stat.tiles == 0) break;
		out += std::format("| {} | {} | {} | {} | {} | {} | {} |\n", mdCell(keyLabel(in, k)), stat.tiles, percent(stat.blockingTiles, stat.tiles),
			r.impassable ? "yes" : "no", r.regions, r.largest, r.enclosed);
	}

	out += "\n## Trait rate by distance to impassable ground\n\n| distance | tiles | doodad | wall | border |\n|---|---|---|---|---|\n";
	const auto& d = in.features->distance;
	for (size_t b = 0; b < distanceBinCount; ++b) {
		out += std::format("| {} | {} | {} | {} | {} |\n", distanceLabel(b), d.tiles[b], percent(d.doodad[b], d.tiles[b]),
			percent(d.wall[b], d.tiles[b]), percent(d.border[b], d.tiles[b]));
	}

	out += "\n## Blocks (decile histograms)\n\n| scale | blocks | mostly walkable (>=90%) | mostly impassable ground (>=90%) |\n|---|---|---|---|\n";
	for (const auto& sc : in.features->scales) {
		out += std::format("| {} | {} | {} | {} |\n", sc.scale, sc.blocks, percent(sc.walkableShare[9], sc.blocks), percent(sc.impassableShare[9], sc.blocks));
	}

	const auto& adj = in.features->adjacency;
	struct Pair { size_t a, b; uint64_t count; };
	std::vector<Pair> pairs;
	for (size_t a = 0; a < adj.keyCount; ++a) {
		for (size_t b = 0; b < adj.keyCount; ++b) {
			if (a == b) continue;
			const uint64_t c = adj.east[a * adj.keyCount + b] + adj.south[a * adj.keyCount + b]
				+ adj.east[b * adj.keyCount + a] + adj.south[b * adj.keyCount + a];
			if (a < b && c > 0) pairs.push_back({a, b, c});
		}
	}
	std::ranges::stable_sort(pairs, [](const Pair& x, const Pair& y) { return x.count > y.count; });
	out += "\n## Most common ground transitions (a != b, both directions summed)\n\n| a | b | neighbour pairs |\n|---|---|---|\n";
	for (size_t n = 0; n < std::min<size_t>(15, pairs.size()); ++n) {
		out += std::format("| {} | {} | {} |\n", mdCell(keyLabel(in, pairs[n].a)), mdCell(keyLabel(in, pairs[n].b)), pairs[n].count);
	}

	if (!in.unknownIds.empty()) {
		out += "\n## Unknown ids\n\n| id | count |\n|---|---|\n";
		for (const auto& u : in.unknownIds) out += std::format("| {} | {} |\n", u.id, u.count);
	}
	out += "\nFull data: `reference-analysis.json` and the CSV tables. Aggregates only; no tile coordinates.\n";
	return out;
}

} // namespace rme::procedural
