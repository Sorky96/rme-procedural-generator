// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/generation_input.h"
#include <array>
#include <span>
#include <string>

namespace rme::procedural {

struct GroundProfile {
	// GroundRoll: Grass, Dirt, Water (count 3). Surface: Grass, Dirt, Rock, Shore, Lake, River (count 6), or with a
	// seventh brush for Path cells (count 7), or with an eighth for Floor cells as well (count 8). A grid may only
	// contain the kinds its profile has brushes for.
	// Names are resolved in loaded assets; the same brush may serve several kinds. Entries past
	// `count` are unused and must stay empty.
	static constexpr uint8_t groundRollCount = 3;
	static constexpr uint8_t surfaceCount = 6;
	static constexpr uint8_t surfaceWithPathCount = 7;
	static constexpr uint8_t surfaceWithFloorCount = 8; // Path brush at index 6 (always filled), Floor brush at index 7
	std::array<std::string, 8> brushes;
	uint8_t count = groundRollCount;
	[[nodiscard]] std::span<const std::string> active() const noexcept { return std::span(brushes).first(count); }
	// Vegetation doodad brushes in DecorKind order after None: Tree, Bush, Stone. decorCount is 0 (none)
	// or decorationCount; entries past it must stay empty.
	static constexpr uint8_t decorationCount = 3;
	std::array<std::string, 3> decor;
	uint8_t decorCount = 0;
	[[nodiscard]] std::span<const std::string> activeDecor() const noexcept { return std::span(decor).first(decorCount); }
	bool operator==(const GroundProfile&) const = default;
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
	// Row-major vegetation item IDs placed on top of the ground; empty when vegetation is off,
	// otherwise one entry per cell with 0 meaning none.
	std::vector<uint16_t> decor;
	[[nodiscard]] uint64_t fingerprint() const;
};

// True when a ground brush of the profile has an outer border to "nothing": the border calculator then draws
// it onto empty neighbour tiles, which for a generated area means outside it. Such profiles need
// surfaceKeepClear(snapshot, true). Brushes that do not resolve count as false; resolution reports them.
[[nodiscard]] bool profileNeedsEdgeClearance(const GroundProfile& profile);

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
