// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/png_writer.h"

#include <algorithm>
#include <array>
#include <limits>
#include <new>

namespace rme::procedural {

namespace {

constexpr std::array<uint32_t, 256> makeCrcTable() {
	std::array<uint32_t, 256> table {};
	for (uint32_t n = 0; n < 256; ++n) {
		uint32_t c = n;
		for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
		table[n] = c;
	}
	return table;
}
constexpr auto crcTable = makeCrcTable();

constexpr size_t storedBlockMax = 65535;

void putU32(std::vector<uint8_t>& out, uint32_t value) {
	for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(value >> shift));
}

void putChunk(std::vector<uint8_t>& out, const char (&type)[5], std::span<const uint8_t> data) {
	putU32(out, static_cast<uint32_t>(data.size()));
	const size_t typeStart = out.size();
	for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(type[i]));
	out.insert(out.end(), data.begin(), data.end());
	putU32(out, crc32(std::span<const uint8_t>(out).subspan(typeStart)));
}

} // namespace

uint32_t crc32(std::span<const uint8_t> data) noexcept {
	uint32_t c = 0xFFFFFFFFu;
	for (const uint8_t byte : data) c = crcTable[(c ^ byte) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFu;
}

uint32_t adler32(std::span<const uint8_t> data) noexcept {
	uint32_t a = 1, b = 0;
	for (const uint8_t byte : data) {
		a = (a + byte) % 65521;
		b = (b + a) % 65521;
	}
	return (b << 16) | a;
}

std::expected<std::vector<uint8_t>, PngError> encodeIndexedPng(
	uint32_t width, uint32_t height, std::span<const PngColor> palette, std::span<const uint8_t> indices) {
	if (width == 0 || height == 0) return std::unexpected(PngError::InvalidSize);
	if (palette.empty() || palette.size() > 256) return std::unexpected(PngError::InvalidPalette);
	const uint64_t cells = static_cast<uint64_t>(width) * height;
	if (indices.size() != cells) return std::unexpected(PngError::SizeMismatch);
	const uint64_t rawBytes = cells + height; // one filter byte per row
	if (rawBytes > 1'500'000'000ull) return std::unexpected(PngError::TooLarge);
	for (const uint8_t index : indices) {
		if (index >= palette.size()) return std::unexpected(PngError::IndexOutOfPalette);
	}

	try {
		// Raw scanlines: filter 0 (none) then the row.
		std::vector<uint8_t> raw;
		raw.reserve(static_cast<size_t>(rawBytes));
		for (uint32_t y = 0; y < height; ++y) {
			raw.push_back(0);
			const auto row = indices.subspan(static_cast<size_t>(y) * width, width);
			raw.insert(raw.end(), row.begin(), row.end());
		}

		// zlib stream: header, stored blocks, Adler-32 of the raw data.
		std::vector<uint8_t> zlib;
		zlib.reserve(raw.size() + raw.size() / storedBlockMax * 5 + 16);
		zlib.push_back(0x78);
		zlib.push_back(0x01);
		for (size_t pos = 0; pos < raw.size() || pos == 0;) {
			const size_t len = std::min(storedBlockMax, raw.size() - pos);
			const bool last = pos + len >= raw.size();
			zlib.push_back(last ? 1 : 0);
			zlib.push_back(static_cast<uint8_t>(len & 0xFF));
			zlib.push_back(static_cast<uint8_t>(len >> 8));
			zlib.push_back(static_cast<uint8_t>(~len & 0xFF));
			zlib.push_back(static_cast<uint8_t>((~len >> 8) & 0xFF));
			zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
			pos += len;
			if (last) break;
		}
		putU32(zlib, adler32(raw));

		std::vector<uint8_t> png {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
		png.reserve(zlib.size() + 64 + palette.size() * 3);
		std::vector<uint8_t> header;
		putU32(header, width);
		putU32(header, height);
		header.insert(header.end(), {8, 3, 0, 0, 0}); // 8 bit, indexed, deflate, filter 0, no interlace
		putChunk(png, "IHDR", header);
		std::vector<uint8_t> plte;
		for (const PngColor& color : palette) plte.insert(plte.end(), {color.r, color.g, color.b});
		putChunk(png, "PLTE", plte);
		putChunk(png, "IDAT", zlib);
		putChunk(png, "IEND", {});
		return png;
	} catch (const std::bad_alloc&) {
		return std::unexpected(PngError::AllocationFailed);
	}
}

} // namespace rme::procedural
