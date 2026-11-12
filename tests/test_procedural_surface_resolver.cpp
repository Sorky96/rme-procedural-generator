// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only wxModule linked with the current RME objects, never into normal RME.
// Resolves surface v2 against the loaded 1098-BlackTek client and runs the existing
// GroundBorderCalculator over the result. It adds no border logic of its own.
#include "app/main.h"
#include "procedural_probe_warnings.h"
#include "procedural/adapter/generation_input.h"
#include "procedural/core/reachability.h"
#include "procedural/adapter/surface_resolver.h"
#include "app/managers/version_manager.h"
#include "app/settings.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/operations/draw_operations.h"
#include "procedural/adapter/terrain_preparation.h"
#include "procedural/adapter/terrain_validation.h"
#include "procedural/adapter/style_profile_store.h"
#include "procedural/core/style_profile.h"
#include "procedural/adapter/snapshot_equal.h"
#include <format>
#include "brushes/doodad/doodad_brush.h"
#include "brushes/ground/ground_border_calculator.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/terrain_placement.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/map.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "ui/dialogs/terrain_generator_dialog.h"
#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/timer.h>
#include <wx/module.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace rme::procedural;

void check(bool condition, const std::string& message) {
	if (!condition) throw std::runtime_error(message);
}

const GroundProfile profile {{"grass", "dirt", "rocky ground", "sand", "sea", "sea"}, GroundProfile::surfaceCount};
constexpr CoreBudget coreBudget {250000, 1024 * 1024};
constexpr SurfaceBudget surfaceBudget {250000, 4 * 1024 * 1024, 64 * 1024 * 1024};
constexpr uint64_t inputBudget = 4 * 1024 * 1024, planBudget = 4 * 1024 * 1024;

const GroundBrush* brushOf(uint16_t id) {
	const auto definition = g_item_definitions.get(id);
	if (!definition) return nullptr;
	const auto baseId = definition.attribute(ItemAttributeKey::BorderBaseGroundId);
	const auto ground = definition.isGroundTile() ? definition :
		(baseId > 0 && baseId <= UINT16_MAX ? g_item_definitions.get(uint16_t(baseId)) : ItemDefinitionView{});
	return ground ? static_cast<const GroundBrush*>(ground.editorData().brush) : nullptr;
}

// Stable text image of the tile stacks in a rectangle, for equality checks.
std::vector<uint32_t> stackImage(Map& map, int x0, int y0, int z, int width, int height) {
	std::vector<uint32_t> image;
	for (int y = y0; y < y0 + height; ++y) for (int x = x0; x < x0 + width; ++x) {
		const auto* tile = map.getTile(Position(x, y, z));
		if (!tile) { image.push_back(0xFFFFFFFFu); continue; }
		image.push_back(tile->ground ? tile->ground->getID() : 0);
		for (const auto& item : tile->items) image.push_back(item->getID());
		image.push_back(0xFFFFFFFEu);
	}
	return image;
}

struct BorderStats {
	size_t tiles = 0, withBorder = 0, multiBorder = 0, maxStack = 0;
	size_t adjacentPairs = 0, pairsWithoutBorder = 0, haloModified = 0;
	std::map<std::string, size_t> missingByPair;
};

void oneRun(uint32_t side, uint64_t seed, std::ostream& report, bool withOutsideRing) {
	Map map;
	map.initializeEmpty();
	const GenerationConfig config {{100, 100, 7, side, side}, seed};
	const GroundBrush* grass = g_brushes.getBrush("grass")->as<GroundBrush>();
	// Ring of foreign terrain around the area: borders must not alter tiles beyond the halo.
	const int x0 = config.region.x, y0 = config.region.y, z = config.region.z;
	std::vector<uint32_t> ringBefore;
	if (withOutsideRing) {
		for (int y = y0 - 3; y < y0 + int(side) + 3; ++y) for (int x = x0 - 3; x < x0 + int(side) + 3; ++x) {
			if (x >= x0 && y >= y0 && x < x0 + int(side) && y < y0 + int(side)) continue;
			TerrainPlacement::placeBrushItem(*map.getOrCreateTile(Position(x, y, z)), grass->getBorderItems().front().id);
		}
		ringBefore = stackImage(map, x0 - 3, y0 - 3, z, int(side) + 6, int(side) + 6);
	}
	const auto input = captureGenerationInput(map, config, coreBudget, inputBudget);
	check(bool(input), "Capture failed");
	const SurfaceConfig surface {config.region, seed};
	const auto grid = generateSurface(surface, input->extent, surfaceBudget, input->mask, input->protection);
	check(bool(grid) && grid->valid(), "Surface generation failed");

	const auto started = std::chrono::steady_clock::now();
	const auto plan = resolveSurfacePlan(*input, *grid, profile, planBudget);
	const auto resolveMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
	check(bool(plan), "Surface resolution failed, error " + std::to_string(plan ? 0 : int(plan.error())));
	const auto again = resolveSurfacePlan(*input, *grid, profile, planBudget);
	check(again && plan->items == again->items && plan->fingerprint() == again->fingerprint(), "Plan not deterministic");
	check(plan->client == "1098-BlackTek" && plan->config.seed == seed && plan->assetRevision == g_version.GetAssetRevision(),
		"Plan provenance missing");
	for (size_t i = 0; i < plan->items.size(); ++i) {
		check(plan->items[i] != 0, "Plan skipped a writable cell");
		const auto* expected = g_brushes.getBrush(profile.brushes[size_t(grid->cells[i]) - 1])->as<GroundBrush>();
		check(brushOf(plan->items[i]) == expected, "Plan chose an item outside the mapped brush");
	}
	// A mask must not change the variant of any other cell.
	std::vector<uint8_t> mask(grid->cells.size(), 1);
	for (size_t i = 0; i < mask.size(); i += 11) mask[i] = 0;
	const auto maskedInput = captureGenerationInput(map, config, coreBudget, inputBudget, mask);
	check(bool(maskedInput), "Masked capture failed");
	const auto maskedGrid = generateSurface(surface, maskedInput->extent, surfaceBudget, maskedInput->mask, maskedInput->protection);
	const auto maskedPlan = maskedGrid ? resolveSurfacePlan(*maskedInput, *maskedGrid, profile, planBudget)
		: std::unexpected(ResolveError::InvalidInput);
	check(bool(maskedPlan), "Masked resolution failed");
	for (size_t i = 0; i < mask.size(); ++i) {
		check(maskedPlan->items[i] == (mask[i] ? plan->items[i] : 0), "Mask changed another cell's variant");
	}

	// Place the plan on a copy of the situation and run the existing border calculator.
	for (size_t i = 0; i < plan->items.size(); ++i) {
		const int x = x0 + int(i % side), y = y0 + int(i / side);
		TerrainPlacement::placeBrushItem(*map.getOrCreateTile(Position(x, y, z)), plan->items[i]);
	}
	std::vector<Tile*> affected;
	for (int y = y0 - 1; y <= y0 + int(side); ++y) for (int x = x0 - 1; x <= x0 + int(side); ++x) {
		if (auto* tile = map.getTile(Position(x, y, z))) affected.push_back(tile);
	}
	const auto borderStart = std::chrono::steady_clock::now();
	for (auto* tile : affected) GroundBorderCalculator::calculate(&map, tile, true);
	const auto borderMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - borderStart).count();
	const auto afterFirst = stackImage(map, x0 - 3, y0 - 3, z, int(side) + 6, int(side) + 6);
	for (auto* tile : affected) GroundBorderCalculator::calculate(&map, tile, true);
	check(afterFirst == stackImage(map, x0 - 3, y0 - 3, z, int(side) + 6, int(side) + 6), "Second border pass changed the stacks");

	BorderStats stats;
	const auto borderCount = [&](const Tile* tile) {
		size_t count = 0;
		for (const auto& item : tile->items) count += item->isBorder();
		return count;
	};
	for (size_t i = 0; i < plan->items.size(); ++i) {
		const int x = x0 + int(i % side), y = y0 + int(i / side);
		const auto* tile = map.getTile(Position(x, y, z));
		check(tile && tile->ground && tile->ground->getID() == plan->items[i], "Borders replaced or lost the placed ground");
		const size_t borders = borderCount(tile);
		++stats.tiles;
		stats.withBorder += borders > 0;
		stats.multiBorder += borders > 1;
		stats.maxStack = std::max(stats.maxStack, tile->items.size());
		for (const auto& item : tile->items) check(item->isBorder() || !item->isGroundTile(), "Unexpected non-border item added by border pass");
		for (const auto [dx, dy] : {std::pair{1, 0}, std::pair{0, 1}}) {
			const int nx = x + dx, ny = y + dy;
			if (nx >= x0 + int(side) || ny >= y0 + int(side)) continue;
			const auto* neighbour = map.getTile(Position(nx, ny, z));
			const auto* a = brushOf(tile->ground->getID());
			const auto* b = brushOf(neighbour->ground->getID());
			if (a == b) continue;
			++stats.adjacentPairs;
			if (borders == 0 && borderCount(neighbour) == 0) {
				++stats.pairsWithoutBorder;
				++stats.missingByPair[a->getName() + "|" + b->getName()];
			}
		}
	}
	if (withOutsideRing) {
		const auto ringAfter = stackImage(map, x0 - 3, y0 - 3, z, int(side) + 6, int(side) + 6);
		// Tiles beyond the one-tile halo (distance 2 and 3 from the area) must be byte-identical.
		for (int y = y0 - 3; y < y0 + int(side) + 3; ++y) for (int x = x0 - 3; x < x0 + int(side) + 3; ++x) {
			const bool inside = x >= x0 && y >= y0 && x < x0 + int(side) && y < y0 + int(side);
			const bool halo = !inside && x >= x0 - 1 && y >= y0 - 1 && x <= x0 + int(side) && y <= y0 + int(side);
			if (inside || halo) continue;
			const auto* tile = map.getTile(Position(x, y, z));
			check(tile && tile->items.empty(), "Border pass reached beyond the one-tile halo");
		}
		for (int y = y0 - 1; y <= y0 + int(side); ++y) for (int x = x0 - 1; x <= x0 + int(side); ++x) {
			const bool inside = x >= x0 && y >= y0 && x < x0 + int(side) && y < y0 + int(side);
			if (!inside && !map.getTile(Position(x, y, z))->items.empty()) ++stats.haloModified;
		}
		(void)ringBefore; (void)ringAfter;
	}
	report << "SURFACE_RESOLVER_SIZE_PASS\t" << side << '\t' << seed << '\t' << plan->items.size() << "\tresolve_ms=" << resolveMs
		<< "\tborder_ms=" << borderMs << "\tfingerprint=" << plan->fingerprint() << '\n';
	report << "SURFACE_BORDER_STATS\t" << side << '\t' << seed << "\ttiles=" << stats.tiles << "\twith_border=" << stats.withBorder
		<< "\tmulti_border=" << stats.multiBorder << "\tmax_stack=" << stats.maxStack << "\tbrush_pairs=" << stats.adjacentPairs
		<< "\tpairs_without_border=" << stats.pairsWithoutBorder << "\thalo_tiles_modified=" << stats.haloModified << '\n';
	for (const auto& [pair, count] : stats.missingByPair) report << "SURFACE_PAIR_WITHOUT_BORDER\t" << side << '\t' << seed << '\t' << pair << '\t' << count << '\n';
	check(stats.multiBorder > 0, "No tile received more than one border item");
}

// Whole pipeline for the surface algorithm: prepare, validate, commit as one history step, Undo, Redo.
// The default Undo memory limit is 40 MiB, which refuses large areas by design (the dialog says so).
// The G1 probes raise it to 512 MiB in the private profile; do the same here and restore it.
struct UndoLimits {
	int count = g_settings.getInteger(Config::UNDO_SIZE), bytes = g_settings.getInteger(Config::UNDO_MEM_SIZE);
	UndoLimits() { g_settings.setInteger(Config::UNDO_SIZE, 1000); g_settings.setInteger(Config::UNDO_MEM_SIZE, 512); }
	~UndoLimits() { g_settings.setInteger(Config::UNDO_SIZE, count); g_settings.setInteger(Config::UNDO_MEM_SIZE, bytes); }
};

void endToEnd(uint32_t side, std::ostream& report, bool vegetation = false) {
	UndoLimits undoLimits;
	check(g_gui.NewMap(), "Editor creation failed");
	auto& editor = *g_gui.GetCurrentEditor();
	GenerationConfig config {{100, 100, 7, side, side}, 1729};
	config.algorithm = GenerationAlgorithm::Surface;
	config.vegetation = vegetation;
	GroundProfile profile = ::profile;
	if (vegetation) {
		profile.decor = {"trees", "bushes", "small rocks"};
		profile.decorCount = GroundProfile::decorationCount;
	}
	constexpr PreparationBudget preparation {250000, 254016, 250000, 512ULL * 1024 * 1024};
	constexpr TerrainCommitBudget commit {250000, 768ULL * 1024 * 1024};
	const auto baselineTiles = editor.map.getTileCount();
	const auto prior = editor.actionQueue->getCurrentIndex();
	auto prepared = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(prepared), "Surface preparation failed, error index " + std::to_string(prepared ? 0 : prepared.error().index()));
	if (side == 100) {
		const auto staged = [&]() {
			auto steps = prepareTerrainSteps(editor.map, config, profile, preparation, {});
			check(bool(steps), "Staged surface preflight failed");
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
			for (;;) {
				if (auto result = steps->advance()) return std::move(*result);
				check(std::chrono::steady_clock::now() < deadline, "Staged surface timed out");
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		};
		auto async = staged();
		check(async && async->plan.items == prepared->plan.items && async->plan.decor == prepared->plan.decor &&
			async->preview.getFingerprint() == prepared->preview.getFingerprint(), "Worker changed deterministic surface/decor payload");
		{
			std::stop_source stop;
			auto steps = prepareTerrainSteps(editor.map, config, profile, preparation, {}, stop.get_token());
			check(steps && !steps->advance() && !steps->advance(), "Staged worker did not suspend");
			stop.request_stop();
			auto cancelled = steps->advance();
			check(cancelled && !*cancelled && std::holds_alternative<PreparationInputError>(cancelled->error()) &&
				std::get<PreparationInputError>(cancelled->error()) == PreparationInputError::Cancelled,
				"Staged worker published output after cancellation");
		}
		{
			auto dropped = prepareTerrainSteps(editor.map, config, profile, preparation, {});
			check(dropped && !dropped->advance() && !dropped->advance(), "Dropped worker did not suspend");
		} // The dialog's Cancel/close path destroys the continuation without advancing it.
		auto retry = staged();
		check(retry && retry->preview.getFingerprint() == prepared->preview.getFingerprint() &&
			editor.map.getTileCount() == baselineTiles && editor.actionQueue->getCurrentIndex() == prior,
			"Cancellation/destruction changed source, history or deterministic retry");
		report << "SURFACE_WORKER_CANCEL_RETRY_PASS\tvegetation=" << vegetation << '\n';
	}
	check(bool(validatePreparedTerrain(editor.map, *prepared, config, profile)), "Surface validation failed");
	auto tuned = config;
	tuned.surface.riverCount += 1;
	check(!validatePreparedTerrain(editor.map, *prepared, tuned, profile), "Changed tuning accepted as current");
	auto classic = config;
	classic.algorithm = GenerationAlgorithm::GroundRoll;
	classic.vegetation = false;
	check(!validatePreparedTerrain(editor.map, *prepared, classic, profile), "Changed algorithm accepted as current");
	const GroundProfile threeNames {{"grass", "dirt", "sea"}};
	check(!prepareTerrain(editor.map, config, threeNames, preparation), "Surface accepted a three-brush profile");
	check(!prepareTerrain(editor.map, classic, profile, preparation), "Classic accepted a six-brush profile");
	if (vegetation) {
		auto withoutNames = profile;
		withoutNames.decorCount = 0;
		check(!prepareTerrain(editor.map, config, withoutNames, preparation), "Vegetation accepted without decoration brushes");
		auto classicVegetation = classic;
		classicVegetation.vegetation = true;
		check(!prepareTerrain(editor.map, classicVegetation, profile, preparation), "Classic accepted vegetation");
		auto badBrush = profile;
		badBrush.decor[0] = "grass";
		check(!prepareTerrain(editor.map, config, badBrush, preparation), "A ground brush was accepted as a doodad brush");
		auto changed = config;
		changed.vegetation = false;
		check(!validatePreparedTerrain(editor.map, *prepared, changed, profile), "Vegetation toggle accepted as current");
		check(prepared->plan.decor.size() == prepared->plan.items.size(), "Vegetation plan has no decoration layer");
	} else {
		check(prepared->plan.decor.empty(), "Plan without vegetation carries a decoration layer");
	}
	const auto expected = prepared->plan.items;
	const auto expectedDecor = prepared->plan.decor;
	const auto receipt = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
	check(bool(receipt), "Surface commit failed, error index " + std::to_string(receipt ? 0 : receipt.error().index()) +
		(receipt || receipt.error().index() != 0 ? std::string() : " value " + std::to_string(int(std::get<TerrainCommitFailure>(receipt.error())))));
	check(editor.actionQueue->getCurrentIndex() == prior + 1 && editor.actionQueue->getSize() == prior + 1, "Commit is not one history step");
	const auto checkPlaced = [&](bool placed) {
		for (size_t i = 0; i < expected.size(); ++i) {
			const auto* tile = editor.map.getTile(Position(100 + int(i % side), 100 + int(i / side), 7));
			if (placed) {
				check(tile && tile->ground && tile->ground->getID() == expected[i], "Committed ground differs from the plan");
				size_t plain = 0;
				for (const auto& item : tile->items) {
					if (item->isBorder()) continue;
					++plain;
					check(!expectedDecor.empty() && item->getID() == expectedDecor[i], "Committed tile holds an item outside the plan");
				}
				check(plain == ((!expectedDecor.empty() && expectedDecor[i]) ? 1u : 0u), "Decoration count differs from the plan");
			} else {
				check(!tile || (!tile->ground && tile->items.empty()), "Undo left generated content");
			}
		}
	};
	checkPlaced(true);
	const auto committedTiles = editor.map.getTileCount();
	check(editor.actionQueue->undo(), "Surface Undo refused");
	checkPlaced(false);
	check(editor.map.getTileCount() == baselineTiles, "Undo changed the tile count");
	check(editor.actionQueue->redo(), "Surface Redo refused");
	checkPlaced(true);
	check(editor.map.getTileCount() == committedTiles, "Redo changed the tile count");
	// A modified synthetic map would make the final Close() ask to save it and wait for an operator.
	editor.map.clearChanges();
	report << (vegetation ? "SURFACE_VEGETATION_E2E_PASS\t" : "SURFACE_E2E_PASS\t") << side << "\tdecorated="
		<< std::ranges::count_if(expectedDecor, [](uint16_t id) { return id != 0; }) << "\tchanges=" << receipt->changes << "\tbytes=" << receipt->totalBytes << '\n';
}

