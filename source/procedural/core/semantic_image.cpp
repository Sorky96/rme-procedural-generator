// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/semantic_image.h"

#include <algorithm>
#include <format>
#include <new>

namespace rme::procedural {

namespace {

// Integer HSV (h 0..359, s and v 0..255) to RGB; no floating point, so palettes are exact.
PngColor hsv(uint32_t h, uint32_t s, uint32_t v) {
	const uint32_t region = h / 60, remainder = (h % 60) * 255 / 60;
	const uint32_t p = v * (255 - s) / 255;
	const uint32_t q = v * (255 - s * remainder / 255) / 255;
	const uint32_t t = v * (255 - s * (255 - remainder) / 255) / 255;
	switch (region) {
		case 0: return {static_cast<uint8_t>(v), static_cast<uint8_t>(t), static_cast<uint8_t>(p)};
		case 1: return {static_cast<uint8_t>(q), static_cast<uint8_t>(v), static_cast<uint8_t>(p)};
		case 2: return {static_cast<uint8_t>(p), static_cast<uint8_t>(v), static_cast<uint8_t>(t)};
		case 3: return {static_cast<uint8_t>(p), static_cast<uint8_t>(q), static_cast<uint8_t>(v)};
		case 4: return {static_cast<uint8_t>(t), static_cast<uint8_t>(p), static_cast<uint8_t>(v)};
		default: return {static_cast<uint8_t>(v), static_cast<uint8_t>(p), static_cast<uint8_t>(q)};
	}
}

uint8_t groundSlot(size_t key) noexcept {
	return key == noGroundKey ? static_cast<uint8_t>(SemanticClass::GroundNoBrush)
		: static_cast<uint8_t>(firstGroundSlot + (key - 1) % 247);
}

std::string jsonEscape(std::string_view text) {
	std::string out;
	for (const char c : text) {
		if (c == '"' || c == '\\') { out += '\\'; out += c; }
		else if (static_cast<unsigned char>(c) < 0x20) out += std::format("\\u{:04x}", static_cast<unsigned>(c));
		else out += c;
	}
	return out;
}

} // namespace

std::expected<SemanticImages, SemanticImageError> renderSemanticImages(
	const SemanticMap& map, const TopologyReport& topology, std::stop_token stop) {
	SemanticImages out;
	std::vector<PngColor> palette(256);
	try {
		auto fixed = [&](SemanticClass c, PngColor color, const char* label) {
			palette[static_cast<uint8_t>(c)] = color;
			out.legend.push_back({static_cast<uint8_t>(c), color, label});
		};
		fixed(SemanticClass::Empty, {0, 0, 0}, "no tile");
		fixed(SemanticClass::Unknown, {255, 0, 255}, "unknown content");
		fixed(SemanticClass::Wall, {240, 240, 240}, "wall");
		fixed(SemanticClass::Door, {255, 220, 0}, "door");
		fixed(SemanticClass::Teleport, {0, 220, 220}, "teleport");
		fixed(SemanticClass::FloorChange, {230, 40, 40}, "floor change");
		fixed(SemanticClass::BlockedByItem, {90, 90, 90}, "blocked by an item on passable ground");
		fixed(SemanticClass::NoGround, {120, 70, 20}, "tile without ground");
		fixed(SemanticClass::GroundNoBrush, {150, 150, 150}, "ground without a brush");
		const auto& keys = map.groundKeys();
		for (size_t k = 1; k < keys.size(); ++k) {
			const bool impassable = topology.keys[k].impassable;
			const PngColor color = hsv(static_cast<uint32_t>(k * 137 % 360), impassable ? 200 : 140, impassable ? 110 : 235);
			const uint8_t slot = groundSlot(k);
			if (k <= 247) palette[slot] = color; // reused slots keep the first key's colour
			out.legend.push_back({slot, k <= 247 ? color : palette[slot], keys[k] + (impassable ? " (impassable)" : "")});
		}
		out.floors.reserve(map.floors().size());
	} catch (const std::bad_alloc&) {
		return std::unexpected(SemanticImageError::AllocationFailed);
	}
	if (topology.keys.size() != map.groundKeys().size()) return std::unexpected(SemanticImageError::Png);

	std::vector<uint8_t> indices;
	for (const SemanticFloor& floor : map.floors()) {
		const int w = floor.bounds.width, h = floor.bounds.height;
		try {
			indices.assign(floor.traits.size(), 0);
		} catch (const std::bad_alloc&) {
			return std::unexpected(SemanticImageError::AllocationFailed);
		}
		for (int y = 0; y < h; ++y) {
			if (stop.stop_requested()) return std::unexpected(SemanticImageError::Cancelled);
			for (int x = 0; x < w; ++x) {
				const auto i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
				const uint32_t t = floor.traits[i];
				if (!(t & bit(Trait::Present))) continue;
				SemanticClass cls;
				if (t & bit(Trait::Unknown)) cls = SemanticClass::Unknown;
				else if (t & bit(Trait::Wall)) cls = SemanticClass::Wall;
				else if (t & bit(Trait::Door)) cls = SemanticClass::Door;
				else if (t & bit(Trait::Teleport)) cls = SemanticClass::Teleport;
				else if (t & bit(Trait::FloorChange)) cls = SemanticClass::FloorChange;
				else if (!(t & bit(Trait::HasGround))) cls = SemanticClass::NoGround;
				else if ((t & bit(Trait::Blocking)) && !topology.keys[floor.ground[i]].impassable) cls = SemanticClass::BlockedByItem;
				else { indices[i] = groundSlot(floor.ground[i]); continue; }
				indices[i] = static_cast<uint8_t>(cls);
			}
		}
		auto png = encodeIndexedPng(static_cast<uint32_t>(w), static_cast<uint32_t>(h), palette, indices);
		if (!png) {
			return std::unexpected(png.error() == PngError::AllocationFailed ? SemanticImageError::AllocationFailed : SemanticImageError::Png);
		}
		try {
			out.floors.push_back({floor.bounds, std::move(*png)});
		} catch (const std::bad_alloc&) {
			return std::unexpected(SemanticImageError::AllocationFailed);
		}
	}
	return out;
}

std::string renderSemanticLegendJson(const SemanticImages& images) {
	std::string out = "{\n  \"schema\": \"g3-semantic-legend/1\",\n  \"legend\": [\n";
	for (size_t i = 0; i < images.legend.size(); ++i) {
		const auto& e = images.legend[i];
		out += std::format("    {{\"slot\": {}, \"rgb\": [{}, {}, {}], \"label\": \"{}\"}}{}\n", e.index, e.color.r, e.color.g, e.color.b,
			jsonEscape(e.label), i + 1 < images.legend.size() ? "," : "");
	}
	out += "  ],\n  \"floors\": [\n";
	for (size_t i = 0; i < images.floors.size(); ++i) {
		const auto& f = images.floors[i];
		out += std::format("    {{\"z\": {}, \"x0\": {}, \"y0\": {}, \"width\": {}, \"height\": {}, \"png_bytes\": {}}}{}\n", f.bounds.z, f.bounds.x0,
			f.bounds.y0, f.bounds.width, f.bounds.height, f.png.size(), i + 1 < images.floors.size() ? "," : "");
	}
	out += "  ]\n}\n";
	return out;
}

} // namespace rme::procedural
