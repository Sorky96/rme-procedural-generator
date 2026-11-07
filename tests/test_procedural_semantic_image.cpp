// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/semantic_image.h"

#include <array>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace rme::procedural;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
	if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

// Independent reference implementations (bitwise CRC, textbook Adler) so the encoder is not
// checked against its own helpers.
uint32_t refCrc(const uint8_t* data, size_t size) {
	uint32_t c = 0xFFFFFFFFu;
	for (size_t i = 0; i < size; ++i) {
		c ^= data[i];
		for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
	}
	return ~c;
}
uint32_t refAdler(const std::vector<uint8_t>& data) {
	uint32_t a = 1, b = 0;
	for (const uint8_t x : data) { a = (a + x) % 65521; b = (b + a) % 65521; }
	return (b << 16) | a;
}
uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

struct Decoded {
	uint32_t width = 0, height = 0;
	std::vector<uint8_t> palette; // rgb triples
	std::vector<uint8_t> indices; // width * height
	std::vector<std::string> chunks;
};

// Decodes only what encodeIndexedPng produces (stored deflate blocks); nullopt on any inconsistency.
std::optional<Decoded> decode(const std::vector<uint8_t>& png) {
	static constexpr uint8_t sig[8] {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
	if (png.size() < 8 || std::memcmp(png.data(), sig, 8) != 0) return std::nullopt;
	Decoded d;
	std::vector<uint8_t> idat;
	for (size_t pos = 8; pos < png.size();) {
		if (pos + 12 > png.size()) return std::nullopt;
		const uint32_t len = be32(&png[pos]);
		if (pos + 12 + len > png.size()) return std::nullopt;
		const std::string type(reinterpret_cast<const char*>(&png[pos + 4]), 4);
		if (refCrc(&png[pos + 4], 4 + len) != be32(&png[pos + 8 + len])) return std::nullopt;
		const uint8_t* body = &png[pos + 8];
		d.chunks.push_back(type);
		if (type == "IHDR") {
			if (len != 13 || body[8] != 8 || body[9] != 3 || body[10] != 0 || body[11] != 0 || body[12] != 0) return std::nullopt;
			d.width = be32(body); d.height = be32(body + 4);
		} else if (type == "PLTE") {
			d.palette.assign(body, body + len);
		} else if (type == "IDAT") {
			idat.insert(idat.end(), body, body + len);
		}
		pos += 12 + len;
	}
	if (idat.size() < 6 || idat[0] != 0x78 || ((idat[0] << 8) | idat[1]) % 31 != 0) return std::nullopt;
	std::vector<uint8_t> raw;
	size_t pos = 2;
	for (bool last = false; !last;) {
		if (pos + 5 > idat.size()) return std::nullopt;
		last = idat[pos] & 1;
		if ((idat[pos] & 6) != 0) return std::nullopt; // only stored blocks
		const size_t len = idat[pos + 1] | (idat[pos + 2] << 8), nlen = idat[pos + 3] | (idat[pos + 4] << 8);
		if ((len ^ 0xFFFF) != nlen || pos + 5 + len > idat.size()) return std::nullopt;
		raw.insert(raw.end(), idat.begin() + pos + 5, idat.begin() + pos + 5 + len);
		pos += 5 + len;
	}
	if (pos + 4 != idat.size() || refAdler(raw) != be32(&idat[pos])) return std::nullopt;
	if (raw.size() != static_cast<size_t>(d.height) * (d.width + 1)) return std::nullopt;
	for (uint32_t y = 0; y < d.height; ++y) {
		const size_t row = static_cast<size_t>(y) * (d.width + 1);
		if (raw[row] != 0) return std::nullopt;
		d.indices.insert(d.indices.end(), raw.begin() + row + 1, raw.begin() + row + 1 + d.width);
	}
	return d;
}

// '.' grass, '#' wall, 'w' water (impassable ground), 'u' unknown content, 'd' door, 't' teleport,
// 'f' floor change, 'b' grass blocked by an item, 'n' tile without ground, ' ' no tile.
SemanticMap build(const std::vector<std::string>& rows, int z = 7, int x0 = 0) {
	const FloorBounds bounds {z, x0, 0, static_cast<int>(rows.front().size()), static_cast<int>(rows.size())};
	auto map = SemanticMap::create(std::vector<FloorBounds> {bounds}, {1'000'000}).value();
	const uint16_t grass = map.internGroundKey("grass").value();
	const uint16_t water = map.internGroundKey("water").value();
	for (size_t y = 0; y < rows.size(); ++y) {
		for (size_t x = 0; x < rows[y].size(); ++x) {
			uint16_t key = grass;
			uint32_t traits = bit(Trait::HasGround);
			switch (rows[y][x]) {
				case ' ': continue;
				case '#': traits |= bit(Trait::Blocking) | bit(Trait::Wall); break;
				case 'w': key = water; traits |= bit(Trait::Blocking); break;
				case 'u': traits |= bit(Trait::Unknown); break;
				case 'd': traits |= bit(Trait::Door); break;
				case 't': traits |= bit(Trait::Teleport); break;
				case 'f': traits |= bit(Trait::FloorChange); break;
				case 'b': traits |= bit(Trait::Blocking); break;
				case 'n': key = noGroundKey; traits = 0; break;
				default: break;
			}
			check(map.set(x0 + static_cast<int>(x), static_cast<int>(y), z, key, traits).has_value(), "set cell");
		}
	}
	return map;
}
}