// Paths, building foundations and their walls through the real client: a profile with eight brushes plus a wall brush
// is prepared, validated, committed, undone and redone; every planned wall ends up as a wall item on its tile and
// nothing else is left behind after Undo. Profiles that do not match the tuning are refused.
void structures(uint32_t side, std::ostream& report) {
	UndoLimits undoLimits;
	check(g_gui.NewMap(), "Editor creation failed");
	auto& editor = *g_gui.GetCurrentEditor();
	GenerationConfig config {{100, 100, 7, side, side}, 1729};
	config.algorithm = GenerationAlgorithm::Surface;
	config.surface.pathLength = 80;
	config.surface.buildingDensity = 40;
	GroundProfile profile {{"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount};
	profile.wall = "stone wall";
	constexpr PreparationBudget preparation {250000, 254016, 250000, 512ULL * 1024 * 1024};
	constexpr TerrainCommitBudget commit {250000, 768ULL * 1024 * 1024};
	const auto baselineTiles = editor.map.getTileCount();
	const auto prior = editor.actionQueue->getCurrentIndex();
	auto prepared = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(prepared), "Structure preparation failed, error index " + std::to_string(prepared ? 0 : prepared.error().index()) +
		(prepared || prepared.error().index() != 0 ? std::string() : " value " + std::to_string(int(std::get<PreparationInputError>(prepared.error())))));
	check(bool(validatePreparedTerrain(editor.map, *prepared, config, profile)), "Structure validation failed");
	check(prepared->plan.walls.size() == prepared->plan.items.size(), "The plan has no wall layer");
	const size_t plannedWalls = size_t(std::ranges::count_if(prepared->plan.walls, [](uint16_t id) { return id != 0; }));
	check(plannedWalls > 0, "No wall was planned");
	auto sixBrushes = profile;
	sixBrushes.count = GroundProfile::surfaceCount;
	sixBrushes.wall.clear();
	check(!prepareTerrain(editor.map, config, sixBrushes, preparation), "Buildings accepted a six-brush profile");
	auto noBuildings = config;
	noBuildings.surface.buildingDensity = 0;
	check(!prepareTerrain(editor.map, noBuildings, profile, preparation), "Walls accepted without buildings");
	auto badWall = profile;
	badWall.wall = "grass";
	check(!prepareTerrain(editor.map, config, badWall, preparation), "A ground brush was accepted as a wall brush");
	auto sevenBrushes = profile;
	sevenBrushes.count = GroundProfile::surfaceWithPathCount;
	sevenBrushes.wall.clear();
	check(!prepareTerrain(editor.map, config, sevenBrushes, preparation), "Buildings accepted a seven-brush profile");
	const auto expected = prepared->plan.items;
	const auto expectedWalls = prepared->plan.walls;
	const auto receipt = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
	check(bool(receipt), "Structure commit failed, error index " + std::to_string(receipt ? 0 : receipt.error().index()));
	check(editor.actionQueue->getCurrentIndex() == prior + 1 && editor.actionQueue->getSize() == prior + 1, "Commit is not one history step");
	const auto checkPlaced = [&](bool placed) {
		size_t walls = 0;
		for (size_t i = 0; i < expected.size(); ++i) {
			const auto* tile = editor.map.getTile(Position(100 + int(i % side), 100 + int(i / side), 7));
			if (placed) {
				check(tile && tile->ground && tile->ground->getID() == expected[i], "Committed ground differs from the plan");
				size_t plain = 0;
				for (const auto& item : tile->items) {
					if (item->isBorder()) continue;
					++plain;
					check(item->isWall(), "Committed tile holds a plain item that is not a wall");
				}
				check(plain == (expectedWalls[i] ? 1u : 0u), "Wall count differs from the plan");
				walls += plain;
			} else {
				check(!tile || (!tile->ground && tile->items.empty()), "Undo left generated content");
			}
		}
		check(!placed || walls == plannedWalls, "The number of walls differs from the plan");
	};
	checkPlaced(true);
	const auto committedTiles = editor.map.getTileCount();
	check(editor.actionQueue->undo(), "Structure Undo refused");
	checkPlaced(false);
	check(editor.map.getTileCount() == baselineTiles, "Undo changed the tile count");
	check(editor.actionQueue->redo(), "Structure Redo refused");
	checkPlaced(true);
	check(editor.map.getTileCount() == committedTiles, "Redo changed the tile count");
	editor.map.clearChanges();
	report << "SURFACE_STRUCTURES_E2E_PASS" << '\t' << side << '\t' << "walls=" << plannedWalls << '\t' << "changes=" << receipt->changes << '\n';
}

// Doors on top of the walls (G4 iteration 12e): one door item per planned door after Commit, none after Undo, again after
// Redo; a door kind without a wall brush or an unknown kind is refused, and a plan without doors keeps its fingerprint.
void doors(uint32_t side, std::ostream& report) {
	UndoLimits undoLimits;
	check(g_gui.NewMap(), "Editor creation failed");
	auto& editor = *g_gui.GetCurrentEditor();
	GenerationConfig config {{100, 100, 7, side, side}, 1729};
	config.algorithm = GenerationAlgorithm::Surface;
	config.surface.pathLength = 80;
	config.surface.buildingDensity = 40;
	GroundProfile profile {{"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount};
	profile.wall = "stone wall";
	constexpr PreparationBudget preparation {250000, 254016, 250000, 512ULL * 1024 * 1024};
	constexpr TerrainCommitBudget commit {250000, 768ULL * 1024 * 1024};
	auto walled = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(walled), "Walled preparation failed");
	profile.door = "Normal door";
	const auto baselineTiles = editor.map.getTileCount();
	const auto prior = editor.actionQueue->getCurrentIndex();
	auto prepared = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(prepared), "Door preparation failed, error index " + std::to_string(prepared ? 0 : prepared.error().index()) +
		(prepared || prepared.error().index() != 4 ? std::string() : " preview error " + std::to_string(int(std::get<PreviewError>(prepared.error())))));
	check(bool(validatePreparedTerrain(editor.map, *prepared, config, profile)), "Door validation failed");
	check(prepared->plan.doors.size() == prepared->plan.items.size(), "The plan has no door layer");
	const size_t plannedDoors = size_t(std::ranges::count_if(prepared->plan.doors, [](uint8_t flag) { return flag != 0; }));
	check(plannedDoors > 0, "No door was planned");
	check(walled && prepared->plan.fingerprint() != walled->plan.fingerprint(), "Doors did not change the plan fingerprint");
	check(walled && prepared->plan.walls == walled->plan.walls && prepared->plan.items == walled->plan.items, "Doors changed ground or walls of the plan");
	auto noWall = profile;
	noWall.wall.clear();
	check(!prepareTerrain(editor.map, config, noWall, preparation), "A door was accepted without a wall brush");
	auto badKind = profile;
	badKind.door = "Trapdoor of doom";
	check(!prepareTerrain(editor.map, config, badKind, preparation), "An unknown door kind was accepted");
	const auto expectedDoors = prepared->plan.doors;
	const auto receipt = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
	check(bool(receipt), "Door commit failed, error index " + std::to_string(receipt ? 0 : receipt.error().index()));
	check(editor.actionQueue->getCurrentIndex() == prior + 1 && editor.actionQueue->getSize() == prior + 1, "Commit is not one history step");
	const auto countDoors = [&](bool placed) {
		size_t found = 0;
		for (size_t i = 0; i < expectedDoors.size(); ++i) {
			const auto* tile = editor.map.getTile(Position(100 + int(i % side), 100 + int(i / side), 7));
			size_t here = 0;
			if (tile) for (const auto& item : tile->items) here += item->isBrushDoor();
			check(here == (placed && expectedDoors[i] ? 1u : 0u), "Door count on a tile differs from the plan");
			found += here;
		}
		return found;
	};
	check(countDoors(true) == plannedDoors, "The number of doors differs from the plan");
	const auto committedTiles = editor.map.getTileCount();
	check(editor.actionQueue->undo(), "Door Undo refused");
	check(countDoors(false) == 0 && editor.map.getTileCount() == baselineTiles, "Undo left doors or changed the tile count");
	check(editor.actionQueue->redo(), "Door Redo refused");
	check(countDoors(true) == plannedDoors && editor.map.getTileCount() == committedTiles, "Redo did not restore the doors");
	editor.map.clearChanges();
	report << "SURFACE_DOORS_E2E_PASS" << '\t' << side << '\t' << "doors=" << plannedDoors << '\t' << "changes=" << receipt->changes << '\n';
}

// Fords change water to Shore; I40 also preserves original terrain on islands
// they connect. Preview, Commit, Undo and Redo work with the resulting plan.
void fords(uint32_t side, uint64_t seed, std::ostream& report) {
	UndoLimits undoLimits;
	check(g_gui.NewMap(), "Editor creation failed");
	auto& editor = *g_gui.GetCurrentEditor();
	GenerationConfig config {{100, 100, 7, side, side}, seed};
	config.algorithm = GenerationAlgorithm::Surface;
	config.surface.riverCount = 8;
	config.surface.lakePermille = 450;
	GroundProfile profile {{"grass", "dirt", "rocky ground", "sand", "sea", "sea"}, GroundProfile::surfaceCount};
	constexpr PreparationBudget preparation {250000, 254016, 250000, 512ULL * 1024 * 1024};
	constexpr TerrainCommitBudget commit {250000, 768ULL * 1024 * 1024};
	auto plain = config;
	plain.surface.fordLength = 0;
	config.surface.fordLength = 256;
	// The first seed from the given one on that needs a ford (the layout of rivers and lakes decides).
	std::optional<SurfaceGrid> withFords, without;
	size_t changed = 0;
	for (uint64_t offset = 0; offset < 40 && !changed; ++offset) {
		config.seed = plain.seed = seed + offset;
		auto a = generateSurface({config.region, config.seed, config.surface}, MapExtent {65001, 65001, 16}, surfaceBudget);
		auto b = generateSurface({plain.region, plain.seed, plain.surface}, MapExtent {65001, 65001, 16}, surfaceBudget);
		check(bool(a) && bool(b), "Ford surface generation failed");
		if (!a || !b) return;
		const auto aIslands = sandIslandsOf(*a), bIslands = sandIslandsOf(*b);
		check(bool(aIslands) && bool(bIslands), "Ford island classification failed");
		if (!aIslands || !bIslands) return;
		std::vector<uint8_t> aIsland(a->cells.size()), bIsland(b->cells.size());
		for (const auto i : aIslands->cells) aIsland[i] = 1;
		for (const auto i : bIslands->cells) bIsland[i] = 1;
		for (size_t i = 0; i < a->cells.size(); ++i) {
			if (a->cells[i] == b->cells[i]) continue;
			const bool crossing = a->cells[i] == SurfaceKind::Shore && (b->cells[i] == SurfaceKind::River || b->cells[i] == SurfaceKind::Lake);
			changed += crossing;
			const auto naturalLand = [](SurfaceKind k) { return k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Rock; };
			check(crossing || (bIsland[i] && b->cells[i] == SurfaceKind::Shore && naturalLand(a->cells[i])) ||
				(aIsland[i] && a->cells[i] == SurfaceKind::Shore && naturalLand(b->cells[i])), "A ford changed terrain outside a crossing or normalized island");
		}
		withFords = std::move(*a);
		without = std::move(*b);
	}
	check(changed > 0, "No seed produced a ford");
	const auto baselineTiles = editor.map.getTileCount();
	const auto prior = editor.actionQueue->getCurrentIndex();
	auto plainPrepared = prepareTerrain(editor.map, plain, profile, preparation);
	auto prepared = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(plainPrepared), "Preparation without fords failed, error index " + std::to_string(plainPrepared ? 0 : plainPrepared.error().index()));
	check(bool(prepared), "Ford preparation failed, error index " + std::to_string(prepared ? 0 : prepared.error().index()) +
		(prepared || prepared.error().index() != 4 ? std::string() : " preview error " + std::to_string(int(std::get<PreviewError>(prepared.error())))));
	if (!plainPrepared || !prepared) return;
	check(bool(validatePreparedTerrain(editor.map, *prepared, config, profile)), "Ford validation failed");
	check(prepared->plan.fingerprint() != plainPrepared->plan.fingerprint(), "Fords did not change the plan fingerprint");
	const auto receipt = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
	check(bool(receipt), "Ford commit failed, error index " + std::to_string(receipt ? 0 : receipt.error().index()));
	check(editor.actionQueue->getCurrentIndex() == prior + 1 && editor.actionQueue->getSize() == prior + 1, "Ford commit is not one history step");
	if (withFords && without) {
		// Walkability on the committed tiles (not the model): ford tiles and every land tile beside one must not block, and
		// each pocket the fords joined to the main land must be joined on the real map too (4-neighbour moves).
		const size_t count = withFords->cells.size();
		const auto tileAt = [&](size_t i) { return editor.map.getTile(Position(100 + int(i % side), 100 + int(i / side), 7)); };
		const auto walkableAt = [&](size_t i) { const auto* tile = tileAt(i); return tile && tile->ground && !tile->isBlocking(); };
		size_t fordCells = 0, blockedFord = 0, blockedBeside = 0;
		std::string firstBlocked;
		const auto describe = [&](size_t i) {
			const auto* tile = tileAt(i);
			std::string text = "(" + std::to_string(i % side) + "," + std::to_string(i / side) + ") kind " + std::to_string(int(withFords->cells[i]));
			if (!tile || !tile->ground) return text + " no ground";
			text += " ground " + std::string(tile->ground->getName()) + (tile->ground->isBlocking() ? "(blocks)" : "");
			for (const auto& item : tile->items) text += ", " + std::string(item->getName()) + (item->isBlocking() ? "(blocks)" : "");
			return text;
		};
		for (size_t i = 0; i < count; ++i) {
			if (without->cells[i] != SurfaceKind::Lake && without->cells[i] != SurfaceKind::River) continue;
			if (withFords->cells[i] != SurfaceKind::Shore) continue;
			++fordCells;
			if (!walkableAt(i)) { ++blockedFord; if (firstBlocked.empty()) firstBlocked = describe(i); }
			for (const int step : {-1, 1, -int(side), int(side)}) {
				const size_t n = size_t(int64_t(i) + step);
				if (n >= count || withFords->cells[n] == without->cells[n]) continue;
				const SurfaceKind kind = withFords->cells[n];
				if (kind == SurfaceKind::Lake || kind == SurfaceKind::River) continue;
				if (!walkableAt(n)) { ++blockedBeside; if (firstBlocked.empty()) firstBlocked = "beside: " + describe(n); }
			}
		}
		// Kinds the validator calls walkable, counted on the committed tiles: how many block (D22).
		size_t rockTiles = 0, rockBlocking = 0, shoreTiles = 0, shoreBlocking = 0, grassTiles = 0, grassBlocking = 0;
		std::string rockSample;
		for (size_t i = 0; i < count; ++i) {
			const SurfaceKind kind = withFords->cells[i];
			const auto* tile = tileAt(i);
			if (!tile) continue;
			if (kind == SurfaceKind::Rock) {
				++rockTiles; rockBlocking += tile->isBlocking();
				if (rockSample.empty() && tile->ground) {
					rockSample = std::string(tile->ground->getName());
					for (const auto& item : tile->items) rockSample += ", " + std::string(item->getName()) + (item->isBlocking() ? "(blocks)" : "");
				}
			} else if (kind == SurfaceKind::Shore) { ++shoreTiles; shoreBlocking += tile->isBlocking(); }
			else if (kind == SurfaceKind::Grass) { ++grassTiles; grassBlocking += tile->isBlocking(); }
		}
		report << "KIND_BLOCKING\trock=" << rockTiles << "/" << rockBlocking << "\tshore=" << shoreTiles << "/" << shoreBlocking << "\tgrass=" << grassTiles << "/" << grassBlocking << "\t" << rockSample << '\n';
		// Edge between sand and grass on the committed tiles: do the two kinds get a border item (iteration 18 diagnosis)?
		size_t sandGrassPairs = 0, sandWithItems = 0, grassWithItems = 0;
		std::string sandItems, grassItems;
		for (size_t i = 0; i < count; ++i) {
			if (withFords->cells[i] != SurfaceKind::Shore) continue;
			for (const int step : {1, int(side)}) {
				const size_t n = i + size_t(step);
				if (n >= count || withFords->cells[n] != SurfaceKind::Grass) continue;
				const auto* sand = tileAt(i);
				const auto* grassTile = tileAt(n);
				if (!sand || !grassTile) continue;
				++sandGrassPairs;
				sandWithItems += !sand->items.empty();
				grassWithItems += !grassTile->items.empty();
				if (sandItems.empty() && !sand->items.empty()) for (const auto& item : sand->items) sandItems += std::string(item->getName()) + ";";
				if (grassItems.empty() && !grassTile->items.empty()) for (const auto& item : grassTile->items) grassItems += std::string(item->getName()) + ";";
			}
		}
		report << "SAND_GRASS_EDGE\tpairs=" << sandGrassPairs << "\tsandWithItems=" << sandWithItems << "\tgrassWithItems=" << grassWithItems << "\t" << sandItems << "\t" << grassItems << '\n';
		std::vector<uint32_t> real(count, 0);
		std::vector<size_t> queue;
		uint32_t realComponents = 0;
		for (size_t start = 0; start < count; ++start) {
			if (real[start] || !walkableAt(start)) continue;
			real[start] = ++realComponents;
			queue.assign(1, start);
			for (size_t head = 0; head < queue.size(); ++head) {
				const size_t at = queue[head];
				for (const int step : {-1, 1, -int(side), int(side)}) {
					if ((step == -1 && at % side == 0) || (step == 1 && at % side + 1 == side)) continue;
					const size_t n = size_t(int64_t(at) + step);
					if (n >= count || real[n] || !walkableAt(n)) continue;
					real[n] = realComponents;
					queue.push_back(n);
				}
			}
		}
		const auto before = walkableLabelsOf(without->cells, side, side, {}, false);
		const auto after = walkableLabelsOf(withFords->cells, side, side, {}, false);
		check(bool(before) && bool(after), "Walkable labels failed");
		size_t joined = 0, notJoinedOnMap = 0;
		if (before && after) {
			std::vector<size_t> first(before->sizes.size(), count);
			for (size_t i = count; i-- > 0;) if (before->label[i]) first[before->label[i]] = i;
			const size_t main = first[before->largest];
			for (uint32_t id = 1; id < before->sizes.size(); ++id) {
				if (id == before->largest || before->sizes[id] < reachabilityPocketCells || after->label[first[id]] != after->label[main]) continue;
				++joined;
				if (!real[first[id]] || real[first[id]] != real[main]) ++notJoinedOnMap;
			}
		}
		report << "FORD_TILES\t" << fordCells << "\tblocked=" << blockedFord << "\tblockedBeside=" << blockedBeside << "\tjoined=" << joined << "\tnotJoinedOnMap=" << notJoinedOnMap << "\t" << firstBlocked << '\n';
		check(blockedFord == 0 && blockedBeside == 0, "A ford tile or a land tile beside it blocks: " + firstBlocked);
		check(joined > 0 && notJoinedOnMap == 0, "A pocket joined by a ford is not joined on the committed map");
	}
	const auto committedTiles = editor.map.getTileCount();
	check(editor.actionQueue->undo(), "Ford Undo refused");
	check(editor.map.getTileCount() == baselineTiles, "Undo changed the tile count");
	check(editor.actionQueue->redo(), "Ford Redo refused");
	check(editor.map.getTileCount() == committedTiles, "Redo did not restore the tiles");
	editor.map.clearChanges();
	report << "SURFACE_FORDS_E2E_PASS" << '\t' << side << '\t' << "fordCells=" << changed << '\t' << "changes=" << (receipt ? receipt->changes : 0) << '\n';
}

