// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace rme::procedural {

struct PngColor {
	uint8_t r = 0, g = 0, b = 0;
};

enum class PngError : uint8_t { InvalidSize, InvalidPalette, IndexOutOfPalette, SizeMismatch, TooLarge, AllocationFailed };

// 8-bit indexed PNG with uncompressed (stored) deflate blocks. Output depends only on the input,
// so the same image gives the same bytes; no compression library is needed. Files are about as
// large as the raw pixels (one byte per cell plus one filter byte per row).
// indices holds width * height palette indices, row-major.
[[nodiscard]] std::expected<std::vector<uint8_t>, PngError> encodeIndexedPng(
	uint32_t width, uint32_t height, std::span<const PngColor> palette, std::span<const uint8_t> indices);

// Exposed for tests of the encoder against independent checks.
[[nodiscard]] uint32_t crc32(std::span<const uint8_t> data) noexcept;
[[nodiscard]] uint32_t adler32(std::span<const uint8_t> data) noexcept;

} // namespace rme::procedural
