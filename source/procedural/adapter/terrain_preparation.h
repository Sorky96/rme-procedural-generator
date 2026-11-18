// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/generation_snapshot.h"
#include "procedural/adapter/preparation_steps.h"
#include "procedural/adapter/ground_resolver.h"
#include "procedural/adapter/terrain_preview.h"
#include "procedural/core/reachability.h"
#include "procedural/core/level_repair.h"
#include "procedural/core/candidate_score.h"
#include "procedural/core/copy_guard.h"

#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <variant>

class Map;

namespace rme::procedural {

// All limits fail closed: a zero field rejects every job.
struct PreparationBudget {
	uint64_t maxCells = 0; // Generation rectangle cells.
	uint64_t maxContextCells = 0; // Clipped two-cell halo context captured from the map.
	uint64_t maxChanges = 0; // Writable cells and resulting preview changes.
	uint64_t maxBytes = 0; // One joint estimate for every stage below.
    bool measureCandidate = false; // I52: worker measurement of the actual adapter surface.
    const CopyIndex* copyIndex = nullptr; // Borrowed immutable series index; caller retains through worker join.
};

// Fixed allowance for synchronous stack frames, the wrapper's own locals, RNG engines and seed
// sequences, the preview's temporary client-ID comparison copy and container bookkeeping inside
// the stages. It is an
// estimate charged up front, not a measured or enforced RSS figure.
inline constexpr uint64_t preparationScratchBytes = 64ULL * 1024;

struct PreparationMemory {
	uint64_t infrastructureBytes = 0; // sizeof(PreparedTerrain) + preparationScratchBytes.
	uint64_t snapshotBytes = 0; // GenerationSnapshot::estimatedBytes (owned masks, cells, tile copies).
	uint64_t semanticBytes = 0; // SemanticGrid output plus one allocation allowance.
	uint64_t planBytes = 0; // Conservative GroundPlan upper estimate reserved for the resolver.
	uint64_t previewBytes = 0; // TerrainPreview::getEstimatedPeakBytes().
	uint64_t totalBytes = 0; // Sum of the above; never exceeds PreparationBudget::maxBytes.
};

enum class PreparationInputError {
    WrongThread,
    NoClient,
    InvalidProfile,
    MemoryLimit,
    ContextLimit,
    ChangeLimit,
    Cancelled,
    AllocationFailed,
    InvalidParameters,
    NoLowerFloor,
    UpperFloorOccupied,
    NoUpperFloor
};

// Stage failures are forwarded unchanged so callers see the exact module cause.
using PreparationError = std::variant<PreparationInputError, SnapshotError, GenerationError, ResolveError, PreviewError>;

// One move-only unit: the snapshot the preview was built from, the resolved plan and the sandbox.
struct PreparedTerrain {
	GenerationSnapshot snapshot;
	GroundPlan plan;
	TerrainPreview preview;
	PreparationMemory memory;
	std::unique_ptr<PreparedTerrain> cave;
    std::unique_ptr<PreparedTerrain> plateau;
	struct Levels {
		LevelPlan plan;
		std::vector<uint8_t> blockingMask;
		LevelReachabilityReport report;
        LevelRepairReport repair;
        std::vector<MountainPass> passes;
	};
	std::unique_ptr<Levels> levels;
    std::optional<CandidateMetrics> candidateMetrics;
    CopyAssessment copyAssessment;
};

// Captures, generates, resolves and builds the DrawOperations preview for one region in a single
// synchronous GUI-thread call. Never yields, never mutates the map, global brushes or editor RNG,
// and returns either a complete result or an error with no partial state.
//
// Memory: every stage draws from one remaining-byte counter that starts at budget.maxBytes.
// Infrastructure, then the semantic grid and plan estimates, are reserved before the snapshot is
// captured; the snapshot and preview are charged with their own conservative estimates. Charges
// are never refunded, even after the semantic grid is released. The estimates rely on the same
// standard-container assumptions as estimateSnapshotTileMemory (vector capacity == size on copy,
// fixed per-allocation overhead, std::string capacity rounding bounded by 2*(length+1)). They are
// not a heap or RSS limit. Outside the budget: the source map, the borrowed config/profile/mask,
// loaded client assets and brushes, UI/renderer/C-runtime allocations (including worker thread
// runtime and stack reservation), allocator overhead beyond
// the stated allowances, and any future commit/Undo.
std::expected<PreparedTerrain, PreparationError> prepareTerrain(
	const Map& map, const GenerationConfig& config, const GroundProfile& profile,
	PreparationBudget budget, std::span<const uint8_t> mask = {}, std::stop_token stop = {});

// GUI-only staged variant. Keep map, request and mask alive while advancing; discard the
// continuation if the target disappears. No event dispatch occurs inside advance().
// Surface semantics/decoration and mountain geometry run on owned workers with plain inputs; all map/brush
// access remains on the GUI thread. Destroying the continuation requests stop and joins the worker.
// The additional owned mask is charged to the preparation budget. The synchronous API uses no worker.
using TerrainPreparationSteps = PreparationSteps<std::expected<PreparedTerrain, PreparationError>>;
std::expected<TerrainPreparationSteps, PreparationError> prepareTerrainSteps(
	const Map& map, const GenerationConfig& config, const GroundProfile& profile,
	PreparationBudget budget, std::span<const uint8_t> mask, std::stop_token stop = {});

} // namespace rme::procedural