// Ruins and camps (G5 iteration 25): the plan has a wall on every outline cell except the gaps; Commit puts no wall piece on a gap tile, Undo and
// Redo restore everything; the grid is valid and the plan fingerprint differs from one without ruins and camps.
void ruinsAndCamps(uint32_t side, std::ostream& report) {
	UndoLimits undoLimits;
	check(g_gui.NewMap(), "Editor creation failed");
	auto& editor = *g_gui.GetCurrentEditor();
	GenerationConfig config {{100, 100, 7, side, side}, 1729};
	config.algorithm = GenerationAlgorithm::Surface;
	config.surface.pathLength = 40;
	config.surface.buildingDensity = 40;
	GroundProfile profile {{"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount};
	profile.wall = "stone wall";
	constexpr PreparationBudget preparation {250000, 254016, 250000, 512ULL * 1024 * 1024};
	constexpr TerrainCommitBudget commit {250000, 768ULL * 1024 * 1024};
	auto plain = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(plain), "Preparation without ruins failed");
	config.surface.ruinPercent = 40;
	config.surface.campCount = 8;
	const auto baselineTiles = editor.map.getTileCount();
	const auto prior = editor.actionQueue->getCurrentIndex();
	auto prepared = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(prepared), "Ruin preparation failed, error index " + std::to_string(prepared ? 0 : prepared.error().index()));
	if (!plain || !prepared) return;
	check(bool(validatePreparedTerrain(editor.map, *prepared, config, profile)), "Ruin validation failed");
	check(prepared->plan.fingerprint() != plain->plan.fingerprint(), "Ruins did not change the plan fingerprint");
	const auto grid = generateSurface({config.region, config.seed, config.surface}, MapExtent {65001, 65001, 16}, surfaceBudget);
	check(bool(grid) && grid->valid() && !grid->gaps.empty() && !grid->sites.empty(), "The surface has no gaps or sites");
	if (!grid) return;
	size_t outline = 0;
	for (size_t i = 0; i < grid->cells.size(); ++i) {
		if (grid->cells[i] != SurfaceKind::Floor) continue;
		const size_t x = i % side, y = i / side;
		outline += x == 0 || y == 0 || x + 1 == side || y + 1 == side || grid->cells[i - 1] != SurfaceKind::Floor || grid->cells[i + 1] != SurfaceKind::Floor ||
			grid->cells[i - side] != SurfaceKind::Floor || grid->cells[i + side] != SurfaceKind::Floor;
	}
	const size_t plannedWalls = size_t(std::ranges::count_if(prepared->plan.walls, [](uint16_t item) { return item != 0; }));
	check(plannedWalls + grid->gaps.size() == outline, "Planned walls plus gaps do not add up to the outline cells");
	const auto receipt = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
	check(bool(receipt), "Ruin commit failed, error index " + std::to_string(receipt ? 0 : receipt.error().index()));
	check(editor.actionQueue->getCurrentIndex() == prior + 1 && editor.actionQueue->getSize() == prior + 1, "Ruin commit is not one history step");
	size_t wallsOnGaps = 0;
	for (const uint32_t gap : grid->gaps) {
		const auto* tile = editor.map.getTile(Position(100 + int(gap % side), 100 + int(gap / side), 7));
		if (tile) for (const auto& item : tile->items) wallsOnGaps += item->isWall();
	}
	check(wallsOnGaps == 0, "A wall piece stands on a gap tile");
	const auto committedTiles = editor.map.getTileCount();
	check(editor.actionQueue->undo(), "Ruin Undo refused");
	check(editor.map.getTileCount() == baselineTiles, "Undo changed the tile count");
	check(editor.actionQueue->redo(), "Ruin Redo refused");
	check(editor.map.getTileCount() == committedTiles, "Redo did not restore the tiles");
	editor.map.clearChanges();
	report << "SURFACE_RUINS_E2E_PASS" << '\t' << side << '\t' << "gaps=" << grid->gaps.size() << '\t' << "sites=" << grid->sites.size() << '\t' << "walls=" << plannedWalls << '\n';
}

// Camp fires (G5 iteration 26). The 10.98 client of the probe has no "campfire" doodad brush, so "small rocks" stands in: the probe checks the mechanism
// (one item of the brush on every fire cell and nowhere else, Undo and Redo, refusals), not the look.
void campfires(uint32_t side, std::ostream& report) {
	UndoLimits undoLimits;
	check(g_gui.NewMap(), "Editor creation failed");
	auto& editor = *g_gui.GetCurrentEditor();
	GenerationConfig config {{100, 100, 7, side, side}, 1729};
	config.algorithm = GenerationAlgorithm::Surface;
	config.surface.campCount = 8;
	GroundProfile profile {{"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount};
	constexpr PreparationBudget preparation {250000, 254016, 250000, 512ULL * 1024 * 1024};
	constexpr TerrainCommitBudget commit {250000, 768ULL * 1024 * 1024};
	auto plain = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(plain), "Camps without a fire failed, error index " + std::to_string(plain ? 0 : plain.error().index()));
	profile.campfire = "small rocks";
	const auto baselineTiles = editor.map.getTileCount();
	const auto prior = editor.actionQueue->getCurrentIndex();
	auto prepared = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(prepared), "Camp fire preparation failed, error index " + std::to_string(prepared ? 0 : prepared.error().index()) +
		(prepared || prepared.error().index() != 4 ? std::string() : " preview error " + std::to_string(int(std::get<PreviewError>(prepared.error())))));
	if (!plain || !prepared) return;
	check(bool(validatePreparedTerrain(editor.map, *prepared, config, profile)), "Camp fire validation failed");
	check(prepared->plan.fingerprint() != plain->plan.fingerprint(), "A fire did not change the plan fingerprint");
	const auto grid = generateSurface({config.region, config.seed, config.surface}, MapExtent {65001, 65001, 16}, surfaceBudget);
	check(bool(grid) && !grid->fires.empty(), "The surface has no camp fire cells");
	if (!grid) return;
	auto fireCells = grid->fires;
	std::ranges::sort(fireCells);
	size_t plannedFires = 0;
	for (size_t i = 0; i < prepared->plan.fires.size(); ++i) {
		const bool planned = prepared->plan.fires[i] != 0;
		plannedFires += planned;
		check(planned == std::ranges::binary_search(fireCells, uint32_t(i)), "The fire layer differs from the fire cells");
	}
	check(plannedFires == grid->fires.size(), "The number of planned fires differs from the fire cells");
	auto unknown = profile;
	unknown.campfire = "no such brush";
	check(!prepareTerrain(editor.map, config, unknown, preparation), "An unknown fire brush was accepted");
	auto ground = profile;
	ground.campfire = "grass";
	check(!prepareTerrain(editor.map, config, ground, preparation), "A ground brush was accepted as the fire brush");
	auto sixBrushes = profile;
	sixBrushes.count = GroundProfile::surfaceCount;
	check(!prepareTerrain(editor.map, config, sixBrushes, preparation), "A fire was accepted without a floor brush");
	const auto expectedFires = prepared->plan.fires;
	const auto receipt = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
	check(bool(receipt), "Camp fire commit failed, error index " + std::to_string(receipt ? 0 : receipt.error().index()));
	check(editor.actionQueue->getCurrentIndex() == prior + 1 && editor.actionQueue->getSize() == prior + 1, "Camp fire commit is not one history step");
	const auto countFires = [&](bool placed) {
		size_t found = 0;
		for (size_t i = 0; i < expectedFires.size(); ++i) {
			const auto* tile = editor.map.getTile(Position(100 + int(i % side), 100 + int(i / side), 7));
			size_t here = 0;
			if (tile && expectedFires[i]) for (const auto& item : tile->items) here += item->getID() == expectedFires[i];
			check(here == (placed && expectedFires[i] ? 1u : 0u), "Fire item count on a tile differs from the plan");
			found += here;
		}
		return found;
	};
	check(countFires(true) == plannedFires, "The number of fire items differs from the plan");
	const auto committedTiles = editor.map.getTileCount();
	check(editor.actionQueue->undo(), "Camp fire Undo refused");
	check(countFires(false) == 0 && editor.map.getTileCount() == baselineTiles, "Undo left fire items or changed the tile count");
	check(editor.actionQueue->redo(), "Camp fire Redo refused");
	check(countFires(true) == plannedFires && editor.map.getTileCount() == committedTiles, "Redo did not restore the fires");
	editor.map.clearChanges();
	report << "SURFACE_FIRES_E2E_PASS" << '\t' << side << '\t' << "fires=" << plannedFires << '\n';
}

