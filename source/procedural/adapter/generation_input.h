// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/generation.h"

class Map;

namespace rme::procedural {

// Owned semantic inputs only: no live tiles, brush pointers or GUI objects.
// This is not the tile/halo snapshot required by the concrete resolver/commit.
struct GenerationInput {
	GenerationConfig config;
	MapExtent extent;
	uint64_t mapGeneration = 0;
	uint64_t mapRevision = 0;
	uint64_t assetRevision = 0;
	std::vector<uint8_t> mask;
	std::vector<uint8_t> protection;
};

enum class InputError { WrongThread, InvalidGeneration, InputLimit, Cancelled, AllocationFailed };

// Conservative G1 empty_only policy: every existing object/metadata is protected.
// Call synchronously on the GUI thread. The returned data can be moved to a worker.
std::expected<GenerationInput, InputError> captureGenerationInput(
	const Map& map, const GenerationConfig& config, CoreBudget coreBudget,
	uint64_t maxInputBytes, std::span<const uint8_t> mask = {}, std::stop_token stop = {});

} // namespace rme::procedural
