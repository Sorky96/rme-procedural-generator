// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/generation_selection.h"
#include "procedural/adapter/terrain_validation.h"

#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <variant>

class Editor;

namespace rme::procedural {

enum class TerrainTargetError {
	WrongThread, // Not on the GUI thread; no GUI or editor state was read.
	Cancelled, // Stop requested before, between or after the checks.
	NoEditor, // No current map editor.
	WrongMap, // The current editor's map is not the map the result was prepared from.
	LiveSession, // The current editor is a live client or server (any connection state).
	BusySelection, // The current editor's selection is in a session or has deferred changes.
};

// Stage failures are forwarded unchanged so callers see the exact module cause.
using TargetValidationError = std::variant<TerrainTargetError, PreparedValidationError, SelectionMaskError>;

// Read-only, synchronous, GUI-thread checks that the CURRENT editor (looked up fresh through
// g_gui.GetCurrentEditor() on every call) is the offline target of a prepared result, followed by
// validatePreparedTerrain(). No editor, map, tab or wx handle is taken as input or stored.
//
// Identity is the existing snapshot map generation, so the guard distinguishes the currently active
// map, not the physical view: switching to another map refuses, while switching to another view of
// the same Editor, or back to the same unchanged map, may pass. Closing the last view and opening a
// new map cannot pass even if the allocator reuses addresses, and no old Editor pointer is read.
//
// The returned Editor* is borrowed and valid only for immediate synchronous continuation at this
// return; it is not a lifetime or commit lease. A future commit must call this directly before
// mutating, without yielding, and must still establish atomic apply/Undo, budgets and reentrancy
// itself. This does not make a commit atomic, does not observe a responsive GUI Cancel, and does not
// check a (not yet implemented) active Lua/history transaction. The asset, RSS and source exclusions
// of validatePreparedTerrain()/prepareTerrain() still apply.
//
// Rectangle request: currentMask is borrowed (empty = whole rectangle). The GUI selection membership
// is not inferred here; active/deferred selection sessions are still refused.
std::expected<Editor*, TargetValidationError> validateTerrainTarget(
	const PreparedTerrain& prepared, const GenerationConfig& currentConfig, const GroundProfile& currentProfile,
	std::span<const uint8_t> currentMask = {}, std::stop_token stop = {});

// Selection request: additionally revalidates exact current selection membership on the target
// editor (validateGenerationSelection) and requires selection.region == currentConfig.region, then
// validates the prepared result with selection.mask, which must equal the captured mask including
// protected bits.
std::expected<Editor*, TargetValidationError> validateSelectedTerrainTarget(
	const PreparedTerrain& prepared, const GenerationSelection& selection, const GenerationConfig& currentConfig,
	const GroundProfile& currentProfile, std::stop_token stop = {});

} // namespace rme::procedural
