// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/terrain_target.h"

#include "editor/editor.h"
#include "editor/selection.h"
#include "live/live_manager.h"
#include "map/map.h"
#include "ui/gui.h"

#include <wx/thread.h>

namespace rme::procedural {
namespace {

std::unexpected<TargetValidationError> fail(TargetValidationError error) {
	return std::unexpected<TargetValidationError>(error);
}

// Shared target preflight for both request kinds. Thread first, before any GUI/global access; the
// editor is looked up fresh and only its current map generation is compared with the prepared data.
std::expected<Editor*, TargetValidationError> currentTarget(const PreparedTerrain& prepared, const std::stop_token& stop) {
	if (!wxIsMainThread()) return fail(TerrainTargetError::WrongThread);
	if (stop.stop_requested()) return fail(TerrainTargetError::Cancelled);
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) return fail(TerrainTargetError::NoEditor);
	if (editor->map.getGeneration() != prepared.snapshot.input.mapGeneration) return fail(TerrainTargetError::WrongMap);
	if (editor->live_manager.IsLive()) return fail(TerrainTargetError::LiveSession);
	if (editor->selection.isBusy() || editor->selection.isDeferred()) return fail(TerrainTargetError::BusySelection);
	if (stop.stop_requested()) return fail(TerrainTargetError::Cancelled);
	return editor;
}

} // namespace

std::expected<Editor*, TargetValidationError> validateTerrainTarget(
	const PreparedTerrain& prepared, const GenerationConfig& currentConfig, const GroundProfile& currentProfile,
	std::span<const uint8_t> currentMask, std::stop_token stop) {
	const auto target = currentTarget(prepared, stop);
	if (!target) return target;
	Editor* editor = *target;

	if (const auto valid = validatePreparedTerrain(editor->map, prepared, currentConfig, currentProfile, currentMask, stop); !valid) {
		return fail(valid.error());
	}
	if (stop.stop_requested()) return fail(TerrainTargetError::Cancelled);
	return editor;
}

std::expected<Editor*, TargetValidationError> validateSelectedTerrainTarget(
	const PreparedTerrain& prepared, const GenerationSelection& selection, const GenerationConfig& currentConfig,
	const GroundProfile& currentProfile, std::stop_token stop) {
	const auto target = currentTarget(prepared, stop);
	if (!target) return target;
	Editor* editor = *target;

	// Exact current membership on this editor's map (it also checks the selection's map generation).
	if (const auto members = validateGenerationSelection(*editor, selection, stop); !members) {
		return fail(members.error());
	}
	// A coherent mask moved to another region must not validate.
	if (!(selection.region == currentConfig.region)) return fail(SelectionMaskError::StaleSelection);

	if (const auto valid = validatePreparedTerrain(editor->map, prepared, currentConfig, currentProfile, selection.mask, stop); !valid) {
		return fail(valid.error());
	}
	if (stop.stop_requested()) return fail(TerrainTargetError::Cancelled);
	return editor;
}

} // namespace rme::procedural