// Iteration 28: real 10.98 cave brushes, two previews, two history steps and failure rollback.
bool caves(uint32_t side, CaveEntranceStyle style, std::ostream& report) {
	UndoLimits undoLimits;
	check(g_gui.NewMap(), "Cave editor creation failed");
	auto& editor = *g_gui.GetCurrentEditor();
	GenerationConfig config {{100, 100, 7, side, side}, 1729};
	config.algorithm = GenerationAlgorithm::Surface;
	config.surface.buildingDensity = 10;
	config.surface.caveEntrances = 20;
    config.surface.caveFarExits = false; // historical entrance/ramp one-to-one regression
	config.surface.caveEntranceStyle = style;
	config.surface.rockPermille = 200;
	GroundProfile profile {{"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount};
	constexpr PreparationBudget preparation {250000, 254016, 250000, 512ULL * 1024 * 1024};
	constexpr TerrainCommitBudget commit {250000, 768ULL * 1024 * 1024};
	// Two earlier history steps outside the area, so the full-history case below has old steps to drop.
	for (const int32_t x : {400, 420}) {
		const GenerationConfig earlier {{x, 400, 7, 10, 10}, 7};
		const GroundProfile roll {{"grass", "dirt", "sea"}, GroundProfile::groundRollCount};
		const auto step = prepareTerrain(editor.map, earlier, roll, preparation);
		check(step && DrawOperations::commitTerrainPreview(*step, earlier, roll, commit), "Earlier history step failed");
	}
	auto plain = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(plain), "Cave baseline preparation failed");
	profile.caveEntrance = style == CaveEntranceStyle::RockFace ? "cave enter" : "cave hole";
	profile.caveExit = "ramp";
	profile.caveGround = "cave";
	profile.caveWall = "mountain";
	auto prepared = prepareTerrain(editor.map, config, profile, preparation);
	check(bool(prepared), std::format("Cave preparation failed: domain {} value {}", prepared ? 0 : prepared.error().index(),
		prepared ? 0 : std::visit([](auto error) { return int(error); }, prepared.error())));
	check(bool(prepared->cave), "Cave unit missing");
	check(bool(validateTerrainTarget(*prepared, config, profile)), "Cave validation failed");
	check(plain->plan.fingerprint() != prepared->plan.fingerprint(), "Cave did not change fingerprint");
	check(!prepared->preview.getChanges().empty() && !prepared->cave->preview.getChanges().empty(), "Both floors need preview changes");
	const auto& input = prepared->snapshot.input;
	const auto keepClear = surfaceKeepClear(prepared->snapshot, profileNeedsEdgeClearance(profile));
	auto protection = input.protection;
	for (size_t i = 0; i < protection.size(); ++i) protection[i] |= keepClear[i];
	const auto grid = generateSurface({config.region, config.seed, config.surface}, input.extent, surfaceBudget, input.mask, protection);
	check(bool(grid), "Cave reference surface failed");
	const auto cave = generateCave(*grid, config.seed);
	const auto brushes = resolveCaveBrushes(profile, config);
	check(bool(cave) && bool(brushes), "Cave geometry/brush resolution failed");
	const auto exits = placeCaveExits(*grid, *cave, brushes->shapes);
	if (exits) report << std::format("SURFACE_CAVE_LAYOUT style={} entrances={} exits={} skipped={}\n",
		style == CaveEntranceStyle::RockFace ? "rockface" : "floorhole", grid->entrances.size(), exits->exits.size(), exits->skipped.size());
	check(bool(exits), "Cave exit planning failed");
	const bool sufficientExits = exits->exits.size() >= 2;
	std::vector<uint16_t> entrances(grid->cells.size(), 0), ramps(entrances);
	for (const auto& exit : exits->exits) {
		const auto e = grid->entrances[exit.entrance];
		const auto& ids = brushes->items[exit.shape];
		const auto& shape = brushes->shapes[exit.shape];
		entrances[e] = brushes->entrances[(uint64_t(e) + config.seed) % brushes->entrances.size()];
		ramps[exit.cell] = ids.ramp;
		if (shape.hasTail) ramps[size_t(int64_t(exit.cell) + shape.tailDx + int64_t(shape.tailDy) * side)] = ids.tail;
	}
	check(prepared->plan.entrances == entrances && prepared->cave->plan.exits == ramps,
		"Cave item layers differ from exit geometry");
	for (const auto e : grid->entrances) check(caveEntranceAllowed(grid->cells, side, side, e, style), "Entrance violates style rule");
	constexpr int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
	for (const auto& exit : exits->exits) {
		const auto d = size_t(brushes->shapes[exit.shape].direction);
		const auto destination = size_t(int64_t(exit.cell) + dx[d] + int64_t(dy[d]) * side);
		check(exit.cell == grid->entrances[exit.entrance], "Ramp not directly under entrance");
		const auto kind = grid->cells[destination];
		check(kind == SurfaceKind::Grass || kind == SurfaceKind::Dirt || kind == SurfaceKind::Shore || kind == SurfaceKind::Path, "Destination not open");
		check(prepared->plan.items[destination] == plain->plan.items[destination], "Destination ground changed");
		const auto landing = size_t(int64_t(exit.cell) - dx[d] - int64_t(dy[d]) * side);
		const Position lower(100 + int(landing % side), 100 + int(landing / side), 8);
		const auto* tile = prepared->cave->preview.getBuffer().getTile(lower);
		check(tile && tile->ground && brushOf(tile->ground->getID()) == g_brushes.getBrush(profile.caveGround), "Landing lacks cave ground");
	}
	// Resolve the other style on the same empty target to authenticate distinct plans.
	{ auto otherConfig = config; auto otherProfile = profile;
		otherConfig.surface.caveEntranceStyle = style == CaveEntranceStyle::RockFace ? CaveEntranceStyle::FloorHole : CaveEntranceStyle::RockFace;
		otherProfile.caveEntrance = style == CaveEntranceStyle::RockFace ? "cave hole" : "cave enter";
		const auto other = prepareTerrain(editor.map, otherConfig, otherProfile, preparation);
		check(other && other->plan.fingerprint() != prepared->plan.fingerprint(), "Styles share fingerprint"); }
	size_t caveCells = 0, wallCells = 0;
	for (size_t i = 0; i < cave->cells.size(); ++i) {
		const Position upper(100 + int(i % side), 100 + int(i / side), 7), lower(upper.x, upper.y, 8);
		const auto* tile = prepared->cave->preview.getBuffer().getTile(lower);
		const bool floor = cave->cells[i] == CaveKind::Floor;
		caveCells += floor;
		bool adjacent = false;
		const int cx = int(i % side), cy = int(i / side);
		for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
			if (cx + dx >= 0 && cy + dy >= 0 && cx + dx < int(side) && cy + dy < int(side))
				adjacent |= cave->cells[size_t(cy + dy) * side + size_t(cx + dx)] == CaveKind::Floor;
		const bool wall = !floor && adjacent;
		wallCells += wall;
		check(bool(tile && tile->ground) == (floor || wall), "Cave Floor/wall band mismatch");
		if (wall) check(brushOf(tile->ground->getID()) == g_brushes.getBrush(profile.caveWall), "Foreign cave wall ground");
		if (style == CaveEntranceStyle::RockFace && entrances[i]) {
			check(prepared->plan.items[i] == plain->plan.items[i], "Rock entrance ground changed");
		}
		if (style == CaveEntranceStyle::FloorHole && entrances[i]) {
			const auto* hole = prepared->preview.getBuffer().getTile(upper);
			check(hole && hole->ground && hole->ground->getID() == entrances[i], "Hole ground missing");
		}
		if (floor) check(brushOf(tile->ground->getID()) == g_brushes.getBrush(profile.caveGround), "Foreign cave ground");
		const auto checkItem = [&](const PreparedTerrain& unit, const Position& position, uint16_t expected, std::span<const uint16_t> allowed) {
			const auto* here = unit.preview.getBuffer().getTile(position);
			size_t found = 0;
			if (here && here->ground && std::ranges::find(allowed, here->ground->getID()) != allowed.end()) {
				check(here->ground->getID() == expected, "Exit top ground on wrong cell"); ++found;
			}
			if (here) for (const auto& item : here->items) {
				if (std::ranges::find(allowed, item->getID()) == allowed.end()) continue;
				check(item->getID() == expected, "Cave item on wrong cell"); ++found;
			}
			check(found == (expected ? 1u : 0u), "Cave item missing or duplicated");
		};
		std::vector<uint16_t> topIds, rampIds;
		for (const auto& ids : brushes->items) { rampIds.push_back(ids.ramp); if (ids.tail) rampIds.push_back(ids.tail); }
		// Independently inspect the source composite, since the resolved plan has no top layer.
		const auto* exitBrush = g_brushes.getBrush(profile.caveExit)->as<DoodadBrush>();
		for (int variant = 0; variant < exitBrush->getTotalVariants(); ++variant)
			for (const auto& [pos, items] : *exitBrush->getVariant(variant).composite)
				if (pos.z == -1) for (const auto& item : items) topIds.push_back(item->getID());
		checkItem(*prepared, upper, entrances[i], brushes->entrances);
		checkItem(*prepared, upper, 0, topIds);
		checkItem(*prepared->cave, lower, ramps[i], rampIds);
		if (!floor && tile) {
			bool adjacent = false;
			const int x = int(i % side), y = int(i / side);
			for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
				if (x + dx >= 0 && y + dy >= 0 && x + dx < int(side) && y + dy < int(side))
					adjacent |= cave->cells[size_t(y + dy) * side + size_t(x + dx)] == CaveKind::Floor;
			check((adjacent || !tile->ground) && std::ranges::all_of(tile->items, [](const auto& item) { return item->isBorder(); }), "Nonadjacent Solid cell changed");
		}
	}
	for (int y = 97; y < 103 + int(side); ++y) for (int x = 97; x < 103 + int(side); ++x) {
		if (x >= 100 && y >= 100 && x < 100 + int(side) && y < 100 + int(side)) continue;
		check(!prepared->cave->preview.getBuffer().getTile(Position(x, y, 8)), "Cave preview escaped rectangle");
	}
	for (const auto id : brushes->entrances) check(!Item::Create(id)->isBlocking(), "Cave entrance blocks movement in client data");
	for (const auto& ids : brushes->items) check(!Item::Create(ids.ramp)->isBlocking(), "Cave ramp blocks movement in client data");
	const auto beforeUpper = stackImage(editor.map, 97, 97, 7, int(side) + 6, int(side) + 6);
	const auto beforeLower = stackImage(editor.map, 97, 97, 8, int(side) + 6, int(side) + 6);
	{ auto limited = preparation; limited.maxBytes = prepared->memory.totalBytes - 1;
		check(!prepareTerrain(editor.map, config, profile, limited), "Joint preparation budget was exceeded"); }
	{ auto limited = preparation; limited.maxChanges = prepared->preview.getChanges().size() + prepared->cave->preview.getChanges().size() - 1;
		check(!prepareTerrain(editor.map, config, profile, limited), "Joint change budget was exceeded"); }
	{ const auto cell = exits->exits.empty() ? grid->entrances.front() : exits->exits.front().cell;
		const auto saved = prepared->cave->plan.exits[cell];
		prepared->cave->plan.exits[cell] ^= 1;
		check(!validateTerrainTarget(*prepared, config, profile), "Tampered lower plan passed target validation");
		prepared->cave->plan.exits[cell] = saved; }
	const auto prior = editor.actionQueue->getCurrentIndex(), size = editor.actionQueue->getSize();
	// A one-step history limit admits the provisional cave, then deterministically refuses surface.
	g_settings.setInteger(Config::UNDO_SIZE, 1);
	const auto refused = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
	g_settings.setInteger(Config::UNDO_SIZE, 1000);
	check(!refused && std::holds_alternative<TerrainCommitFailure>(refused.error()) &&
		std::get<TerrainCommitFailure>(refused.error()) == TerrainCommitFailure::HistoryRefused, "Step-two failure not exercised");
	check(editor.actionQueue->getCurrentIndex() == prior && editor.actionQueue->getSize() == size && !editor.actionQueue->canRedo(), "Rollback left generator history/redo");
	check(stackImage(editor.map, 97, 97, 7, int(side) + 6, int(side) + 6) == beforeUpper &&
		stackImage(editor.map, 97, 97, 8, int(side) + 6, int(side) + 6) == beforeLower, "Rollback changed map tiles");
	check(bool(validateTerrainTarget(*prepared, config, profile)), "Rollback invalidated original preparation");
	// A history already at its step limit: the pair must drop the oldest steps, not refuse.
	const auto atLimit = int(editor.actionQueue->getSize());
	check(atLimit >= 2, "Cave probe needs earlier history steps");
	g_settings.setInteger(Config::UNDO_SIZE, atLimit);
	const auto receipt = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
	g_settings.setInteger(Config::UNDO_SIZE, 1000);
	check(bool(receipt), "Two-floor commit failed with a full history");
	check(receipt->changes == prepared->preview.getChanges().size() + prepared->cave->preview.getChanges().size(), "Receipt lost changes");
	check(editor.actionQueue->getSize() == size_t(atLimit) && editor.actionQueue->getCurrentIndex() == size_t(atLimit), "Full history was not trimmed to its limit");
	for (const auto* unit : {&*prepared, prepared->cave.get()}) for (const auto& position : unit->preview.getChanges())
		check(snapshotTilesEqual(editor.map.getTile(position), unit->preview.getBuffer().getTile(position)), "Map differs from preview");
	const auto afterUpper = stackImage(editor.map, 97, 97, 7, int(side) + 6, int(side) + 6);
	const auto afterLower = stackImage(editor.map, 97, 97, 8, int(side) + 6, int(side) + 6);
	check(editor.actionQueue->undo(), "Surface Undo refused");
	check(stackImage(editor.map, 97, 97, 7, int(side) + 6, int(side) + 6) == beforeUpper &&
		stackImage(editor.map, 97, 97, 8, int(side) + 6, int(side) + 6) == afterLower, "First Undo did not leave cave alone");
	check(editor.actionQueue->undo(), "Cave Undo refused");
	check(stackImage(editor.map, 97, 97, 7, int(side) + 6, int(side) + 6) == beforeUpper &&
		stackImage(editor.map, 97, 97, 8, int(side) + 6, int(side) + 6) == beforeLower, "Second Undo did not restore both floors");
	check(editor.actionQueue->redo() && editor.actionQueue->redo(), "Cave Redo refused");
	check(stackImage(editor.map, 97, 97, 7, int(side) + 6, int(side) + 6) == afterUpper &&
		stackImage(editor.map, 97, 97, 8, int(side) + 6, int(side) + 6) == afterLower, "Redo did not restore both floors");
	editor.map.clearChanges();
	// Fresh map for fail-closed input tests, including an occupied cell outside a sparse mask.
	check(g_gui.NewMap(), "Cave refusal editor failed");
	auto& emptyMap = g_gui.GetCurrentEditor()->map;
	const auto invalidProfile = [&](const GroundProfile& bad) {
		const auto result = prepareTerrain(emptyMap, config, bad, preparation);
		check(!result && ((std::holds_alternative<PreparationInputError>(result.error()) && std::get<PreparationInputError>(result.error()) == PreparationInputError::InvalidProfile) ||
			(std::holds_alternative<ResolveError>(result.error()) && std::get<ResolveError>(result.error()) == ResolveError::InvalidProfile)), "Invalid cave profile accepted/wrong error");
	};
	for (const std::string name : {"no such brush", "grass"}) { auto bad = profile; bad.caveEntrance = name; invalidProfile(bad); }
	{ auto bad = profile; bad.caveExit = "small rocks"; invalidProfile(bad); }
	{ auto bad = profile; bad.caveGround = "small rocks"; invalidProfile(bad); }
	{ auto bad = profile; bad.caveExit.clear(); invalidProfile(bad); }
	{ auto bad = profile; bad.caveWall.clear(); invalidProfile(bad); }
	{ auto bad = profile; bad.caveWall = "small rocks"; invalidProfile(bad); }
	{ auto bad = profile; bad.caveEntrance = style == CaveEntranceStyle::RockFace ? "cave hole" : "cave enter"; invalidProfile(bad); }
	// The dialog's exit list is every doodad brush resolveCaveExitBrush accepts: in 10.98 exactly the five four-way ramps.
	std::vector<std::string> exitBrushes;
	for (const auto& [name, brush] : g_brushes.getMap()) {
		const auto* doodad = brush->as<DoodadBrush>();
		if (!doodad) continue;
		const auto resolved = resolveCaveExitBrush(*doodad);
		if (!resolved) continue;
		check(resolved->shapes.size() == 4, "Exit brush " + name + " has not four variants");
		if (std::ranges::find(exitBrushes, name) == exitBrushes.end()) exitBrushes.push_back(name);
	}
	std::ranges::sort(exitBrushes);
	check(exitBrushes == std::vector<std::string>{"bamboo ramp", "dirt ramp", "pyramid ramp", "ramp", "snowy ramp"}, "Unexpected cave exit brush list");
	{ auto bad = config; bad.region.z = 15; const auto result = prepareTerrain(emptyMap, bad, profile, preparation);
		check(!result && std::get<PreparationInputError>(result.error()) == PreparationInputError::NoLowerFloor, "Floor 15 accepted"); }
	{
		auto vegetated = config; vegetated.vegetation = true;
		auto decorated = profile; decorated.decorCount = GroundProfile::decorationCount;
		decorated.decor = {"small rocks", "small rocks", "small rocks"};
		std::vector<uint8_t> sparse(size_t(side) * side, 1); sparse[0] = 0;
		const auto masked = prepareTerrain(emptyMap, vegetated, decorated, preparation, sparse);
		check(masked && masked->cave && validateTerrainTarget(*masked, vegetated, decorated, sparse), "Masked vegetated cave failed");
		for (size_t i = 0; i < masked->plan.items.size(); ++i)
			if (masked->plan.entrances[i]) check(!masked->plan.decor[i], "Vegetation overlaps cave access");
	}
	{
		auto noRock = config; noRock.surface.rockPermille = 0;
		noRock.surface.caveEntranceStyle = CaveEntranceStyle::RockFace;
		auto noRockProfile = profile; noRockProfile.caveEntrance = "cave enter";
		auto emptyCave = prepareTerrain(emptyMap, noRock, noRockProfile, preparation);
		check(emptyCave && emptyCave->cave && emptyCave->cave->preview.getChanges().empty(), "Expected an empty cave without rocks");
		auto* queue = g_gui.GetCurrentEditor()->actionQueue.get();
		const auto before = queue->getCurrentIndex();
		check(bool(DrawOperations::commitTerrainPreview(*emptyCave, noRock, noRockProfile, commit)), "Empty cave commit failed");
		check(queue->getCurrentIndex() == before + 1 && queue->undo(), "Empty cave must add exactly one history step");
		check(stackImage(emptyMap, 97, 97, 7, int(side) + 6, int(side) + 6) == beforeUpper &&
			stackImage(emptyMap, 97, 97, 8, int(side) + 6, int(side) + 6) == beforeLower, "Empty cave undo changed the map");
	}
	const auto id = g_brushes.getBrush("grass")->as<GroundBrush>()->getPlacementItemId();
	TerrainPlacement::placeBrushItem(*emptyMap.getOrCreateTile(Position(100, 100, 8)), id);
	std::vector<uint8_t> mask(size_t(side) * side, 1); mask[0] = 0;
	const auto occupied = prepareTerrain(emptyMap, config, profile, preparation, mask);
	check(!occupied && std::get<PreparationInputError>(occupied.error()) == PreparationInputError::LowerFloorOccupied, "Masked occupied lower floor accepted");
	emptyMap.clearChanges();
	report << (sufficientExits ? "SURFACE_CAVE_E2E_PASS	" : "SURFACE_CAVE_E2E_FAIL	") << side << "	style=" << (style == CaveEntranceStyle::RockFace ? "rockface" : "floorhole") << " entrances=" << grid->entrances.size() << " exits=" << exits->exits.size()
		<< " skipped=" << exits->skipped.size() << " caveCells=" << caveCells << " wallCells=" << wallCells << " exitBrushes=" << exitBrushes.size() << '\n';
	return sufficientExits;
}

// Iteration 31: GUI-sized offset rectangles through the real preparation and commit path.
void guiCaves(std::ostream& report) {
	UndoLimits undoLimits;
	constexpr uint32_t side = 512;
	constexpr PreparationBudget preparation {600000, 610000, 600000, 2ULL * 1024 * 1024 * 1024};
	constexpr TerrainCommitBudget commit {600000, 3ULL * 1024 * 1024 * 1024};
	for (const auto style : {CaveEntranceStyle::RockFace, CaveEntranceStyle::FloorHole})
	for (const uint32_t buildings : {0u, 40u})
	for (const std::string rockBrush : {"rocky ground", "mountain"})
	for (const uint64_t seed : {1729u, 1001u, 1002u, 1003u, 1004u, 1005u, 1006u, 1007u}) {
		check(g_gui.NewMap(), "GUI cave editor creation failed");
		auto& editor = *g_gui.GetCurrentEditor();
		GenerationConfig config {{800, 1000, 7, side, side}, seed};
		config.algorithm = GenerationAlgorithm::Surface;
		config.vegetation = true;
		config.surface.pathLength = 80;
		config.surface.repairReach = 1;
		config.surface.shoreSmoothing = 2;
		config.surface.buildingDensity = buildings;
		config.surface.caveEntrances = 20;
        config.surface.caveFarExits = false; // historical one-to-one matrix; new exits have their own GUI probe
		config.surface.caveEntranceStyle = style;
		config.surface.rockPermille = 200;
		GroundProfile profile {{"grass", "dirt", "mountain", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount};
		profile.brushes[2] = rockBrush;
		profile.decor = {"trees", "bushes", "small rocks"};
		profile.decorCount = GroundProfile::decorationCount;
		if (buildings) profile.wall = "stone wall";
		profile.caveEntrance = style == CaveEntranceStyle::RockFace ? "cave enter" : "cave hole";
		profile.caveExit = "ramp";
		profile.caveGround = "cave";
		profile.caveWall = "mountain";
		const auto combination = std::format("style={} buildings={} rock={} seed={}",
			style == CaveEntranceStyle::RockFace ? "rockface" : "floorhole", buildings, rockBrush, seed);
		auto prepared = prepareTerrain(editor.map, config, profile, preparation);
		check(prepared && prepared->cave, std::format("GUI cave preparation failed {}", combination));
		check(bool(validateTerrainTarget(*prepared, config, profile)), "GUI cave validation failed");
		const auto brushes = resolveCaveBrushes(profile, config);
		check(bool(brushes), "GUI cave brushes failed");
		const auto receipt = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
		check(bool(receipt), std::format("GUI cave commit failed {}", combination));
		size_t entrances = 0, exits = 0;
		for (size_t i = 0; i < size_t(side) * side; ++i) {
			const Position upper(config.region.x + int(i % side), config.region.y + int(i / side), config.region.z);
			const auto* surface = editor.map.getTile(upper);
			const auto* lower = editor.map.getTile(Position(upper.x, upper.y, upper.z + 1));
			const auto countIds = [](const Tile* tile, const auto& matches) {
				size_t count = 0;
				if (tile) {
					if (tile->ground) count += matches(tile->ground->getID());
					for (const auto& item : tile->items) count += matches(item->getID());
				}
				return count;
			};
			const auto entranceCount = countIds(surface, [&](uint16_t id) {
				return std::ranges::find(brushes->entrances, id) != brushes->entrances.end();
			});
			const auto rampCount = countIds(lower, [&](uint16_t id) {
				return std::ranges::any_of(brushes->items, [&](const auto& ids) { return ids.ramp == id; });
			});
			check(entranceCount == rampCount && entranceCount <= 1,
				std::format("GUI cave mismatch {} cell=({},{},{}) entrances={} ramps={} plannedEntrance={} plannedExit={}",
					combination, upper.x, upper.y, upper.z, entranceCount, rampCount,
					prepared->plan.entrances[i], prepared->cave->plan.exits[i]));
			if (style == CaveEntranceStyle::FloorHole && entranceCount)
				check(surface && surface->ground && surface->ground->getID() == prepared->plan.entrances[i], "GUI hole is not ground");
			check(entranceCount == (prepared->plan.entrances[i] ? 1u : 0u), "GUI commit lost planned entrance");
			entrances += entranceCount;
			exits += rampCount;
		}
        const bool noCaveFloor = std::ranges::none_of(prepared->cave->plan.caveFloor, [](auto kind) { return kind == 1; });
		check((entrances > 0 && exits > 0) || (prepared->levels && noCaveFloor &&
            (prepared->levels->repair.removedPois > 0 || prepared->levels->repair.removedCaveComponents > 0)),
            std::format("GUI cave case lost entrances without a reported removal and empty cave {}", combination));
		editor.map.clearChanges();
		report << std::format("SURFACE_CAVE_GUI_PASS\t512\t{} entrances={} exits={} fingerprint={} caveFingerprint={} preview={} cavePreview={}\n",
			combination, entrances, exits, prepared->plan.fingerprint(), prepared->cave->plan.fingerprint(),
			prepared->preview.getFingerprint(), prepared->cave->preview.getFingerprint());
		g_gui.CloseCurrentEditor();
	}
}

// D5: a surface generated next to existing terrain (a ring around the area, optionally one existing
// tile inside it) must neither be refused nor touch those tiles. Cells beside existing ground stay empty.
void nextToTerrain(uint32_t side, uint64_t seed, bool withHole, std::ostream& report) {
	UndoLimits undoLimits;
	check(g_gui.NewMap(), "Editor creation failed");
	auto& editor = *g_gui.GetCurrentEditor();
	const int x0 = 100, y0 = 100, z = 7, area = int(side);
	const auto grassId = g_brushes.getBrush("grass")->as<GroundBrush>()->getBorderItems().front().id;
	const auto inArea = [&](int x, int y) { return x >= x0 && y >= y0 && x < x0 + area && y < y0 + area; };
	const int holeX = x0 + area / 2, holeY = y0 + area / 2;
	std::vector<Position> existing;
	for (int y = y0 - 3; y < y0 + area + 3; ++y) for (int x = x0 - 3; x < x0 + area + 3; ++x) {
		if (inArea(x, y) && !(withHole && x == holeX && y == holeY)) continue;
		TerrainPlacement::placeBrushItem(*editor.map.getOrCreateTile(Position(x, y, z)), grassId);
		existing.emplace_back(x, y, z);
	}
	const auto existingImage = [&] {
		std::vector<std::vector<uint32_t>> image;
		for (const auto& position : existing) image.push_back(stackImage(editor.map, position.x, position.y, z, 1, 1));
		return image;
	};
	const auto before = existingImage();
	GenerationConfig config {{x0, y0, z, side, side}, seed};
	config.algorithm = GenerationAlgorithm::Surface;
	constexpr PreparationBudget preparation {250000, 254016, 250000, 512ULL * 1024 * 1024};
	constexpr TerrainCommitBudget commit {250000, 768ULL * 1024 * 1024};
	auto prepared = prepareTerrain(editor.map, config, ::profile, preparation);
	check(bool(prepared), "Surface next to existing terrain refused, error index " + std::to_string(prepared ? 0 : prepared.error().index()) +
		(prepared || prepared.error().index() != 4 ? std::string() : " preview error " + std::to_string(int(std::get<PreviewError>(prepared.error())))));
	check(bool(validatePreparedTerrain(editor.map, *prepared, config, ::profile)), "Surface next to terrain failed validation");
	const auto& items = prepared->plan.items;
	size_t gap = 0, placed = 0;
	for (size_t i = 0; i < items.size(); ++i) {
		const int x = x0 + int(i % side), y = y0 + int(i / side);
		const bool hole = withHole && x == holeX && y == holeY;
		bool beside = false;
		for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
			const int nx = x + dx, ny = y + dy;
			beside |= (dx || dy) && (!inArea(nx, ny) || (withHole && nx == holeX && ny == holeY));
		}
		check((items[i] == 0) == (hole || beside), "Cell beside existing ground is not empty, or a far cell is");
		gap += !hole && beside;
		placed += items[i] != 0;
	}
	check(gap > 0 && placed > 0, "No gap or no terrain generated");
	const auto receipt = DrawOperations::commitTerrainPreview(*prepared, config, ::profile, commit);
	check(bool(receipt), "Commit next to existing terrain failed");
	check(existingImage() == before, "Commit changed existing terrain");
	check(editor.actionQueue->undo(), "Undo refused next to terrain");
	check(existingImage() == before, "Undo changed existing terrain");
	check(editor.actionQueue->redo(), "Redo refused next to terrain");
	check(existingImage() == before, "Redo changed existing terrain");
	editor.map.clearChanges();
	report << "SURFACE_NEXT_TO_TERRAIN_PASS\t" << side << '\t' << seed << "\thole=" << withHole << "\tgap=" << gap << "\tplaced=" << placed << '\n';
}

void mountainDialogControls(std::ostream& report)
{
    check(g_gui.NewMap(), "Mountain dialog source map");
    auto* dialog = new TerrainGeneratorDialog(g_gui.root);
    auto* style = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-style", dialog));
    auto* percent = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName("terrain-plateau-percent", dialog));
    auto* levels = wxWindow::FindWindowByName("terrain-plateau-levels", dialog);
    check(style && percent && levels, "Mountain dialog controls missing");
    auto* width = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName("terrain-width", dialog));
    auto* height = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName("terrain-height", dialog));
    check(width && height && width->GetMax() == 2000 && height->GetMax() == 2000, "Generation dialog must allow 2000 x 2000");
    check(!wxWindow::FindWindowByName("terrain-plateau-min-top", dialog), "Obsolete mountain cell input");
    style->SetSelection(1);
    wxCommandEvent changed(wxEVT_CHOICE, style->GetId());
    changed.SetEventObject(style);
    style->GetEventHandler()->ProcessEvent(changed);
    check(percent->GetValue() == 10 && percent->IsEnabled() && levels->IsEnabled(), "Percentage must enable mountains directly");
    percent->SetValue(0);
    wxSpinEvent disabled(wxEVT_SPINCTRL, percent->GetId());
    disabled.SetEventObject(percent);
    percent->GetEventHandler()->ProcessEvent(disabled);
    check(percent->IsEnabled() && !levels->IsEnabled(), "Zero percent must disable mountain options");
    dialog->Destroy();
    report << "SURFACE_MOUNTAIN_DIALOG_PERCENT_PASS\n";
}