int main() {
	// --- PNG encoder
	const std::array<PngColor, 3> palette {{{0, 0, 0}, {255, 0, 0}, {0, 0, 255}}};
	const std::vector<uint8_t> pixels {0, 1, 2, 2, 1, 0};
	const auto small = encodeIndexedPng(3, 2, palette, pixels);
	check(small.has_value(), "encodes a 3x2 image");
	const auto decodedSmall = decode(small.value_or(std::vector<uint8_t> {}));
	check(decodedSmall && decodedSmall->width == 3 && decodedSmall->height == 2 && decodedSmall->indices == pixels, "pixels round-trip");
	check(decodedSmall && decodedSmall->palette == std::vector<uint8_t> {0, 0, 0, 255, 0, 0, 0, 0, 255}, "palette round-trips");
	check(decodedSmall && decodedSmall->chunks == std::vector<std::string> {"IHDR", "PLTE", "IDAT", "IEND"}, "chunk order");
	check(small == encodeIndexedPng(3, 2, palette, pixels), "same input, same bytes");

	// More than one stored block (65535 bytes each).
	std::vector<uint8_t> large(300 * 300);
	for (size_t i = 0; i < large.size(); ++i) large[i] = static_cast<uint8_t>((i * 7 + i / 300) % 3);
	const auto big = encodeIndexedPng(300, 300, palette, large);
	const auto decodedBig = decode(big.value_or(std::vector<uint8_t> {}));
	check(decodedBig && decodedBig->indices == large, "multi-block image round-trips");

	check(crc32(std::vector<uint8_t> {'I', 'E', 'N', 'D'}) == 0xAE426082u, "CRC of IEND is the well-known value");
	check(encodeIndexedPng(0, 2, palette, {}).error() == PngError::InvalidSize, "zero width refused");
	check(encodeIndexedPng(3, 2, {}, pixels).error() == PngError::InvalidPalette, "empty palette refused");
	check(encodeIndexedPng(3, 2, palette, std::vector<uint8_t> {0, 1}).error() == PngError::SizeMismatch, "size mismatch refused");
	check(encodeIndexedPng(3, 2, palette, std::vector<uint8_t> {0, 1, 2, 3, 0, 0}).error() == PngError::IndexOutOfPalette, "index outside palette refused");

	// --- Semantic image of a hand-checked map
	const auto grid = build({
		"..#...w",
		".d#.u.w",
		"###.tbw",
		"nf ..ww"});
	const auto topology = analyzeTopology(grid, summarize(grid), {}, {}).value();
	const auto images = renderSemanticImages(grid, topology, {});
	check(images.has_value() && images->floors.size() == 1, "one floor image");
	const auto d = decode(images ? images->floors[0].png : std::vector<uint8_t> {});
	check(d && d->width == 7 && d->height == 4, "image size equals the floor box");
	const uint8_t G = 9, W = 10;   // grass slot (key 1), water slot (key 2)
	const uint8_t U = 1, WALL = 2, DOOR = 3, TELE = 4, FLOOR = 5, BLOCKED = 6, NOGROUND = 7, E = 0;
	const std::vector<uint8_t> expected {
		G, G, WALL, G, G, G, W,
		G, DOOR, WALL, G, U, G, W,
		WALL, WALL, WALL, G, TELE, BLOCKED, W,
		NOGROUND, FLOOR, E, G, G, W, W};
	check(d && d->indices == expected, "every cell has the class worked out by hand");
	check(images && images->legend[static_cast<size_t>(SemanticClass::Wall)].label == "wall", "legend lists fixed classes");
	check(images && images->legend.back().label == "water (impassable)", "impassable key is marked in the legend");
	check(images && images->legend[images->legend.size() - 2].label == "grass", "passable key has a plain label");
	const auto again = renderSemanticImages(grid, topology, {});
	check(images && again && images->floors[0].png == again->floors[0].png && renderSemanticLegendJson(*images) == renderSemanticLegendJson(*again),
		"images and legend are repeatable");

	// Coordinates: pixel (0, 0) is the box origin, also for a box at a negative offset.
	const auto shifted = build({".#"}, 7, -5);
	const auto shiftedTopology = analyzeTopology(shifted, summarize(shifted), {}, {}).value();
	const auto shiftedImages = renderSemanticImages(shifted, shiftedTopology, {});
	check(shiftedImages && shiftedImages->floors[0].bounds.x0 == -5 && decode(shiftedImages->floors[0].png)->indices == std::vector<uint8_t> {G, WALL},
		"box origin and pixels for a negative offset");

	// Slots are reused beyond 247 ground keys and the legend still names every key.
	auto many = SemanticMap::create(std::vector<FloorBounds> {{7, 0, 0, 2, 1}}, {10}).value();
	for (int k = 1; k <= 250; ++k) check(many.internGroundKey("key" + std::to_string(k)).has_value(), "intern");
	check(many.set(0, 0, 7, 1, bit(Trait::HasGround)).has_value() && many.set(1, 0, 7, 248, bit(Trait::HasGround)).has_value(), "set");
	const auto manyTopology = analyzeTopology(many, summarize(many), {}, {}).value();
	const auto manyImages = renderSemanticImages(many, manyTopology, {});
	const auto manyPixels = decode(manyImages ? manyImages->floors[0].png : std::vector<uint8_t> {});
	check(manyImages && manyPixels && manyPixels->indices[0] == manyPixels->indices[1] && manyImages->legend.size() == 9 + 250, "slot reuse past 247 keys");

	// Cancellation and mismatched input.
	std::stop_source cancel;
	cancel.request_stop();
	check(renderSemanticImages(grid, topology, cancel.get_token()).error() == SemanticImageError::Cancelled, "cancelled");
	TopologyReport mismatched = topology;
	mismatched.keys.pop_back();
	check(!renderSemanticImages(grid, mismatched, {}).has_value(), "topology mismatch refused");

	if (failures == 0) std::cout << "SEMANTIC_IMAGE_PASS\n";
	return failures == 0 ? 0 : 1;
}
