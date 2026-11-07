// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/similarity.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <new>

namespace rme::procedural {
namespace {

constexpr uint64_t base1 = 0x9E3779B97F4A7C15ull;
constexpr uint64_t base2 = 0xC2B2AE3D27D4EB4Full;
constexpr uint64_t emptyCell = 0x51ED270B0B9E6A4Dull;

constexpr uint64_t mix(uint64_t value) noexcept {
	value += 0x9E3779B97F4A7C15ull;
	value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
	value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
	return value ^ (value >> 31);
}

constexpr uint64_t nameHash(std::string_view name) noexcept {
	uint64_t h = 0xCBF29CE484222325ull;
	for (const char c : name) h = (h ^ static_cast<uint8_t>(c)) * 0x100000001B3ull;
	return mix(h);
}

constexpr uint64_t power(uint64_t base, uint32_t exponent) noexcept {
	uint64_t result = 1;
	for (uint32_t i = 0; i < exponent; ++i) result *= base;
	return result;
}

constexpr uint64_t geometricSum(uint64_t base, uint32_t count) noexcept {
	uint64_t sum = 0, term = 1;
	for (uint32_t i = 0; i < count; ++i) { sum += term; term *= base; }
	return sum;
}

} // namespace

std::expected<Fingerprint, SimilarityError> computeFingerprint(
	const SemanticMap& map, const FingerprintOptions& options, std::stop_token stop) {
	const uint32_t window = options.window;
	if (window < 2 || window > 256 || options.sampleModulo == 0 || options.minPresentPercent > 100) {
		return std::unexpected(SimilarityError::InvalidOptions);
	}
	try {
		Fingerprint result;
		result.options = options;

		const auto& keys = map.groundKeys();
		std::vector<uint64_t> keyValue(keys.size());
		for (size_t i = 0; i < keys.size(); ++i) keyValue[i] = nameHash(keys[i]);

		// A window whose cells all hold value v hashes to v * G1 * G2 before the final mix.
		const uint64_t uniformFactor = geometricSum(base1, window) * geometricSum(base2, window);
		std::vector<uint64_t> uniform;
		uniform.reserve(keys.size() + 1);
		uniform.push_back(mix(emptyCell * uniformFactor));
		for (const uint64_t v : keyValue) uniform.push_back(mix(v * uniformFactor));
		std::sort(uniform.begin(), uniform.end());

		const uint64_t base1Window = power(base1, window), base2Window = power(base2, window);
		const uint64_t minPresent = (static_cast<uint64_t>(window) * window * options.minPresentPercent + 99) / 100;

		for (const SemanticFloor& floor : map.floors()) {
			const uint32_t width = static_cast<uint32_t>(floor.bounds.width), height = static_cast<uint32_t>(floor.bounds.height);
			if (width < window || height < window) continue;
			const uint32_t columns = width - window + 1;
			std::vector<uint64_t> ring(static_cast<size_t>(window) * columns), vertical(columns), rowHash(columns);
			std::vector<uint32_t> present(width);

			auto cellValue = [&](uint32_t x, uint32_t y) {
				const size_t index = static_cast<size_t>(y) * width + x;
				return (floor.traits[index] & bit(Trait::Present)) ? keyValue[floor.ground[index]] : emptyCell;
			};
			auto isPresent = [&](uint32_t x, uint32_t y) {
				return (floor.traits[static_cast<size_t>(y) * width + x] & bit(Trait::Present)) != 0 ? 1u : 0u;
			};

			for (uint32_t y = 0; y < height; ++y) {
				if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
				uint64_t h = 0;
				for (uint32_t i = 0; i < window; ++i) h = h * base1 + cellValue(i, y);
				rowHash[0] = h;
				for (uint32_t x = 1; x < columns; ++x) {
					h = h * base1 - cellValue(x - 1, y) * base1Window + cellValue(x + window - 1, y);
					rowHash[x] = h;
				}
				uint64_t* slot = ring.data() + static_cast<size_t>(y % window) * columns;
				for (uint32_t x = 0; x < columns; ++x) {
					vertical[x] = y >= window ? vertical[x] * base2 - slot[x] * base2Window + rowHash[x]
					                          : vertical[x] * base2 + rowHash[x];
					slot[x] = rowHash[x];
				}
				for (uint32_t x = 0; x < width; ++x) {
					present[x] += isPresent(x, y);
					if (y >= window) present[x] -= isPresent(x, y - window);
				}
				if (y + 1 < window) continue;

				uint64_t sum = 0;
				for (uint32_t i = 0; i < window; ++i) sum += present[i];
				for (uint32_t x = 0; x < columns; ++x) {
					if (x > 0) { sum += present[x + window - 1]; sum -= present[x - 1]; }
					++result.windows;
					if (sum < minPresent) { ++result.sparseWindows; continue; }
					const uint64_t hash = mix(vertical[x]);
					if (hash % options.sampleModulo != 0) continue;
					++result.candidateWindows;
					if (std::binary_search(uniform.begin(), uniform.end(), hash)) { ++result.uniformWindows; continue; }
					result.hashes.push_back(hash);
				}
			}
		}
		std::sort(result.hashes.begin(), result.hashes.end());
		result.hashes.erase(std::unique(result.hashes.begin(), result.hashes.end()), result.hashes.end());
		return result;
	} catch (const std::bad_alloc&) {
		return std::unexpected(SimilarityError::AllocationFailed);
	}
}

Containment compareFingerprints(const Fingerprint& a, const Fingerprint& b) {
	Containment c;
	c.a = a.hashes.size();
	c.b = b.hashes.size();
	c.comparable = a.options == b.options && c.a > 0 && c.b > 0;
	if (!c.comparable) return c;
	size_t i = 0, j = 0;
	while (i < a.hashes.size() && j < b.hashes.size()) {
		if (a.hashes[i] < b.hashes[j]) ++i;
		else if (b.hashes[j] < a.hashes[i]) ++j;
		else { ++c.shared; ++i; ++j; }
	}
	return c;
}

std::vector<Fingerprint> withoutGenericWindows(std::span<const Fingerprint> all, uint32_t genericAtLeast) {
	std::vector<Fingerprint> result(all.begin(), all.end());
	if (genericAtLeast < 2) return result;
	size_t total = 0;
	for (const auto& f : all) total += f.hashes.size();
	std::vector<uint64_t> pooled;
	pooled.reserve(total);
	for (const auto& f : all) pooled.insert(pooled.end(), f.hashes.begin(), f.hashes.end());
	std::sort(pooled.begin(), pooled.end());
	for (auto& f : result) {
		std::erase_if(f.hashes, [&](uint64_t h) {
			const auto range = std::equal_range(pooled.begin(), pooled.end(), h);
			return static_cast<uint64_t>(range.second - range.first) >= genericAtLeast;
		});
	}
	return result;
}

std::string serializeFingerprint(const Fingerprint& f) {
	std::string out = std::format("g4-fingerprint/1 window={} modulo={} minpresent={}\n", f.options.window, f.options.sampleModulo, f.options.minPresentPercent);
	out += std::format("counts windows={} sparse={} candidates={} uniform={} kept={}\n", f.windows, f.sparseWindows, f.candidateWindows, f.uniformWindows, f.hashes.size());
	for (const uint64_t h : f.hashes) out += std::format("{:016x}\n", h);
	return out;
}

std::expected<Fingerprint, SimilarityError> parseFingerprint(std::string_view text) {
	auto nextLine = [&text]() {
		const size_t end = text.find('\n');
		const std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		return line;
	};
	auto number = [](std::string_view line, std::string_view key, uint64_t& out) {
		const size_t at = line.find(key);
		if (at == std::string_view::npos) return false;
		const char* begin = line.data() + at + key.size();
		const auto r = std::from_chars(begin, line.data() + line.size(), out);
		return r.ec == std::errc {};
	};
	Fingerprint f;
	std::string_view line = nextLine();
	uint64_t window = 0, modulo = 0, minPresent = 0;
	if (!line.starts_with("g4-fingerprint/1 ") || !number(line, "window=", window) || !number(line, "modulo=", modulo) ||
	    !number(line, "minpresent=", minPresent) || window > 256 || modulo > UINT32_MAX || minPresent > 100) {
		return std::unexpected(SimilarityError::BadFormat);
	}
	f.options = {static_cast<uint32_t>(window), static_cast<uint32_t>(modulo), static_cast<uint32_t>(minPresent)};
	line = nextLine();
	uint64_t kept = 0;
	if (!line.starts_with("counts ") || !number(line, "windows=", f.windows) || !number(line, "sparse=", f.sparseWindows) ||
	    !number(line, "candidates=", f.candidateWindows) || !number(line, "uniform=", f.uniformWindows) || !number(line, "kept=", kept)) {
		return std::unexpected(SimilarityError::BadFormat);
	}
	try {
		f.hashes.reserve(static_cast<size_t>(std::min<uint64_t>(kept, 1u << 26)));
		while (!text.empty()) {
			line = nextLine();
			if (line.empty()) continue;
			uint64_t h = 0;
			const auto r = std::from_chars(line.data(), line.data() + line.size(), h, 16);
			if (r.ec != std::errc {} || r.ptr != line.data() + line.size()) return std::unexpected(SimilarityError::BadFormat);
			f.hashes.push_back(h);
		}
	} catch (const std::bad_alloc&) {
		return std::unexpected(SimilarityError::AllocationFailed);
	}
	if (f.hashes.size() != kept || !std::is_sorted(f.hashes.begin(), f.hashes.end()) ||
	    std::adjacent_find(f.hashes.begin(), f.hashes.end()) != f.hashes.end()) {
		return std::unexpected(SimilarityError::BadFormat);
	}
	return f;
}

} // namespace rme::procedural