// Exercise the actual dialog request, timer preparation, validation and Place handlers.
// Direct prepareTerrain probes alone cannot catch a GUI-only mountain enable/threshold bug.
class MountainDialogRun final : public wxEvtHandler {
public:
    MountainDialogRun(TerrainGeneratorDialog& dialog, Editor& editor, size_t floors, bool requireReachable = false) : dialog_(dialog), editor_(editor), floors_(floors), requireReachable_(requireReachable),
        sourceChanged_(editor.map.hasChanged()), sourceGeneration_(editor.map.getGeneration()), sourceRevision_(editor.map.getChangeTracker().getGeneration()) {
        unchanged();
        Bind(wxEVT_TIMER, &MountainDialogRun::tick, this);
        timer_.Start(20);
    }
    ~MountainDialogRun() override {timer_.Stop();}
    void stop() {timer_.Stop();}
    bool placed() const {return phase_ == Phase::Placed && failure_.empty();}
    const std::string& failure() const {return failure_;}
    size_t floors() const {return floors_;}
    const std::string& status() const {return status_;}
private:
    enum class Phase {Start, Preparing, Placing, Placed};
    void command(const char* name) {
        auto* button = dynamic_cast<wxButton*>(wxWindow::FindWindowByName(name, &dialog_));
        check(button && button->IsEnabled(), std::format("GUI mountain command unavailable: {}", name));
        wxCommandEvent event(wxEVT_BUTTON, button->GetId());
        event.SetEventObject(button);
        button->GetEventHandler()->ProcessEvent(event);
    }
    void tick(wxTimerEvent&) {
        try {
            check(std::chrono::steady_clock::now() - started_ < std::chrono::seconds(120), "GUI mountain dialog timeout");
            if (phase_ == Phase::Start) {
                phase_ = Phase::Preparing;
                command("terrain-preview");
            } else if (phase_ == Phase::Preparing) {
                auto* status = wxWindow::FindWindowByName("terrain-status", &dialog_);
                check(status, "GUI mountain status missing");
                const auto message = status->GetLabel();
                if (message.StartsWith("Preparing preview")) return;
                status_ = message.ToStdString();
                if (requireReachable_) check(message.StartsWith("Reachable:") && !message.Contains("Unrepaired") && !message.Contains("Unreachable"),
                    "Final-mask repair status: " + status_);
                check(message.StartsWith("Preview ready:") || message.StartsWith("Reachable:"), "GUI mountain preview refused: " + message.ToStdString());
                unchanged();
                auto* floors = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-preview-floor", &dialog_));
                if (floors && !floors_) floors_ = floors->GetCount();
                check(floors && floors->GetCount() == floors_, std::format(
                    "GUI mountain floor count: expected {}, got {} (percentage did not enable caps)", floors_, floors ? floors->GetCount() : 0));
                command("terrain-validate");
                check(status->GetLabel().StartsWith("Preview validated."), "GUI mountain validation refused");
                unchanged();
                phase_ = Phase::Placing;
            } else if (phase_ == Phase::Placing) {
                phase_ = Phase::Placed;
                command("terrain-place");
                check(!dialog_.IsShown(), "GUI mountain Place refused");
                timer_.Stop();
            }
        } catch (const std::exception& error) {
            failure_ = error.what();
            timer_.Stop();
            dialog_.EndModal(wxID_CANCEL);
        }
    }
    void unchanged() const {
        check(g_gui.GetCurrentEditor() == &editor_ && editor_.map.getTileCount() == 0 &&
            editor_.actionQueue->getSize() == 0 && editor_.map.hasChanged() == sourceChanged_ &&
            editor_.map.getGeneration() == sourceGeneration_ && editor_.map.getChangeTracker().getGeneration() == sourceRevision_,
            "GUI Preview/Validate changed source or history");
    }
    TerrainGeneratorDialog& dialog_;
    Editor& editor_;
    size_t floors_;
    bool requireReachable_;
    std::string status_;
    bool sourceChanged_;
    uint64_t sourceGeneration_, sourceRevision_;
    Phase phase_ = Phase::Start;
    std::string failure_;
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
    wxTimer timer_{this};
};

void guiMountainPercentage(std::ostream& report) {
    UndoLimits undoLimits;
    constexpr uint32_t side = 300;
    for (const uint32_t percent : {10u, 0u}) {
        check(g_gui.NewMap(), "GUI mountain editor creation");
        auto& editor = *g_gui.GetCurrentEditor();
        auto* dialog = new TerrainGeneratorDialog(g_gui.root);
        const auto spin = [&](const char* name, int value) {
            auto* control = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, dialog));
            check(control, std::format("GUI mountain field missing: {}", name));
            control->SetValue(value);
        };
        auto* style = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-style", dialog));
        check(style, "GUI mountain terrain style missing");
        style->SetSelection(1);
        wxCommandEvent changed(wxEVT_CHOICE, style->GetId());
        changed.SetEventObject(style);
        style->GetEventHandler()->ProcessEvent(changed);
        spin("terrain-width", side); spin("terrain-height", side);
        spin("terrain-plateau-percent", percent); spin("terrain-plateau-levels", 3);
        auto* area = wxWindow::FindWindowByName("terrain-plateau-percent", dialog);
        wxSpinEvent areaChanged(wxEVT_SPINCTRL, area->GetId());
        areaChanged.SetEventObject(area);
        area->GetEventHandler()->ProcessEvent(areaChanged);
        auto* seed = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed", dialog));
        check(seed && seed->GetValue() == "1729", "GUI mountain seed fixture");
        GenerationConfig config{{100,100,7,side,side},1729};
        config.algorithm = GenerationAlgorithm::Surface;
        config.surface.shoreSmoothing = 2;
        config.surface.repairReach = 1;
        config.surface.plateauMinTop = percent ? 64 : 0;
        config.surface.plateauMaxPercent = percent;
        config.surface.plateauLevels = 3;
        GroundProfile model = profile;
        const auto brush = [&](const char* name) {
            auto* choice = dynamic_cast<wxComboBox*>(wxWindow::FindWindowByName(name, dialog));
            check(choice, std::format("GUI mountain brush field missing: {}", name));
            return std::string(choice->GetValue().utf8_str());
        };
        model.plateauGround = brush("terrain-plateau-ground");
        model.plateauCliff = brush("terrain-plateau-cliff");
        model.plateauRamp = brush("terrain-plateau-ramp");
        const auto snapshot = captureGenerationSnapshot(editor.map,config,coreBudget,{254016,64ULL*1024*1024});
        check(bool(snapshot), "GUI mountain reference snapshot");
        const auto keepClear = surfaceKeepClear(*snapshot,profileNeedsEdgeClearance(model));
        auto protection = snapshot->input.protection;
        for (size_t i = 0; i < protection.size(); ++i) protection[i] |= keepClear[i];
        const auto surface = generateSurface({config.region,config.seed,config.surface},snapshot->input.extent,surfaceBudget,
            snapshot->input.mask,protection);
        check(bool(surface), "GUI mountain reference surface");
        auto shapeConfig = config;
        shapeConfig.surface.plateauMinTop = 64; // Resolve fixture geometry even in the GUI-disabled case.
        const auto shapes = resolvePlateauBrushes(model,shapeConfig);
        check(bool(shapes), "GUI mountain reference ramp shapes");
        std::vector<PlateauGrid> caps;
        if (percent) {
            auto first = generatePlateau(*surface,config.seed,64,shapes->shapes,{}, {},percent);
            check(first && first->components, "GUI mountain reference has no first cap");
            caps.push_back(std::move(*first));
            for (uint32_t level = 2; level <= 3; ++level) {
                auto next = generateNextPlateau(caps.back(),config.seed ^ (uint64_t(level)<<48),shapes->shapes,{},percent);
                check(bool(next), "GUI mountain reference upper cap");
                if (!next->components) break;
                caps.push_back(std::move(*next));
            }
            check(caps.size() >= 2, "GUI mountain fixture needs cliff ground on an upper floor");
        }
        MountainDialogRun run(*dialog,editor,caps.size()+1);
        const int modal = dialog->ShowModal();
        run.stop();
        check(modal == wxID_OK && run.placed(), "GUI mountain flow failed: " + run.failure());
        check(!wxWindow::FindWindowByName("terrain-plateau-min-top", dialog), "Obsolete mountain cell-count input remains");
        auto* levels = wxWindow::FindWindowByName("terrain-plateau-levels", dialog);
        check(area->IsEnabled() && levels && levels->IsEnabled() == (percent > 0), "GUI mountain percentage control state");
        check(editor.actionQueue->getSize() == caps.size()+1, "GUI mountain history floor count");
        const auto* grass = g_brushes.getBrush(model.plateauGround)->as<GroundBrush>();
        const auto* cliff = g_brushes.getBrush(model.plateauCliff)->as<GroundBrush>();
        const auto hasItem = [](const Tile* tile, uint16_t id) {
            return tile && ((tile->ground && tile->ground->getID() == id) ||
                std::ranges::any_of(tile->items,[&](const auto& item){return item->getID() == id;}));
        };
        uint64_t previousArea = uint64_t(side)*side;
        for (size_t level = 0; level < caps.size(); ++level) {
            uint64_t actualArea = 0, upperCliffs = 0;
            const auto& cap = caps[level];
            for (size_t i = 0; i < cap.cells.size(); ++i) {
                const Position position(100+int(i%side),100+int(i/side),6-int(level));
                const auto* tile = editor.map.getTile(position);
                const auto* actual = tile && tile->ground ? brushOf(tile->ground->getID()) : nullptr;
                const bool top = cap.cells[i] == PlateauKind::Top;
                check((actual == grass || actual == cliff) == top, "GUI mountain cap footprint differs from reference");
                if (!top) continue;
                ++actualArea; upperCliffs += actual == cliff;
                const auto* support = editor.map.getTile(Position(position.x,position.y,position.z+1));
                check(support && support->ground && brushOf(support->ground->getID()) == cliff,
                    "GUI mountain cap has no cliff ground below it");
            }
            check(actualArea*100 <= previousArea*percent, "GUI mountain percentage denominator");
            if (!level) check(actualArea*100 > uint64_t(std::ranges::count(surface->cells,SurfaceKind::Rock))*percent,
                "GUI mountain percentage was incorrectly limited to rock area");
            if (level+1 < caps.size()) check(upperCliffs > 0, "GUI mountain cliff ground missing on upper floor");
            for (const auto& ramp : cap.ramps) {
                const Position head(100+int(ramp.cell%side),100+int(ramp.cell/side),7-int(level));
                const auto& ids = shapes->items[ramp.shape];
                const auto* opening = editor.map.getTile(Position(head.x,head.y,head.z-1));
                check(hasItem(editor.map.getTile(head),ids.ramp) && opening && opening->ground && opening->ground->getID() == ids.opening,
                    "GUI mountain ramp or upper opening missing");
            }
            report << std::format("SURFACE_MOUNTAIN_GUI_FLOOR_PASS\tpercent={}\tz={}\tcap={}\tcliff={}\tramps={}\n",
                percent,6-int(level),actualArea,upperCliffs,cap.ramps.size());
            previousArea = actualArea;
        }
        if (!percent) for (int z = 4; z < 7; ++z) for (uint32_t y = 100; y < 400; ++y) for (uint32_t x = 100; x < 400; ++x)
            check(!editor.map.getTile(x,y,z), "Zero GUI percentage generated an upper floor");
        std::vector<std::vector<uint32_t>> payloads;
        for (int z = 4; z <= 7; ++z) payloads.push_back(stackImage(editor.map,100,100,z,side,side));
        for (size_t i = 0; i <= caps.size(); ++i) check(editor.actionQueue->undo(), "GUI mountain Undo");
        check(editor.map.getTileCount() == 0, "GUI mountain Undo left tiles");
        for (size_t i = 0; i <= caps.size(); ++i) check(editor.actionQueue->redo(), "GUI mountain Redo");
        for (int z = 4; z <= 7; ++z) check(stackImage(editor.map,100,100,z,side,side) == payloads[size_t(z-4)], "GUI mountain Redo payload changed");
        editor.map.clearChanges();
        dialog->Destroy();
        g_gui.CloseCurrentEditor();
        report << std::format("SURFACE_MOUNTAIN_GUI_PERCENT_PASS\tseed=1729\tside=300\tpercent={}\tfloors={}\n",percent,caps.size()+1);
    }
}

