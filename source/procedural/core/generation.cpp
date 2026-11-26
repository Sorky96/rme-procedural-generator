// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/generation.h"

#include <limits>
#include <new>
#include <stdexcept>

namespace rme::procedural {

StableRandom::StableRandom(uint64_t seed, uint64_t stream) {
	std::seed_seq sequence { uint32_t(seed), uint32_t(seed >> 32), uint32_t(stream), uint32_t(stream >> 32) };
	engine.seed(sequence);
}

uint64_t StableRandom::next() { return engine(); }

uint64_t StableRandom::bounded(uint64_t exclusiveUpperBound) {
	if (exclusiveUpperBound == 0) {
		throw std::invalid_argument("Random range must be nonzero");
	}
	const uint64_t threshold = (uint64_t{0} - exclusiveUpperBound) % exclusiveUpperBound;
	uint64_t value;
	do {
		value = next();
	} while (value < threshold);
	return value % exclusiveUpperBound;
}

bool SemanticGrid::valid() const noexcept {
	if (region.x < 0 || region.y < 0 || region.z < 0 || region.width == 0 || region.height == 0 ||
		uint64_t(region.width) * region.height != cells.size()) return false;
	for (const auto terrain : cells) {
		if (terrain != Terrain::Preserve && terrain != Terrain::Grass &&
			terrain != Terrain::Dirt && terrain != Terrain::Water) return false;
	}
	return true;
}

uint64_t SemanticGrid::fingerprint() const {
	// FNV-1a over explicit little-endian fields, never native struct padding.
	uint64_t hash = 14695981039346656037ULL;
	const auto byte = [&](uint8_t value) { hash = (hash ^ value) * 1099511628211ULL; };
	const auto word = [&](uint32_t value) {
		for (int shift = 0; shift < 32; shift += 8) byte(uint8_t(value >> shift));
	};
	word(generationAlgorithmVersion);
	word(uint32_t(region.x)); word(uint32_t(region.y)); word(uint32_t(region.z));
	word(region.width); word(region.height);
	for (const auto terrain : cells) byte(static_cast<uint8_t>(terrain));
	return hash;
}

std::expected<GenerationLayout, GenerationError> validateGeneration(
	const GenerationConfig& config, MapExtent extent, CoreBudget budget,
	std::span<const uint8_t> mask, std::span<const uint8_t> protection, std::stop_token stop) {
	if (stop.stop_requested()) return std::unexpected(GenerationError::Cancelled);
	const auto& r = config.region;
	if (r.x < 0 || r.y < 0 || r.z < 0 || r.width == 0 || r.height == 0 ||
		uint64_t(r.x) + r.width > extent.width || uint64_t(r.y) + r.height > extent.height ||
		uint64_t(r.z) >= extent.layers) {
		return std::unexpected(GenerationError::InvalidRegion);
	}
	if (uint64_t(config.grassWeight) + config.dirtWeight + config.waterWeight == 0) {
		return std::unexpected(GenerationError::InvalidWeights);
	}
	const uint64_t count = uint64_t(r.width) * r.height;
	if (count > std::numeric_limits<size_t>::max() / sizeof(Terrain) ||
		count > std::vector<Terrain>().max_size() ||
		count * sizeof(Terrain) > std::numeric_limits<size_t>::max() - sizeof(SemanticGrid)) {
		return std::unexpected(GenerationError::SizeOverflow);
	}
	if (count > budget.maxCells) return std::unexpected(GenerationError::CellLimit);
	const auto bytes = sizeof(SemanticGrid) + size_t(count) * sizeof(Terrain);
	if (bytes > budget.maxOutputBytes) return std::unexpected(GenerationError::OutputLimit);
	if ((!mask.empty() && mask.size() != count) || (!protection.empty() && protection.size() != count)) {
		return std::unexpected(GenerationError::InvalidMask);
	}
	bool writable = false;
	for (size_t i = 0; i < size_t(count); ++i) {
		if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(GenerationError::Cancelled);
		if ((!mask.empty() && mask[i] > 1) || (!protection.empty() && protection[i] > 1)) {
			return std::unexpected(GenerationError::InvalidMask);
		}
		writable |= (mask.empty() || mask[i] != 0) && (protection.empty() || protection[i] == 0);
	}
	if (!writable) return std::unexpected(GenerationError::NoWritableCells);
	return GenerationLayout { size_t(count), bytes };
}

std::expected<SemanticGrid, GenerationError> generateTerrain(
	const GenerationConfig& config, MapExtent extent, CoreBudget budget,
	std::span<const uint8_t> mask, std::span<const uint8_t> protection, std::stop_token stop) {
	if (stop.stop_requested()) return std::unexpected(GenerationError::Cancelled);
	const auto layout = validateGeneration(config, extent, budget, mask, protection, stop);
	if (!layout) return std::unexpected(layout.error());
	if (stop.stop_requested()) return std::unexpected(GenerationError::Cancelled);
	try {
		SemanticGrid result { config.region, std::vector<Terrain>(layout->cellCount, Terrain::Preserve) };
		constexpr uint64_t terrainStream = 0x7465727261696eULL; // "terrain", fixed in algorithm v1.
		StableRandom random(config.seed, terrainStream);
		const uint64_t dirtEnd = uint64_t(config.grassWeight) + config.dirtWeight;
		const uint64_t total = dirtEnd + config.waterWeight;
		for (size_t i = 0; i < layout->cellCount; ++i) {
			if (i % config.region.width == 0 && stop.stop_requested()) {
				return std::unexpected(GenerationError::Cancelled);
			}
			const auto roll = random.bounded(total);
			if ((!mask.empty() && mask[i] == 0) || (!protection.empty() && protection[i] != 0)) continue;
			result.cells[i] = roll < config.grassWeight ? Terrain::Grass : roll < dirtEnd ? Terrain::Dirt : Terrain::Water;
		}
		if (stop.stop_requested()) return std::unexpected(GenerationError::Cancelled);
		return result;
	} catch (const std::bad_alloc&) {
		return std::unexpected(GenerationError::AllocationFailed);
	}
}

std::vector<uint8_t> protectionMarginCells(std::span<const uint8_t> protection, uint32_t width, uint32_t height,
	uint32_t margin) {
	margin = margin < maxProtectionMargin ? margin : maxProtectionMargin;
	if (!margin || protection.size() != size_t(width) * height) return {};
	// Rows first, then columns: a separable square dilation.
	std::vector<uint8_t> rows(protection.size()), out(protection.size());
	bool any = false;
	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			if (!protection[size_t(y) * width + x]) continue;
			any = true;
			const uint32_t x0 = x >= margin ? x - margin : 0, x1 = x + margin < width ? x + margin : width - 1;
			for (uint32_t xx = x0; xx <= x1; ++xx) rows[size_t(y) * width + xx] = 1;
		}
	}
	if (!any) return {};
	for (uint32_t y = 0; y < height; ++y)
		for (uint32_t x = 0; x < width; ++x) {
			if (!rows[size_t(y) * width + x]) continue;
			const uint32_t y0 = y >= margin ? y - margin : 0, y1 = y + margin < height ? y + margin : height - 1;
			for (uint32_t yy = y0; yy <= y1; ++yy) out[size_t(yy) * width + x] = 1;
		}
	for (size_t i = 0; i < out.size(); ++i) out[i] = out[i] && !protection[i];
	return out;
}

} // namespace rme::procedural
