// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/generation_input.h"
#include <array>
#include <string>

namespace rme::procedural {

struct GroundProfile {
	// Grass, Dirt, Water, in Terrain enum order. Names are resolved in loaded assets.
	std::array<std::string, 3> brushes;
};

struct GroundPlan {
	GenerationConfig config;
	GroundProfile profile;
	std::string client;
	uint64_t mapGeneration = 0;
	uint64_t mapRevision = 0;
	uint64_t assetRevision = 0;
	uint64_t semanticFingerprint = 0;
	// Row-major brush placement IDs; 0 means Preserve, never erase.
	std::vector<uint16_t> items;
	[[nodiscard]] uint64_t fingerprint() const;
};

enum class ResolveError {
	WrongThread, NoClient, StaleAssets, InvalidInput, InvalidProfile, InvalidItem,
	PlanLimit, Cancelled, AllocationFailed
};

// GUI thread only. No map/global brush/RNG mutation. This plan still needs the
// DrawOperations sandbox, halo/diff validation and stale checks before commit.
// maxPlanBytes accounts for this result's payload, not the full generation job.
std::expected<GroundPlan, ResolveError> resolveGroundPlan(
	const GenerationInput& input, const SemanticGrid& grid, const GroundProfile& profile,
	uint64_t maxPlanBytes, std::stop_token stop = {});

} // namespace rme::procedural
