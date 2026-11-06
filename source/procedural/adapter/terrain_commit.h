// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/terrain_target.h"
#include <cstdint>
#include <variant>

namespace rme::procedural {

struct TerrainCommitBudget {
	uint64_t maxChanges = 0;
	// Joint conservative estimate: PreparedTerrain plus transition and retained Undo state.
	// Excludes other history, assets, observers, renderer, allocator/RSS and other jobs.
	uint64_t maxBytes = 0;
};

enum class TerrainCommitFailure {
	MemoryLimit, ChangeLimit, BusyHistory, ActiveLuaTransaction, Cancelled,
	AllocationFailed, HistoryRefused
};
using TerrainCommitError = std::variant<TerrainCommitFailure, TargetValidationError>;

struct TerrainCommitReceipt {
	uint64_t changes = 0;
	uint64_t transitionBytes = 0;
	uint64_t totalBytes = 0;
	uint64_t previewFingerprint = 0;
};

} // namespace rme::procedural
