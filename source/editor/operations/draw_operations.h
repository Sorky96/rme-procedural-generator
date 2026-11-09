#ifndef RME_EDITOR_OPERATIONS_DRAW_OPERATIONS_H
#define RME_EDITOR_OPERATIONS_DRAW_OPERATIONS_H

#include "app/rme_forward_declarations.h"
#include "map/position.h"
#include "procedural/adapter/terrain_preview.h"
#include "procedural/adapter/terrain_commit.h"
#include "procedural/adapter/preparation_steps.h"
// We don't want partial define of Editor, so forward declare or include?
// Ideally forward declare to avoid circular dependency if Editor includes this.
class Editor;

class DrawOperations {
public:
	// Copies the owned concrete diff unchanged; never rerolls or borders during commit/Undo/Redo.
	// Synchronous GUI-only, offline, freshly validates current map/assets/request/selection.
	static std::expected<rme::procedural::TerrainCommitReceipt, rme::procedural::TerrainCommitError> commitTerrainPreview(
		const rme::procedural::PreparedTerrain& prepared, const rme::procedural::GenerationConfig& config,
		const rme::procedural::GroundProfile& profile, rme::procedural::TerrainCommitBudget budget,
		std::span<const uint8_t> mask = {}, std::stop_token stop = {});
	static std::expected<rme::procedural::TerrainCommitReceipt, rme::procedural::TerrainCommitError> commitSelectedTerrainPreview(
		const rme::procedural::PreparedTerrain& prepared, const rme::procedural::GenerationSelection& selection,
		const rme::procedural::GenerationConfig& config, const rme::procedural::GroundProfile& profile,
		rme::procedural::TerrainCommitBudget budget, std::stop_token stop = {});
	static void draw(Editor& editor, Position offset, bool alt, bool dodraw);
	static void draw(Editor& editor, const PositionVector& tilestodraw, bool alt, bool dodraw);
	static void draw(Editor& editor, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool dodraw);
	static std::expected<rme::procedural::TerrainPreview, rme::procedural::PreviewError> prepareTerrainPreview(
		const Map& map, const rme::procedural::GenerationSnapshot& snapshot,
		const rme::procedural::GroundPlan& plan, rme::procedural::PreviewBudget budget, std::stop_token stop = {});
	// Snapshot and plan must outlive the continuation. The map is read on its first advance.
	static rme::procedural::PreparationSteps<std::expected<rme::procedural::TerrainPreview, rme::procedural::PreviewError>>
	prepareTerrainPreviewSteps(const Map& map, const rme::procedural::GenerationSnapshot& snapshot,
		const rme::procedural::GroundPlan& plan, rme::procedural::PreviewBudget budget, std::stop_token stop);
private:
	static std::expected<rme::procedural::TerrainCommitReceipt, rme::procedural::TerrainCommitError> commitTerrainUnits(
		Editor& editor, const rme::procedural::PreparedTerrain& prepared,
		rme::procedural::TerrainCommitBudget budget, std::stop_token stop);
	static std::expected<rme::procedural::TerrainCommitReceipt, rme::procedural::TerrainCommitError> commitValidatedTerrain(
		Editor& editor, const rme::procedural::PreparedTerrain& prepared,
		rme::procedural::TerrainCommitBudget budget, std::stop_token stop,
		const rme::procedural::PreparedTerrain* retained = nullptr);
};

#endif