void plateaus(std::ostream& report, uint32_t levels = 1)
{
    UndoLimits limits;
    check(g_gui.NewMap(), "Plateau map creation");
    auto& editor = *g_gui.GetCurrentEditor();
    constexpr uint32_t side = 256;
    GenerationConfig config {{100, 100, 7, side, side}, 1729};
    config.algorithm = GenerationAlgorithm::Surface;
    config.surface.rockPermille = 300;
    config.surface.plateauMinTop = 64;
    config.surface.plateauLevels = levels;
    config.vegetation = true;
    GroundProfile profile {
        {"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount
    };
    profile.decorCount = 3;
    profile.decor = {"small rocks", "small rocks", "small rocks"};
    profile.plateauGround = "grass";
    profile.plateauRamp = "ramp";
    config.surface.caveEntrances = 20;
    profile.caveEntrance = "cave enter";
    profile.caveExit = "ramp";
    profile.caveGround = "cave";
    profile.caveWall = "mountain";
    constexpr PreparationBudget preparation {250000, 254016, 750000, 768ULL * 1024 * 1024};
    constexpr TerrainCommitBudget commit {750000, 1536ULL * 1024 * 1024};
    auto prepared = prepareTerrain(editor.map, config, profile, preparation);
    check(
        bool(prepared),
        std::format(
            "Plateau prepare domain {} error {}", prepared ? 0 : prepared.error().index(),
            prepared ? 0 : std::visit([](auto e) { return int(e); }, prepared.error())
        )
    );
    check(prepared->plateau && !prepared->plateau->preview.getChanges().empty(), "Plateau preview empty");
    check(prepared->cave && !prepared->cave->preview.getChanges().empty(), "Combined cave empty");
    if (levels == 3) {
        auto steps = prepareTerrainSteps(editor.map, config, profile, preparation, {});
        check(bool(steps), "Staged mountain preflight");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
        std::optional<std::expected<PreparedTerrain, PreparationError>> result;
        while (!(result = steps->advance())) {
            check(std::chrono::steady_clock::now() < deadline, "Staged mountain timeout");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(bool(*result), "Staged mountain preparation");
        check((**result).cave && (**result).plateau, "Staged mountain/cave floors missing");
        const auto same = [](const PreparedTerrain& a, const PreparedTerrain& b) {
            check(a.plan.fingerprint() == b.plan.fingerprint() && a.preview.getFingerprint() == b.preview.getFingerprint(),
                "Worker changed a floor's full plan or preview payload");
        };
        same(*prepared, **result);
        same(*prepared->cave, *(**result).cave);
        auto* a = prepared->plateau.get();
        auto* b = (**result).plateau.get();
        for (; a && b; a = a->plateau.get(), b = b->plateau.get()) same(*a, *b);
        check(!a && !b && editor.map.getTileCount() == 0 && editor.actionQueue->getSize() == 0,
            "Worker changed mountain count, source or history");
        report << "SURFACE_MOUNTAIN_WORKER_PAYLOAD_PASS\n";
    }
    check(bool(validateTerrainTarget(*prepared, config, profile)), "Plateau validation");
    std::vector<const PreparedTerrain*> allUnits {&*prepared};
    for (auto* p = prepared->plateau.get(); p; p = p->plateau.get()) {
        allUnits.push_back(p);
    }
    check(allUnits.size() == levels + 1, "Requested mountain levels missing");
    allUnits.push_back(prepared->cave.get());
    std::vector<int> history;
    for (auto i = allUnits.rbegin() + 1; i != allUnits.rend() - 1; ++i) {
        history.push_back((*i)->plan.config.region.z);
    }
    history.push_back(8);
    history.push_back(7);
    const auto& upper = *prepared->plateau;
    size_t top = 0, landing = 0, ramps = 0, decor = 0;
    for (size_t i = 0; i < upper.plan.items.size(); ++i) {
        const Position pos(100 + int(i % side), 100 + int(i / side), 6);
        auto* tile = upper.preview.getBuffer().getTile(pos);
        auto kind = upper.plan.plateauFloor[i];
        check(bool(tile && tile->ground) == bool(kind), "Plateau ground occupancy");
        if (kind && kind != 3) {
            check(brushOf(tile->ground->getID()) == g_brushes.getBrush(kind == 2 ? profile.plateauCliff : profile.plateauGround), "Plateau ground brush");
        }
        top += kind == 1;
        landing += kind == 3;
        if (upper.plan.decor[i]) {
            ++decor;
            check(kind == 1, "Plateau decor outside Top");
        }
        ramps += prepared->plan.plateauRamps[i] != 0;
    }
    check(top && landing && ramps, "Plateau requires grass opening and ramps");
    const auto* rampBrush = g_brushes.getBrush(profile.plateauRamp)->as<DoodadBrush>();
    std::vector<uint16_t> forbidden;
    for (int v = 0; v < rampBrush->getTotalVariants(); ++v) {
        const auto variant = rampBrush->getVariant(v);
        for (const auto& [pos, items] : *variant.composite) {
            if (pos.z == -1) {
                for (const auto& item : items) {
                    forbidden.push_back(item->getID());
                }
            }
        }
    }
    for (const auto* lower : allUnits) {
        if (!lower->plateau) continue;
        const auto& cap = *lower->plateau;
        for (size_t i = 0; i < cap.plan.items.size(); ++i) {
            const auto kind = cap.plan.plateauFloor[i];
            Position pos(100 + int(i % side), 100 + int(i / side), cap.plan.config.region.z);
            const auto* tile = cap.preview.getBuffer().getTile(pos);
            if (kind == 3) {
                check(tile && tile->ground && std::ranges::find(forbidden, tile->ground->getID()) != forbidden.end(), "Mountain opening from template");
            } else if (kind) {
                auto below = pos; ++below.z;
                const auto* support = lower->preview.getBuffer().getTile(below);
                check(support && support->ground && brushOf(support->ground->getID()) == g_brushes.getBrush(profile.plateauCliff), "Same xy cliff under cap");
            }
        }
    }
    for (size_t i = 0; i < prepared->plan.plateauRamps.size(); ++i) {
        if (auto id = prepared->plan.plateauRamps[i]) {
            const Position pos(100 + int(i % side), 100 + int(i / side), 7);
            const auto* tile = prepared->preview.getBuffer().getTile(pos);
            check(
                tile && std::ranges::count_if(tile->items, [id](const auto& item) { return item->getID() == id; }) == 1,
                "Missing plateau ramp or tail"
            );
        }
    }
    for (const auto& pos : upper.preview.getChanges()) {
        check(
            pos.x >= 100 && pos.y >= 100 && pos.x < 100 + int(side) && pos.y < 100 + int(side) && pos.z == 6, "Plateau outside rectangle"
        );
    }
    for (const auto* unit : allUnits) {
        const auto& region = unit->plan.config.region;
        for (size_t i = 0; i < unit->plan.items.size(); ++i) {
            Position pos(region.x + int(i % side), region.y + int(i / side), region.z);
            const auto* tile = unit->preview.getBuffer().getTile(pos);
            if (!unit->plan.plateauFloor.empty()) {
                check(bool(tile && tile->ground) == bool(unit->plan.plateauFloor[i]), "All levels ground footprint");
                if (tile && tile->ground && unit->plan.plateauFloor[i] != 3) {
                    check(brushOf(tile->ground->getID()) == g_brushes.getBrush(unit->plan.plateauFloor[i] == 2 ? profile.plateauCliff : profile.plateauGround), "All levels ground brush");
                }
            }
            if (!unit->plan.plateauRamps.empty() && unit->plan.plateauRamps[i]) {
                auto id = unit->plan.plateauRamps[i];
                check(
                    tile && std::ranges::count_if(tile->items, [id](const auto& item) { return item->getID() == id; }) == 1,
                    "All levels ramp/tail"
                );
            }
        }
        for (const auto& pos : unit->preview.getChanges()) {
            check(
                pos.x >= 100 && pos.y >= 100 && pos.x < 100 + int(side) && pos.y < 100 + int(side) && pos.z == region.z,
                "All levels rectangle"
            );
        }
    }
    std::array<std::vector<uint32_t>, 5> before, after;
    for (int z = 4; z <= 8; ++z) {
        before[z - 4] = stackImage(editor.map, 97, 97, z, side + 6, side + 6);
    }
    auto prior = editor.actionQueue->getCurrentIndex();
    g_settings.setInteger(Config::UNDO_SIZE, int(history.size() - 1));
    auto refused = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
    g_settings.setInteger(Config::UNDO_SIZE, 1000);
    check(!refused, "Plateau last-step failure not triggered");
    check(editor.actionQueue->getCurrentIndex() == prior, "Plateau rollback history");
    for (int z = 4; z <= 8; ++z) {
        check(stackImage(editor.map, 97, 97, z, side + 6, side + 6) == before[z - 4], "Plateau rollback map");
    }
    check(bool(validateTerrainTarget(*prepared, config, profile)), "Plateau rollback source");
    auto receipt = DrawOperations::commitTerrainPreview(*prepared, config, profile, commit);
    check(bool(receipt), "Plateau commit");
    check(editor.actionQueue->getCurrentIndex() == prior + history.size(), "Plateau history steps");
    for (const auto* unit : allUnits) {
        for (const auto& pos : unit->preview.getChanges()) {
            check(
                snapshotTilesEqual(editor.map.getTile(pos), unit->preview.getBuffer().getTile(pos)), "Plateau committed payload mismatch"
            );
        }
    }
    for (int z = 4; z <= 8; ++z) {
        after[z - 4] = stackImage(editor.map, 97, 97, z, side + 6, side + 6);
    }
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        const int z = *it;
        check(editor.actionQueue->undo(), "Plateau undo");
        check(stackImage(editor.map, 97, 97, z, side + 6, side + 6) == before[z - 4], "Plateau undo floor");
    }
    for (int z : history) {
        check(editor.actionQueue->redo(), "Plateau redo");
        check(stackImage(editor.map, 97, 97, z, side + 6, side + 6) == after[z - 4], "Plateau redo floor");
    }
    check(editor.actionQueue->undo() && editor.actionQueue->undo(), "Leave only upper floor for occupied refusal");
    if (levels > 1) {
        check(editor.actionQueue->undo(), "Leave z-2 occupied and z-1 empty");
    }
    auto occupied = prepareTerrain(editor.map, config, profile, preparation);
    check(
        !occupied && std::holds_alternative<PreparationInputError>(occupied.error())
            && std::get<PreparationInputError>(occupied.error()) == PreparationInputError::UpperFloorOccupied,
        "Occupied upper floor accepted"
    );
    editor.map.clearChanges();
    check(g_gui.NewMap(), "Plateau refusal map");
    auto& empty = g_gui.GetCurrentEditor()->map;
    if (levels > 1) {
        auto low = config;
        low.region.z = 2;
        auto refused = prepareTerrain(empty, low, profile, preparation);
        check(
            !refused && std::holds_alternative<PreparationInputError>(refused.error())
                && std::get<PreparationInputError>(refused.error()) == PreparationInputError::NoUpperFloor,
            "Insufficient z range accepted"
        );
    }
    for (int variant = 0; variant < 6; ++variant) {
        auto bad = profile;
        if (variant == 0) {
            bad.plateauGround.clear();
        }
        if (variant == 1) {
            bad.plateauGround = "small rocks";
        }
        if (variant == 2) {
            bad.plateauRamp.clear();
        }
        if (variant == 3) {
            bad.plateauRamp = "small rocks";
        }
        if (variant == 4) bad.plateauCliff.clear();
        if (variant == 5) bad.plateauCliff = "small rocks";
        check(!prepareTerrain(empty, config, bad, preparation), "Invalid plateau profile accepted");
    }
    auto aloneConfig = config;
    aloneConfig.surface.caveEntrances = 0;
    auto aloneProfile = profile;
    aloneProfile.caveEntrance.clear(); aloneProfile.caveExit.clear(); aloneProfile.caveGround.clear(); aloneProfile.caveWall.clear();
    aloneProfile.count = GroundProfile::surfaceCount;
    aloneProfile.brushes[6].clear(); aloneProfile.brushes[7].clear();
    auto alone = prepareTerrain(empty, aloneConfig, aloneProfile, preparation);
    check(alone && alone->plateau && !alone->cave, "Standalone plateau preparation");
    auto& aloneEditor = *g_gui.GetCurrentEditor();
    const auto aloneBefore = aloneEditor.actionQueue->getCurrentIndex();
    check(bool(DrawOperations::commitTerrainPreview(*alone, aloneConfig, aloneProfile, commit)), "Standalone plateau commit");
    check(aloneEditor.actionQueue->getCurrentIndex() == aloneBefore + levels + 1, "Standalone plateau history count");
    for (uint32_t i = 0; i <= levels; ++i) {
        check(aloneEditor.actionQueue->undo(), "Standalone plateau undo");
    }
    for (uint32_t i = 0; i <= levels; ++i) {
        check(aloneEditor.actionQueue->redo(), "Standalone plateau redo");
    }
    aloneEditor.map.clearChanges();
    report << std::format(
        "{}\tgrass={}\topening={}\trampItems={}\tdecor={}\tsteps={}\n",
        levels == 1 ? "SURFACE_PLATEAU_E2E_PASS" : "SURFACE_PLATEAU_LEVELS_E2E_PASS", top, landing, ramps, decor, history.size()
    );
}

void levelRetryProbe(std::ostream& report) {
    check(g_gui.NewMap(), "Retry dialog map");
    auto* dialog = new TerrainGeneratorDialog(g_gui.root);
    const auto spin = [&](const char* name) {
        auto* control = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, dialog));
        check(control != nullptr, std::format("Missing {}", name)); return control;
    };
    auto* style = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-style", dialog));
    style->SetSelection(1);
    wxCommandEvent changed(wxEVT_CHOICE, style->GetId()); changed.SetEventObject(style);
    style->GetEventHandler()->ProcessEvent(changed);
    spin("terrain-width")->SetValue(100); spin("terrain-height")->SetValue(100);
    spin("terrain-plateau-percent")->SetValue(10);
    spin("terrain-plateau-levels")->SetValue(1);
    spin("terrain-cave-entrances")->SetValue(1);
    spin("terrain-building-density")->SetValue(0);
    for (const char* name : {"terrain-walls", "terrain-vegetation", "terrain-repair"}) {
        auto* control = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByName(name, dialog));
        check(control != nullptr, std::format("Missing {}", name));
        control->SetValue(false);
    }
    auto* seed = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed", dialog));
    auto* preview = dynamic_cast<wxButton*>(wxWindow::FindWindowByName("terrain-preview", dialog));
    auto* place = dynamic_cast<wxButton*>(wxWindow::FindWindowByName("terrain-place", dialog));
    auto* status = dynamic_cast<wxStaticText*>(wxWindow::FindWindowByName("terrain-status", dialog));
    check(seed && preview && place && status, "Retry dialog controls");
    const auto run = [&](uint64_t value, int retries) {
        seed->ChangeValue(wxstr(std::format("{}", value)));
        spin("terrain-retry-seeds")->SetValue(retries);
        wxCommandEvent click(wxEVT_BUTTON, preview->GetId()); click.SetEventObject(preview);
        preview->GetEventHandler()->ProcessEvent(click);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
        while (!preview->IsEnabled()) {
            check(std::chrono::steady_clock::now() < deadline, "Retry dialog timeout");
            wxYieldIfNeeded();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(place->IsEnabled(), "Retry dialog preview failed: " + status->GetLabel().ToStdString());
        return !status->GetLabel().Contains("Unreachable:");
    };
    bool previousFailed = false, found = false;
    for (uint64_t value = 1; value <= 64 && !found; ++value) {
        const bool success = run(value, 0);
        if (success && previousFailed) {
            check(run(value - 1, 2), "Retry did not select reachable second seed");
            check(seed->GetValue() == wxstr(std::format("{}", value)) && status->GetLabel().Contains("2 attempts"), "Retry seed or attempt count");
            report << std::format("LEVEL_RETRY_E2E_PASS initial={} used={} attempts=2\n", value - 1, value);
            found = true;
        }
        previousFailed = !success;
    }
    check(found, "No failing/succeeding consecutive retry seeds in search range");
    dialog->Destroy();
}

