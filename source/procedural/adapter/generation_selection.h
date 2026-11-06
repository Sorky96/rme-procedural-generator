// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/generation.h"

#include <cstdint>
#include <expected>
#include <stop_token>
#include <vector>

class Editor;

namespace rme::procedural {

// Zero fields reject every selection.
struct SelectionMaskBudget {
	uint64_t maxCells = 0; // Cells of the inclusive bounding rectangle.
	uint64_t maxBytes = 0; // sizeof(GenerationSelection) + one allocation allowance + rectangle bytes.
};

// Exact membership of an editor selection on one floor: row-major mask over the inclusive bounding
// rectangle, 1 for every selected tile entry (even an empty tile with no selected item) and 0 for
// holes. Owns its data and holds no pointers into the editor or map.
struct GenerationSelection {
	GenerationRegion region;
	uint64_t mapGeneration = 0;
	uint64_t selectedCells = 0;
	uint64_t estimatedBytes = 0;
	std::vector<uint8_t> mask;
};

enum class SelectionMaskError {
	WrongThread, Busy, Empty, InvalidSelection, MultipleFloors, CellLimit,
	MemoryLimit, Cancelled, AllocationFailed, StaleSelection
};

// GUI thread only, synchronous, no yield. Reads Selection::getTiles() (not item selection flags or
// cached bounds) and never mutates the map, selection, assets or RNG. The caller guarantees the
// editor is alive; Editor is non-const only because Selection::isBusy() is. Allocates exactly one
// owned vector, after the cell and byte limits have been checked.
std::expected<GenerationSelection, SelectionMaskError> captureGenerationSelection(
	Editor& editor, SelectionMaskBudget budget, std::stop_token stop = {});

// GUI thread only and allocation-free. Succeeds when the editor's current selection membership is
// exactly the captured mask on the same map instance (map generation), and the stored region is
// exactly the tight inclusive bounding rectangle of the live members. It checks membership only:
// a selection that changed and returned to the same set validates again, pointer identity across
// Undo is not required, and item selection flags, tile contents, map revision and asset changes are
// not covered (the source snapshot must check those). It grants no commit authority.
std::expected<void, SelectionMaskError> validateGenerationSelection(
	Editor& editor, const GenerationSelection& selection, std::stop_token stop = {});

} // namespace rme::procedural
