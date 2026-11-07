// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/reference_header.h"

#include <algorithm>
#include <bit>

namespace rme::procedural {

namespace {
constexpr std::array<uint32_t, 64> roundConstants = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
}

Sha256::Sha256() noexcept
	: state_ {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19} {}

void Sha256::block(const uint8_t* chunk) noexcept {
	std::array<uint32_t, 64> w;
	for (size_t i = 0; i < 16; ++i) {
		w[i] = (uint32_t(chunk[i * 4]) << 24) | (uint32_t(chunk[i * 4 + 1]) << 16)
			| (uint32_t(chunk[i * 4 + 2]) << 8) | uint32_t(chunk[i * 4 + 3]);
	}
	for (size_t i = 16; i < 64; ++i) {
		const uint32_t s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
		const uint32_t s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	auto [a, b, c, d, e, f, g, h] = state_;
	for (size_t i = 0; i < 64; ++i) {
		const uint32_t s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
		const uint32_t ch = (e & f) ^ (~e & g);
		const uint32_t t1 = h + s1 + ch + roundConstants[i] + w[i];
		const uint32_t s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
		const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
		const uint32_t t2 = s0 + maj;
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
	state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}

void Sha256::update(std::span<const std::byte> data) noexcept {
	totalBytes_ += data.size();
	const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
	size_t remaining = data.size();
	if (buffered_ > 0) {
		const size_t take = std::min(remaining, buffer_.size() - buffered_);
		std::copy_n(bytes, take, buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_));
		buffered_ += take; bytes += take; remaining -= take;
		if (buffered_ < buffer_.size()) return;
		block(buffer_.data());
		buffered_ = 0;
	}
	for (; remaining >= 64; remaining -= 64, bytes += 64) block(bytes);
	std::copy_n(bytes, remaining, buffer_.begin());
	buffered_ = remaining;
}

std::array<uint8_t, 32> Sha256::finish() noexcept {
	const uint64_t bits = totalBytes_ * 8;
	buffer_[buffered_++] = 0x80;
	if (buffered_ > 56) {
		std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.end(), uint8_t {0});
		block(buffer_.data());
		buffered_ = 0;
	}
	std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.begin() + 56, uint8_t {0});
	for (size_t i = 0; i < 8; ++i) buffer_[56 + i] = static_cast<uint8_t>(bits >> (56 - 8 * i));
	block(buffer_.data());
	std::array<uint8_t, 32> digest;
	for (size_t i = 0; i < 8; ++i) {
		for (size_t j = 0; j < 4; ++j) digest[i * 4 + j] = static_cast<uint8_t>(state_[i] >> (24 - 8 * j));
	}
	return digest;
}

std::string Sha256::hex(const std::array<uint8_t, 32>& digest) {
	constexpr char digits[] = "0123456789abcdef";
	std::string text;
	text.reserve(64);
	for (const uint8_t byte : digest) {
		text.push_back(digits[byte >> 4]);
		text.push_back(digits[byte & 15]);
	}
	return text;
}

Compatibility checkCompatibility(const ReferenceHeader& header, const ClientItemVersion* client) noexcept {
	if (header.width == 0 || header.height == 0 || header.itemsMajor == 0) return Compatibility::EmptyHeader;
	if (client == nullptr) return Compatibility::NoClient;
	if (!client->otbmSupported) return Compatibility::OtbmUnsupported;
	if (header.itemsMajor != client->itemsMajor) return Compatibility::ItemsMajorMismatch;
	if (header.itemsMinor > client->itemsMinor) return Compatibility::ItemsMinorNewer;
	return Compatibility::Compatible;
}

const char* compatibilityName(Compatibility value) noexcept {
	switch (value) {
		case Compatibility::Compatible: return "compatible";
		case Compatibility::NoClient: return "no_client";
		case Compatibility::OtbmUnsupported: return "otbm_unsupported";
		case Compatibility::ItemsMajorMismatch: return "items_major_mismatch";
		case Compatibility::ItemsMinorNewer: return "items_minor_newer";
		case Compatibility::EmptyHeader: return "empty_header";
	}
	return "unknown";
}

std::optional<uint64_t> estimateLoadBytes(const ReferenceHeader& header, uint64_t bytesPerFileByte) noexcept {
	const uint64_t bytes = header.compressed ? header.expandedBytes : header.fileBytes;
	if (header.compressed && bytes == 0) return std::nullopt;
	if (bytesPerFileByte != 0 && bytes > UINT64_MAX / bytesPerFileByte) return UINT64_MAX;
	return bytes * bytesPerFileByte;
}

} // namespace rme::procedural