void levelReachabilityProbe(std::ostream& report) {
    UndoLimits limits;
    check(g_gui.NewMap(), "Level reachability map");
    auto& editor = *g_gui.GetCurrentEditor();
    constexpr uint32_t side = 256;
    GenerationConfig config {{100, 100, 7, side, side}, 1729};
    config.algorithm = GenerationAlgorithm::Surface;
    config.vegetation = true;
    config.surface.rockPermille = 300;
    config.surface.buildingDensity = 30;
    config.surface.campCount = 4;
    config.surface.ruinPercent = 20;
    config.surface.caveEntrances = 20;
    config.surface.plateauMinTop = 64;
    config.surface.plateauLevels = 3;
    GroundProfile profile {{"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount};
    profile.wall = "stone wall"; profile.door = "Normal door";
    profile.decorCount = 3;
    profile.decor = {"trees", "bushes", "small rocks"};
    profile.plateauGround = "grass"; profile.plateauRamp = "ramp";
    profile.caveEntrance = "cave enter"; profile.caveExit = "ramp";
    profile.caveGround = "cave"; profile.caveWall = "mountain";
    constexpr PreparationBudget preparation {250000, 254016, 750000, 768ULL * 1024 * 1024};
    constexpr TerrainCommitBudget commit {750000, 1536ULL * 1024 * 1024};
    auto prepared = prepareTerrain(editor.map, config, profile, preparation);
    check(prepared && prepared->levels && prepared->cave && prepared->plateau, "Combined level preparation");
    check(bool(DrawOperations::commitTerrainPreview(*prepared, config, profile, commit)), "Level reachability commit");
    const auto& levels = *prepared->levels;
    const auto& model = levels.report;
    const size_t n = side * side;
    std::vector<const GroundPlan*> plans {&prepared->plan, &prepared->cave->plan};
    for (auto* cap = prepared->plateau.get(); cap; cap = cap->plateau.get()) plans.push_back(&cap->plan);
    check(plans.size() == 5, "Combined reachability probe needs three mountain floors");
    uint64_t open = 0, blocked = 0, mismatches = 0, transitions = 0;
    const auto blocking = [](const Tile* tile) {
        if (!tile) return false;
        if (tile->ground && tile->ground->isBlocking()) return true;
        return std::ranges::any_of(tile->items, [](const auto& item) { return item->isBlocking(); });
    };
    for (size_t f = 0; f < levels.plan.floors.size(); ++f) for (size_t i = 0; i < n; ++i) {
        const Position pos(100 + int(i % side), 100 + int(i / side), levels.plan.floors[f].z);
        const auto* tile = editor.map.getTile(pos);
        const bool modelOpen = model.walkable[f*n+i];
        const bool decorOrSlope = levels.blockingMask[f*n+i] ||
            (plans[f]->items[i] && g_item_definitions.get(plans[f]->items[i]).hasFlag(ItemFlag::Unpassable));
        const bool mismatch = modelOpen ? !tile || !tile->ground || blocking(tile) :
            decorOrSlope && tile && tile->ground && !blocking(tile);
        open += modelOpen;
        blocked += !modelOpen && decorOrSlope;
        if (mismatch && mismatches++ < 16) {
            report << std::format("LEVEL_MAP_MISMATCH at {},{},{} open={} ground={} blockers=", pos.x, pos.y, pos.z, modelOpen,
                tile && tile->ground ? tile->ground->getID() : 0);
            if (tile) for (const auto& item : tile->items) if (item->isBlocking()) report << item->getID() << ',';
            report << '\n';
        }
    }
    for (const auto& e : levels.plan.edges) {
        const auto& a = e.a;
        const Position pos(100 + int(a.cell % side), 100 + int(a.cell / side), levels.plan.floors[a.floor].z);
        const auto* tile = editor.map.getTile(pos);
        const auto floorchange = [](const auto& item) { return item && item->getDefinition().hasFlag(ItemFlag::FloorChange); };
        // Cave entrances and mountain heads are at edge.a; cave exit heads too.
        const bool present = tile && (floorchange(tile->ground) || std::ranges::any_of(tile->items, floorchange));
        transitions += present;
    }
    report << std::format("LEVEL_REACHABILITY_E2E_{} walkable={} blocked={} mismatches={} floorchange={}/{} floors={}\n",
        mismatches == 0 && transitions == levels.plan.edges.size() ? "PASS" : "FAIL", open, blocked, mismatches, transitions,
        levels.plan.edges.size(), levels.plan.floors.size());
    check(mismatches == 0 && transitions == levels.plan.edges.size(), "Level model and committed map disagree");
    editor.map.clearChanges();
}

void levelRepairProbe(std::ostream& report) {
    UndoLimits limits;
    uint32_t bridges = 0, removed = 0, waterBorders = 0, bankBorders = 0;
    for (const bool vertical : {false, true})
    for (const bool bridgeGround : {true, false}) {
        check(g_gui.NewMap(), "Repair probe map");
        auto& editor = *g_gui.GetCurrentEditor();
        constexpr uint32_t side = 24, n = side * side;
        GenerationConfig config {{100, 100, 7, side, side}, 37};
        config.algorithm = GenerationAlgorithm::Surface; config.vegetation = true;
        config.surface.repairReach = 1; config.surface.fordLength = 1;
        GroundProfile p {{"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount};
        p.bridgeBrush = bridgeGround ? "bridge" : ""; p.decorCount = 3; p.decor = {"trees", "bushes", "small rocks"};
        auto snapshot = captureGenerationSnapshot(editor.map, config, {n, 64ULL << 20}, {10000, 64ULL << 20});
        check(bool(snapshot), "Repair snapshot");
        const auto clear = surfaceKeepClear(*snapshot, profileNeedsEdgeClearance(p));
        LevelWorld world;
        world.surface.region = config.region; world.surface.cells.resize(n, SurfaceKind::Grass);
        world.decor.push_back({config.region, std::vector<DecorKind>(n)});
        const auto at = [&](uint32_t x, uint32_t y) { return vertical ? x * side + y : y * side + x; };
        for (uint32_t y = 0; y < side; ++y) {
            world.surface.cells[at(13, y)] = SurfaceKind::River;
            world.decor[0].cells[at(16, y)] = DecorKind::Tree;
        }
        for (size_t i = 0; i < n; ++i) if (clear[i]) { world.surface.cells[i] = SurfaceKind::Preserve; world.decor[0].cells[i] = DecorKind::None; }
        world.surface.cells[at(18, 11)] = SurfaceKind::Floor; world.surface.sites = {at(18, 11)};
        const auto repaired = repairLevelReach(world, false, false, {}, {true, true, true}, 1, bridgeGround);
        check(repaired && !repaired->unrepaired && repaired->bridges == (bridgeGround ? 2u : 0u) && repaired->removedDecor == 1, "Repair fixture path with clipped water-border shoulder");
        auto plan = resolveSurfacePlan(snapshot->input, world.surface, p, 64ULL << 20, {}, &world.decor[0], clear);
        check(bool(plan), "Repair resolution");
        auto preview = DrawOperations::prepareTerrainPreview(editor.map, *snapshot, *plan, {10000, 128ULL << 20});
        check(bool(preview), "Repair preview");
        PreparedTerrain prepared {std::move(*snapshot), std::move(*plan), std::move(*preview), {}};
        const auto before = stackImage(editor.map, 98, 98, 7, side + 4, side + 4);
        check(bool(DrawOperations::commitTerrainPreview(prepared, config, p, {10000, 512ULL << 20})), "Repair commit");
        const auto brush = resolveBridgeBrush("bridge");
        check(bool(brush), "Bridge brush");
        for (size_t i = 0; i < n; ++i) {
            const auto* tile = editor.map.getTile(Position(100 + int(i % side), 100 + int(i / side), 7));
            if (world.surface.cells[i] == SurfaceKind::Bridge) {
                check(tile && tile->ground && !tile->isBlocking() && tile->ground->getID() == (vertical ? brush->vertical : brush->horizontal), "Bridge ground/axis/walkability");
                ++bridges;
            }
            if (world.surface.cells[i] == SurfaceKind::Shore) check(tile && tile->ground && !tile->isBlocking(), "Fallback sand ford blocked after bordering");
            if (world.surface.cells[i] == SurfaceKind::River && tile) for (const auto& item : tile->items) waterBorders += item->isBorder();
            if (world.surface.cells[i] == SurfaceKind::Grass && tile) {
                const int x = int(i % side), y = int(i / side);
                bool besideWater = false;
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                    if (x + dx >= 0 && y + dy >= 0 && x + dx < int(side) && y + dy < int(side))
                        besideWater |= world.surface.cells[size_t(y + dy) * side + x + dx] == SurfaceKind::River;
                if (besideWater) for (const auto& item : tile->items) bankBorders += item->isBorder();
            }
            if ((vertical ? i / side : i % side) == 16 && !clear[i] && world.decor[0].cells[i] == DecorKind::None)
                check(!tile || std::ranges::none_of(tile->items, [](const auto& item) { return item->isBlocking(); }), "Removed tree still blocks path");
        }
        auto semantic = levelPlanOf(world.surface, false, false, nullptr, {}, {}, {}, world.decor);
        check(bool(semantic), "Repair semantic model");
        std::vector<uint8_t> mask(n);
        for (size_t i = 0; i < n; ++i) {
            const auto* tile = editor.map.getTile(Position(100 + int(i % side), 100 + int(i / side), 7));
            mask[i] = tile && ((tile->ground && tile->ground->isBlocking()) || std::ranges::any_of(tile->items, [](const auto& item) { return item->isBlocking(); }));
        }
        const auto reach = levelReachabilityOf(*semantic, {}, mask);
        check(reach && reach->allReached(), "Repaired committed model must reach POI");
        const auto after = stackImage(editor.map, 98, 98, 7, side + 4, side + 4);
        check(editor.actionQueue->undo() && stackImage(editor.map, 98, 98, 7, side + 4, side + 4) == before, "Repair Undo");
        check(editor.actionQueue->redo() && stackImage(editor.map, 98, 98, 7, side + 4, side + 4) == after, "Repair Redo");
        removed += repaired->removedDecor;
        editor.map.clearChanges();
    }
    check(g_gui.NewMap(), "Integrated repair map");
    auto& editor = *g_gui.GetCurrentEditor();
    GenerationConfig config {{100, 100, 7, 128, 128}, 1729};
    config.algorithm = GenerationAlgorithm::Surface; config.vegetation = true;
    config.surface.rockPermille = 300; config.surface.buildingDensity = 30;
    config.surface.campCount = 4; config.surface.ruinPercent = 20;
    config.surface.caveEntrances = 20; config.surface.plateauMinTop = 64;
    config.surface.plateauLevels = 3; config.surface.plateauMaxPercent = 0;
    config.surface.repairReach = 1; config.surface.fordLength = 48;
    GroundProfile p {{"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount};
    p.wall = "stone wall"; p.door = "Normal door";
    p.decorCount = 3; p.decor = {"trees", "bushes", "small rocks"};
    p.plateauGround = "grass"; p.plateauRamp = "ramp";
    p.caveEntrance = "cave enter"; p.caveExit = "ramp"; p.caveGround = "cave"; p.caveWall = "mountain";
    p.bridgeBrush = "bridge";
    auto prepared = prepareTerrain(editor.map, config, p, {250000, 254016, 750000, 768ULL << 20});
    check(prepared && prepared->levels && prepared->plateau && prepared->cave, "Integrated multi-floor repair preparation");
    size_t notches = 0;
    for (size_t i = 0; i < prepared->plan.entrances.size(); ++i) if (prepared->plan.entrances[i]) {
        const int x = int(i % 128), y = int(i / 128);
        bool besideCap = false;
        for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) {
            if (x + dx < 0 || y + dy < 0 || x + dx >= 128 || y + dy >= 128) continue;
            const auto j = size_t(y + dy) * 128 + x + dx;
            if (std::abs(dx) <= 1 && std::abs(dy) <= 1) {
                check(prepared->plateau->plan.plateauFloor[j] == 0, "Entrance notch must exclude cap and landing");
                check(brushOf(prepared->plan.items[j]) != g_brushes.getBrush(p.plateauCliff), "Cliff under entrance notch");
            } else besideCap |= prepared->plateau->plan.plateauFloor[j] == 1;
        }
        notches += besideCap;
    }
    check(notches > 0, "Integrated plan needs entrance beside a notched cap");
    check(bool(DrawOperations::commitTerrainPreview(*prepared, config, p, {750000, 1536ULL << 20})), "Integrated repair commit");
    const auto& levels = *prepared->levels;
    for (size_t f = 0; f < levels.plan.floors.size(); ++f) for (size_t i = 0; i < 128 * 128; ++i) {
        const auto* tile = editor.map.getTile(Position(100 + int(i % 128), 100 + int(i / 128), levels.plan.floors[f].z));
        const bool blocks = tile && ((tile->ground && tile->ground->isBlocking()) ||
            std::ranges::any_of(tile->items, [](const auto& item) { return item->isBlocking(); }));
        check(bool(levels.blockingMask[f * 128 * 128 + i]) == blocks, "Integrated repair mask/map mismatch");
        if (levels.report.walkable[f * 128 * 128 + i]) check(tile && tile->ground && !blocks, "Integrated repaired walkability mismatch");
    }
    const auto& r = levels.repair;
    size_t entranceEdges = 0;
    for (const auto& poi : levels.plan.pois) if (poi.kind == LevelPoiKind::CaveEntrance) {
        const auto i = poi.at.cell;
        const auto* tile = editor.map.getTile(Position(100 + int(i % 128), 100 + int(i / 128), 7));
        check(tile && std::ranges::any_of(tile->items, [&](const auto& item) { return item->getID() == prepared->plan.entrances[i]; }),
            "Committed notch entrance item");
        for (const auto& edge : levels.plan.edges) if (edge.a == poi.at) {
            check(levels.report.walkable[edge.a.floor * 128 * 128 + edge.a.cell] &&
                levels.report.walkable[edge.b.floor * 128 * 128 + edge.b.cell], "Notch entrance Z edge blocked");
            ++entranceEdges;
        }
    }
    check(entranceEdges > 0, "Notch entrance transition missing");
    const auto floorImage = [&](int z) { return stackImage(editor.map, 98, 98, z, 132, 132); };
    std::vector<decltype(floorImage(7))> after;
    for (const auto& floor : levels.plan.floors) after.push_back(floorImage(floor.z));
    for (size_t f = 0; f < levels.plan.floors.size(); ++f) check(editor.actionQueue->undo(), "Notch Undo");
    check(editor.map.getTileCount() == 0, "Notch Undo must restore empty map");
    for (size_t f = 0; f < levels.plan.floors.size(); ++f) check(editor.actionQueue->redo(), "Notch Redo");
    for (size_t f = 0; f < levels.plan.floors.size(); ++f) check(floorImage(levels.plan.floors[f].z) == after[f], "Notch Redo payload");
    report << std::format("LEVEL_ENTRANCE_NOTCH_E2E_PASS notches={} entrances={} undoRedo={}\n", notches, entranceEdges, after.size());
    check(bankBorders > 0, "Water coastline borders disappeared beside the crossings");
    report << std::format("LEVEL_REPAIR_E2E_PASS bridges={} removedDecor={} axes=2 waterBorderItems={} bankBorderItems={} undoRedo=4 sandFords=2 integratedFloors={} repairs={} removedCaps={} removedPois={} unrepaired={}\n",
        bridges, removed, waterBorders, bankBorders, levels.plan.floors.size(), r.repairs, r.removedCaps, r.removedPois, r.unrepaired);
    editor.map.clearChanges();
}

void finalMaskRepairProbe(std::ostream& report) {
    UndoLimits undoLimits;
    // A private, generated m19 tuning fixture: no dependency on a user's saved
    // profiles or any item IDs. The second run exercises the unmodified defaults.
    const auto ids = loadedClientAssetIds();
    check(bool(ids), "Final-mask fixture client assets");
    StyleProfile fixture;
    fixture.client = ids->client; fixture.otbSha256 = ids->otbSha256; fixture.datSha256 = ids->datSha256;
    fixture.tuning = SurfaceTuning{192, 300, 12, 8, 20, 8, 240, 8, 300, 6000, 8};
    fixture.decorTuning = DecorTuning{565, 17, 369, 239, 23, 29};
    fixture.palette = {"grass", "dirt", "mountain", "sand", "sea"};
    fixture.decorPalette = {"trees", "grass tufts", "small rocks"};
    const auto encoded = serializeProfile(fixture);
    check(bool(encoded), "Final-mask fixture serialization");
    const auto directory = styleProfileDirectory();
    const auto relative = std::filesystem::weakly_canonical(directory).lexically_relative(std::filesystem::current_path().parent_path());
    check(!relative.empty() && *relative.begin() != "..", "Final-mask profile must stay in private probe directory");
    std::filesystem::create_directories(directory);
    const auto file = directory / "i39-m19-fixture.profile";
    check(!std::filesystem::exists(file), "Final-mask fixture must not overwrite a profile");
    {std::ofstream out(file); out << *encoded; check(bool(out), "Final-mask profile write");}
    for (const bool regression : {true, false}) {
        check(g_gui.NewMap(), "Final-mask GUI map");
        auto& editor = *g_gui.GetCurrentEditor();
        size_t expectedActions = 0;
        if (regression) {
            GenerationConfig config{{1000, 1000, 7, 300, 300}, 1212};
            config.algorithm = GenerationAlgorithm::Surface; config.vegetation = true;
            config.surface = *fixture.tuning; config.decor = *fixture.decorTuning;
            config.surface.shoreSmoothing = 2; config.surface.repairReach = 1;
            config.surface.fordLength = 48; config.surface.caveEntrances = 10;
            config.surface.plateauMinTop = 64; config.surface.plateauMaxPercent = 10; config.surface.plateauLevels = 3;
            GroundProfile p{{"grass", "dirt", "mountain", "sand", "sea", "sea", "cobblestone", "stone floor"}, 8};
            p.decorCount = 3; p.decor = fixture.decorPalette;
            p.caveEntrance = "cave enter"; p.caveExit = "ramp"; p.caveGround = "cave"; p.caveWall = "mountain";
            p.plateauGround = "grass"; p.plateauRamp = "ramp"; p.bridgeBrush = "bridge";
            auto prepared = prepareTerrain(editor.map, config, p, {1000000, 1100000, 2000000, 8ULL << 30});
            check(prepared && prepared->levels && !prepared->levels->repair.unrepaired, "Final-mask repaired fixture");
            std::vector<const PreparedTerrain*> units{&*prepared};
            if (prepared->cave) units.push_back(prepared->cave.get());
            for (auto* cap = prepared->plateau.get(); cap; cap = cap->plateau.get()) units.push_back(cap);
            for (const auto* unit : units) expectedActions += !unit->preview.getChanges().empty();
            std::vector<uint8_t> rawMask(units.size() * 90000);
            size_t clearedBorders = 0;
            for (size_t f = 0; f < units.size(); ++f) {
                const auto* unit = units[f];
                auto raw = DrawOperations::prepareTerrainPreview(editor.map, unit->snapshot, unit->plan, {2000000, 2ULL << 30});
                check(bool(raw), "Final-mask raw bordered fixture");
                for (size_t i = 0; i < 90000; ++i) {
                    const Position pos(1000 + int(i % 300), 1000 + int(i / 300), unit->plan.config.region.z);
                    const auto* before = raw->getBuffer().getTile(pos);
                    const auto* after = unit->preview.getBuffer().getTile(pos);
                    if (!before) continue;
                    rawMask[f * 90000 + i] = (before->ground && before->ground->isBlocking()) ||
                        std::ranges::any_of(before->items, [](const auto& item) { return item->isBlocking(); });
                    for (const auto& item : before->items) if (item->isBlocking() && item->isBorder() && after &&
                        std::ranges::none_of(after->items, [&](const auto& other) { return other->getID() == item->getID(); })) {
                        ++clearedBorders;
                        if (clearedBorders == 1) report << std::format("FINAL_MASK_BLOCKER_DIAG position={},{},{} id={} name={} border=true resolvedGround={} resolvedDecor={}\n",
                            pos.x, pos.y, pos.z, item->getID(), item->getName(), unit->plan.items[i], unit->plan.decor.empty() ? 0 : unit->plan.decor[i]);
                    }
                }
            }
            const auto rawReach = levelReachabilityOf(prepared->levels->plan, {}, rawMask);
            check(rawReach && !rawReach->allReached() && prepared->levels->report.allReached() && clearedBorders,
                "Fixture must require actual post-autoborder repair");
            check(prepared->levels->repair.repairs <= 64, "Final-mask total repair budget");
            report << std::format("FINAL_MASK_RAW_REGRESSION_PASS blockingBordersCleared={} repairs={}\n", clearedBorders, prepared->levels->repair.repairs);
        }
        auto* dialog = new TerrainGeneratorDialog(g_gui.root);
        const auto spin = [&](const char* name, int value) {
            auto* control = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, dialog));
            check(control, std::format("Final-mask GUI field: {}", name)); control->SetValue(value);
        };
        const auto choose = [&](const char* name, int value) {
            auto* control = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName(name, dialog));
            check(control, std::format("Final-mask GUI choice: {}", name)); control->SetSelection(value);
            wxCommandEvent event(wxEVT_CHOICE, control->GetId()); event.SetEventObject(control);
            control->GetEventHandler()->ProcessEvent(event);
        };
        choose("terrain-style", 1);
        if (regression) {
            auto* profiles = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-profile", dialog));
            const int selected = profiles ? profiles->FindString("i39-m19-fixture") : wxNOT_FOUND;
            check(selected != wxNOT_FOUND, "Final-mask GUI profile offered");
            choose("terrain-profile", selected);
            spin("terrain-x", 1000); spin("terrain-y", 1000);
            spin("terrain-width", 300); spin("terrain-height", 300);
            spin("terrain-ford-length", 48);
            auto* vegetation = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByName("terrain-vegetation", dialog));
            check(vegetation, "Final-mask vegetation control"); vegetation->SetValue(true);
        }
        spin("terrain-cave-entrances", 10);
        spin("terrain-plateau-percent", 10); spin("terrain-plateau-levels", 3);
        auto* seed = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed", dialog));
        check(seed, "Final-mask seed field"); seed->SetValue("1212");
        MountainDialogRun run(*dialog, editor, 0, true);
        const int modal = dialog->ShowModal(); run.stop();
        check(modal == wxID_OK && run.placed(), "Final-mask GUI flow: " + run.failure());
        const auto x = regression ? 998 : 98, side = regression ? 304 : 104;
        std::vector<std::vector<uint32_t>> after;
        for (int z = 4; z <= 8; ++z) after.push_back(stackImage(editor.map, x, x, z, side, side));
        const auto actions = editor.actionQueue->getSize();
        check(expectedActions ? actions == expectedActions : actions > 0 && actions <= run.floors(), "Final repair must not add Undo actions");
        for (size_t f = 0; f < actions; ++f) check(editor.actionQueue->undo(), "Final-mask Undo");
        check(editor.map.getTileCount() == 0, "Final-mask Undo restores empty map");
        for (size_t f = 0; f < actions; ++f) check(editor.actionQueue->redo(), "Final-mask Redo");
        for (int z = 4; z <= 8; ++z) check(stackImage(editor.map, x, x, z, side, side) == after[size_t(z - 4)], "Final-mask identical Redo");
        report << std::format("FINAL_MASK_REPAIR_E2E_PASS seed=1212 regression={} undoRedo={} status={}\n", regression, actions, run.status());
        dialog->Destroy(); editor.map.clearChanges();
    }
    std::filesystem::remove(file);
}

void caveFarExitDoorGuiProbe(std::ostream& report, bool mountainPasses = false) {
    UndoLimits limits;
    check(g_gui.NewMap(), "I41 GUI map");
    auto& editor = *g_gui.GetCurrentEditor();
    auto* dialog = new TerrainGeneratorDialog(g_gui.root);
    auto* style = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-style", dialog));
    check(style, "I41 style control"); style->SetSelection(1);
    wxCommandEvent changed(wxEVT_CHOICE, style->GetId()); changed.SetEventObject(style);
    style->GetEventHandler()->ProcessEvent(changed);
    const auto spin = [&](const char* name, int value) {
        auto* c = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, dialog));
        check(c, std::format("I41 spin {}", name)); c->SetValue(value);
        wxSpinEvent event(wxEVT_SPINCTRL, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
    };
    constexpr uint32_t side = 256, n = side * side;
    spin("terrain-width", side); spin("terrain-height", side); spin("terrain-cave-entrances", 16);
    spin("terrain-plateau-percent", 10); spin("terrain-plateau-levels", 3);
    if (mountainPasses) spin("terrain-mountain-passes", 4);
    auto* seed = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed", dialog));
    check(seed, "I41 seed"); seed->SetValue("1001");
    auto* doors = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByName("terrain-cave-door-corridors", dialog));
    auto* distant = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByName("terrain-cave-far-exits", dialog));
    auto* brush = dynamic_cast<wxComboBox*>(wxWindow::FindWindowByName("terrain-cave-door-brush", dialog));
    check(doors && distant && brush && !doors->GetValue() && distant->GetValue(), "I41 option defaults");
    wxCommandEvent dropdown(wxEVT_COMBOBOX_DROPDOWN, brush->GetId()); dropdown.SetEventObject(brush);
    brush->GetEventHandler()->ProcessEvent(dropdown);
    check(brush->SetStringSelection("bridge"), "I41 doorless wall brush offered");
    wxCommandEvent brushChanged(wxEVT_COMBOBOX, brush->GetId()); brushChanged.SetEventObject(brush);
    brush->GetEventHandler()->ProcessEvent(brushChanged);
    check(!doors->IsEnabled() && !doors->GetValue(), "Missing door brush must disable corridors");
    check(brush->SetStringSelection("stone wall"), "I41 normal-door wall brush offered");
    brush->GetEventHandler()->ProcessEvent(brushChanged);
    check(doors->IsEnabled(), "Valid cave door brush must enable corridors");
    doors->SetValue(true);
    wxCommandEvent enabled(wxEVT_CHECKBOX, doors->GetId()); enabled.SetEventObject(doors); doors->GetEventHandler()->ProcessEvent(enabled);
    // Independent adapter preparation checks the concrete closed-door mask.
    GenerationConfig config{{100,100,7,side,side},1001}; config.algorithm = GenerationAlgorithm::Surface;
    config.surface.caveEntrances = 16; config.surface.caveDoorCorridors = true;
    config.surface.repairReach = 1; config.surface.shoreSmoothing = 2;
    config.surface.plateauMinTop = 64; config.surface.plateauMaxPercent = 10; config.surface.plateauLevels = 3;
    if (mountainPasses) config.surface.passes = 4;
    GroundProfile p{{"grass","dirt","rocky ground","sand","clear water","clear water","cobblestone","stone floor"},8};
    p.caveEntrance = "cave enter"; p.caveExit = "ramp"; p.caveGround = "cave"; p.caveWall = "mountain";
    p.caveDoor = "stone wall"; p.plateauGround = "grass"; p.plateauRamp = "ramp"; p.bridgeBrush = "bridge";
    auto prepared = prepareTerrain(editor.map, config, p, {1000000,1100000,2000000,8ULL << 30});
    check(prepared && prepared->levels && prepared->cave, "I41 adapter preparation");
    size_t maskDoors = 0;
    for (size_t i = 0; i < n; ++i) if (prepared->cave->plan.doors[i]) {
        ++maskDoors;
        check(!prepared->levels->blockingMask[n + i] && prepared->levels->report.walkable[n + i], "Closed door must be openable in adapter mask");
    }
    check(maskDoors > 0 && prepared->levels->report.allReached(), "I41 adapter reachable doors");
    std::vector<uint32_t> passCells;
    if (mountainPasses) {
        check(!prepared->levels->passes.empty(), "I42 adapter must produce a pass");
        for (const auto& pass : prepared->levels->passes) for (auto i : pass.cells) {
            check(prepared->levels->report.walkable[i] && !prepared->levels->blockingMask[i], "I42 final adapter pass mask");
            for (const auto* unit = prepared->plateau.get(); unit; unit = unit->plateau.get()) {
                const auto* tile = unit->preview.getBuffer().getTile(Position(100+int(i%side),100+int(i/side),unit->plan.config.region.z));
                check(!tile || (!tile->ground && tile->items.empty()), "I42 empty tile above pass");
            }
            passCells.push_back(i);
        }
        auto fallbackConfig = config;
        fallbackConfig.surface.caveEntrances = 0;
        fallbackConfig.surface.caveDoorCorridors = false;
        fallbackConfig.surface.repairReach = 0;
        auto fallbackProfile = p;
        fallbackProfile.count = GroundProfile::surfaceCount;
        fallbackProfile.brushes[6].clear(); fallbackProfile.brushes[7].clear();
        fallbackProfile.caveEntrance.clear(); fallbackProfile.caveExit.clear(); fallbackProfile.caveGround.clear();
        fallbackProfile.caveWall.clear(); fallbackProfile.caveDoor.clear();
        auto fallback = prepareTerrain(editor.map, fallbackConfig, fallbackProfile, {1000000,1100000,2000000,8ULL << 30});
        check(fallback && fallback->levels && !fallback->levels->passes.empty(), "I42 passes without repair or path brush");
        for (const auto& pass : fallback->levels->passes) for (auto i : pass.cells)
            check(brushOf(fallback->plan.items[i]) == g_brushes.getBrush("dirt"), "I42 Dirt fallback");
    }
    prepared = std::unexpected(PreparationError{PreparationInputError::Cancelled});
    MountainDialogRun run(*dialog, editor, 0, true);
    const int modal = dialog->ShowModal(); run.stop();
    check(modal == wxID_OK && run.placed(), "I41 GUI flow: " + run.failure());
    const auto resolved = resolveCaveDoorBrush("stone wall"); check(bool(resolved), "I41 door resolution");
    size_t placedDoors = 0, farExits = 0;
    const auto open = [&](int x, int y) {
        const auto* t = editor.map.getTile(Position(x, y, 8));
        return t && t->ground && !t->ground->isBlocking() && std::ranges::none_of(t->items,
            [](const auto& item) { return item->isBlocking() && !item->isBrushDoor(); });
    };
    for (uint32_t i = 0; i < n; ++i) {
        const int x = 100 + int(i % side), y = 100 + int(i / side);
        const auto* tile = editor.map.getTile(Position(x, y, 8));
        if (!tile) continue;
        for (const auto& item : tile->items) {
            if (item->isBrushDoor()) {
                ++placedDoors;
                check(!item->isOpen() && (item->getID() == resolved->horizontal || item->getID() == resolved->vertical), "I41 closed unlocked door item");
                const bool west = open(x-1,y), east = open(x+1,y), north = open(x,y-1), south = open(x,y+1);
                check((west && east && !north && !south) || (north && south && !west && !east), "I41 committed door neighbours");
            }
            if (item->getDefinition().hasFlag(ItemFlag::FloorChange) && !item->getDefinition().hasFlag(ItemFlag::FloorChangeDown)) {
                const auto* upper = editor.map.getTile(Position(x,y,7));
                const auto down = [](const auto& v) { return v && v->getDefinition().hasFlag(ItemFlag::FloorChangeDown); };
                if (upper && upper->ground && !down(upper->ground) && std::ranges::none_of(upper->items, down)) ++farExits;
            }
        }
    }
    check(placedDoors > 0 && farExits > 0, "I41 GUI needs doors and exit without surface hole");
    for (auto i : passCells) {
        const int x = 100 + int(i%side), y = 100 + int(i/side);
        const auto* tile = editor.map.getTile(Position(x,y,7));
        check(tile && tile->ground && !tile->ground->isBlocking() &&
            std::ranges::none_of(tile->items,[](const auto& item){return item->isBlocking();}), "I42 committed pass walkable");
        for (int z = 4; z <= 6; ++z) {
            const auto* upper = editor.map.getTile(Position(x,y,z));
            check(!upper || (!upper->ground && upper->items.empty()), "I42 no item above committed pass");
        }
    }
    std::vector<std::vector<uint32_t>> after;
    for (int z=4; z<=8; ++z) after.push_back(stackImage(editor.map,98,98,z,side+4,side+4));
    const auto actions = editor.actionQueue->getSize();
    for (size_t i=0; i<actions; ++i) check(editor.actionQueue->undo(), "I41 Undo");
    check(editor.map.getTileCount() == 0, "I41 Undo restores empty map");
    for (size_t i=0; i<actions; ++i) check(editor.actionQueue->redo(), "I41 Redo");
    for (int z=4; z<=8; ++z) check(after[z-4] == stackImage(editor.map,98,98,z,side+4,side+4), "I41 identical Redo");
    if (mountainPasses)
        report << std::format("MOUNTAIN_PASS_GUI_PASS cells={} farExits={} doors={} maskDoors={} undoRedo={}\n",
            passCells.size(),farExits,placedDoors,maskDoors,actions);
    else
        report << std::format("CAVE_FAR_EXIT_DOOR_GUI_PASS farExits={} doors={} maskDoors={} undoRedo={} brush=stone wall\n",farExits,placedDoors,maskDoors,actions);
    dialog->Destroy(); editor.map.clearChanges();
}

void run() {
    const auto root = std::filesystem::current_path();
    std::ofstream report(root / "resolver-probe.log");
	report << std::unitbuf;
	try {
		check(std::filesystem::is_regular_file(root / ".g1-resolver-probe"), "Private probe required");
		g_version.UnloadVersion();
		wxString error;
		std::vector<std::string> warnings;
		check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Private client load failed");
        caveFarExitDoorGuiProbe(report);
        caveFarExitDoorGuiProbe(report, true);
        finalMaskRepairProbe(report);
        mountainDialogControls(report);
        guiMountainPercentage(report);
        plateaus(report);
        plateaus(report, 3);
        levelReachabilityProbe(report);
        levelRepairProbe(report);
        levelRetryProbe(report);
		for (uint64_t seed = 1; seed <= 4; ++seed) oneRun(100, seed, report, seed == 1);
		oneRun(250, 1729, report, true);
		oneRun(500, 1729, report, false);
		endToEnd(100, report);
		endToEnd(250, report);
		endToEnd(100, report, true);
		endToEnd(250, report, true);
		structures(100, report);
		structures(250, report);
		doors(100, report);
		doors(250, report);
		ruinsAndCamps(250, report);
		campfires(250, report);
		guiCaves(report);
        const bool rockCaves = caves(256, CaveEntranceStyle::RockFace, report);
		const bool holeCaves = caves(256, CaveEntranceStyle::FloorHole, report);
		fords(250, 1729, report);
		nextToTerrain(100, 1, false, report);
		nextToTerrain(100, 1, true, report);
		nextToTerrain(250, 1729, false, report);
		check(rockCaves && holeCaves, "Cave probes need at least two entrances with exits per style");
		report << "SURFACE_RESOLVER_PASS\n";
		report.close();
		wxTheApp->CallAfter([] { g_gui.root->Close(); });
	} catch (const std::exception& error) {
		report << "FAILED\t" << error.what() << '\n';
		report.close();
		std::_Exit(9);
	}
}
}

class SurfaceResolverModule : public wxModule {
public:
	bool OnInit() override {
		wxTheApp->Bind(wxEVT_IDLE, &SurfaceResolverModule::OnIdle, this);
		return true;
	}
	void OnExit() override {}
	void OnIdle(wxIdleEvent& event) {
		event.Skip();
		if (dismissStartupWarningsDialog()) return;
		if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
		wxTheApp->Unbind(wxEVT_IDLE, &SurfaceResolverModule::OnIdle, this);
		wxTheApp->CallAfter([] { run(); });
	}
private:
	wxDECLARE_DYNAMIC_CLASS(SurfaceResolverModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(SurfaceResolverModule, wxModule);
