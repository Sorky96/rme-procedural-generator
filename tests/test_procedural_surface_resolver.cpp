// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only wxModule linked with the current RME objects, never into normal RME.
// Resolves surface v2 against the loaded 1098-BlackTek client and runs the existing
// GroundBorderCalculator over the result. It adds no border logic of its own.
#include "app/main.h"
#include "procedural_probe_warnings.h"
#include "procedural/adapter/generation_input.h"
#include "procedural/core/reachability.h"
#include "procedural/adapter/surface_resolver.h"
#include "procedural/adapter/ground_resolver.h"
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
#include "brushes/wall/wall_brush.h"
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

// Every item a cave entrance brush could place: each doodad alternative, or the hole ground's border items.
std::vector<uint16_t> entranceAlternatives(const std::string& name) {
	std::vector<uint16_t> ids;
	const auto* brush = g_brushes.getBrush(name);
	if (const auto* doodad = brush ? brush->as<DoodadBrush>() : nullptr) {
		for (int alternative = 0; alternative < int(doodad->getAlternativeCount()); ++alternative)
			if (const auto* item = doodad->getRandomSingleItem(alternative, 1)) ids.push_back(item->getID());
	} else if (const auto* ground = brush ? brush->as<GroundBrush>() : nullptr) {
		for (const auto& variant : ground->getBorderItems()) ids.push_back(variant.id);
	}
	return ids;
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
		// Iteration 48b: an ordinary fallback entrance off the rock is the floor hole.
		entrances[e] = style == CaveEntranceStyle::RockFace && grid->cells[e] != SurfaceKind::Rock && brushes->hole ? brushes->hole : brushes->entrance;
		ramps[exit.cell] = ids.ramp;
		if (shape.hasTail) ramps[size_t(int64_t(exit.cell) + shape.tailDx + int64_t(shape.tailDy) * side)] = ids.tail;
	}
	check(prepared->plan.entrances == entrances && prepared->cave->plan.exits == ramps,
		"Cave item layers differ from exit geometry");
	// Iteration 46 (D24): every rock-face entrance opens to the south, so it carries the brush's south-facing first
	// alternative; no other alternative (the east-facing hole) appears anywhere.
	auto alternatives = entranceAlternatives(profile.caveEntrance);
	check(std::ranges::find(alternatives, brushes->entrance) != alternatives.end(), "Entrance item is not from its brush");
	if (style == CaveEntranceStyle::RockFace) {
		check(alternatives.size() >= 2 && brushes->entrance == alternatives.front(), "Entrance is not the brush's first alternative");
		check(brushes->hole && brushOf(brushes->hole) == g_brushes.getBrush("cave hole"), "Fallback entrance is not the cave hole");
		alternatives.push_back(brushes->hole);
		for (const auto e : grid->entrances) {
			const auto open = grid->cells[e + side];
			check(open == SurfaceKind::Grass || open == SurfaceKind::Dirt || open == SurfaceKind::Shore || open == SurfaceKind::Path,
				"Rock-face entrance does not open to the south");
		}
	}
	for (const auto e : grid->entrances) check(caveEntranceFits(grid->cells, side, side, e, style), "Entrance violates style rule");
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
	size_t caveCells = 0, wallCells = 0, wallFloorNorthOrWestOnly = 0, wallWithoutFace = 0;
	std::map<uint16_t, size_t> wallBorders;
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
		// Iteration 46 (D24c): which border pieces the wall brush gives the wall band, and how many wall cells see the
		// cave floor only from the north or west (they would need a north or west face).
		if (wall) {
			for (const auto& item : tile->items) ++wallBorders[item->getID()];
			const auto floorAt = [&](int dx, int dy) {
				return cx + dx >= 0 && cy + dy >= 0 && cx + dx < int(side) && cy + dy < int(side) &&
					cave->cells[size_t(cy + dy) * side + size_t(cx + dx)] == CaveKind::Floor;
			};
			wallFloorNorthOrWestOnly += (floorAt(0, 1) || floorAt(1, 0) || floorAt(1, 1)) ? 0 : 1;
			// Iteration 48: a wall cell sees the floor (8-neighbourhood) by definition; it has a face when the brush
			// gave it a border piece.
			wallWithoutFace += std::ranges::none_of(tile->items, [](const auto& item) { return item->isBorder(); });
		}
		if (style == CaveEntranceStyle::RockFace && entrances[i] && entrances[i] == brushes->entrance) {
			check(prepared->plan.items[i] == plain->plan.items[i], "Rock entrance ground changed");
		}
		if (entrances[i] && (style == CaveEntranceStyle::FloorHole || entrances[i] == brushes->hole)) {
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
		checkItem(*prepared, upper, entrances[i], alternatives);
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
	check(!Item::Create(brushes->entrance)->isBlocking(), "Cave entrance blocks movement in client data");
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
		// Iteration 48: without rock the style falls back to ordinary entrances, so the cave is not empty.
		const auto fallback = prepareTerrain(emptyMap, noRock, noRockProfile, preparation);
		check(fallback && fallback->cave && !fallback->cave->preview.getChanges().empty() &&
			std::ranges::any_of(fallback->plan.entrances, [](uint16_t id) { return id != 0; }), "Expected ordinary entrances without rocks");
		// A checkerboard mask leaves no cell with four open neighbours, so no entrance at all and an empty cave.
		std::vector<uint8_t> checkerboard(size_t(side) * side);
		for (size_t i = 0; i < checkerboard.size(); ++i) checkerboard[i] = uint8_t((i % side + i / side) % 2);
		auto emptyCave = prepareTerrain(emptyMap, noRock, noRockProfile, preparation, checkerboard);
		check(emptyCave && emptyCave->cave && emptyCave->cave->preview.getChanges().empty(), "Expected an empty cave without entrance sites");
		auto* queue = g_gui.GetCurrentEditor()->actionQueue.get();
		const auto before = queue->getCurrentIndex();
		check(bool(DrawOperations::commitTerrainPreview(*emptyCave, noRock, noRockProfile, commit, checkerboard)), "Empty cave commit failed");
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
	std::string borderList;
	for (const auto& [id, count] : wallBorders) borderList += std::format("{}{}:{}", borderList.empty() ? "" : ",", id, count);
	report << std::format("CAVE_WALL_FACES style={} wall={} wallCells={} floorOnlyNorthOrWest={} withoutFace={} withoutFacePermille={} borders={}\n",
		style == CaveEntranceStyle::RockFace ? "rockface" : "floorhole", profile.caveWall, wallCells, wallFloorNorthOrWestOnly,
		wallWithoutFace, wallCells ? wallWithoutFace * 1000 / wallCells : 0, borderList);
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
		auto alternatives = entranceAlternatives(profile.caveEntrance);
		if (brushes->hole) alternatives.push_back(brushes->hole); // ordinary fallback entrances (iteration 48b)
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
				return std::ranges::find(alternatives, id) != alternatives.end();
			});
			if (entranceCount) check(prepared->plan.entrances[i] == brushes->entrance || (brushes->hole && prepared->plan.entrances[i] == brushes->hole),
				"GUI entrance is not the south-facing alternative");
			const auto rampCount = countIds(lower, [&](uint16_t id) {
				return std::ranges::any_of(brushes->items, [&](const auto& ids) { return ids.ramp == id; });
			});
			check(entranceCount == rampCount && entranceCount <= 1,
				std::format("GUI cave mismatch {} cell=({},{},{}) entrances={} ramps={} plannedEntrance={} plannedExit={}",
					combination, upper.x, upper.y, upper.z, entranceCount, rampCount,
					prepared->plan.entrances[i], prepared->cave->plan.exits[i]));
			if ((style == CaveEntranceStyle::FloorHole || (brushes->hole && prepared->plan.entrances[i] == brushes->hole)) && entranceCount)
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
    MountainDialogRun(TerrainGeneratorDialog& dialog, Editor& editor, size_t floors, bool requireReachable = false, bool previewOnly = false) : dialog_(dialog), editor_(editor), floors_(floors), requireReachable_(requireReachable), previewOnly_(previewOnly),
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
                if (previewOnly_) { phase_ = Phase::Placed; timer_.Stop(); dialog_.EndModal(wxID_CANCEL); return; }
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
    bool previewOnly_;
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
        // Closed, unlocked doors are walkable, as in the adapter's level mask.
        return std::ranges::any_of(tile->items, [](const auto& item) {
            return item->isBlocking() && !(item->isBrushDoor() && !item->isLocked());
        });
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
            mask[i] = tile && ((tile->ground && tile->ground->isBlocking()) || std::ranges::any_of(tile->items, [](const auto& item) { return item->isBlocking() && !(item->isBrushDoor() && !item->isLocked()); }));
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
            std::ranges::any_of(tile->items, [](const auto& item) { return item->isBlocking() && !(item->isBrushDoor() && !item->isLocked()); }));
        check(bool(levels.blockingMask[f * 128 * 128 + i]) == blocks, "Integrated repair mask/map mismatch");
        if (levels.report.walkable[f * 128 * 128 + i]) check(tile && tile->ground && !blocks, "Integrated repaired walkability mismatch");
    }
    const auto& r = levels.repair;
    size_t entranceEdges = 0;
    for (const auto& poi : levels.plan.pois) if (poi.kind == LevelPoiKind::CaveEntrance) {
        const auto i = poi.at.cell;
        const auto* tile = editor.map.getTile(Position(100 + int(i % 128), 100 + int(i / 128), 7));
        // An ordinary fallback entrance (iteration 48b) is the hole ground itself.
        check(tile && ((tile->ground && tile->ground->getID() == prepared->plan.entrances[i]) ||
            std::ranges::any_of(tile->items, [&](const auto& item) { return item->getID() == prepared->plan.entrances[i]; })),
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
    fixture.palette = {"grass", "dirt", "rocky ground", "sand", "sea"};
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
    uint64_t fixtureSeed = 1212;
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
            GroundProfile p{{"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, 8};
            p.decorCount = 3; p.decor = fixture.decorPalette;
            p.caveEntrance = "cave enter"; p.caveExit = "ramp"; p.caveGround = "cave"; p.caveWall = "mountain";
            p.plateauGround = "grass"; p.plateauRamp = "ramp"; p.bridgeBrush = "bridge";
            // Iteration 44: with the impassable m19 rock ("mountain") no seed needs this repair any more (POIs stay on the
            // start's land; that case is M19_ROCK_GUI_CASE). The fixture keeps the m19 tuning with a passable rock brush and
            // takes the first seed from 1212 on whose autoborders still cut a path.
            bool found = false;
            std::string last;
            for (uint64_t candidate = 1212; candidate < 1244 && !found; ++candidate) {
                config.seed = candidate;
                expectedActions = 0;
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
                            std::ranges::any_of(before->items, [](const auto& item) { return item->isBlocking() && !(item->isBrushDoor() && !item->isLocked()); });
                        for (const auto& item : before->items) if (item->isBlocking() && item->isBorder() && after &&
                            std::ranges::none_of(after->items, [&](const auto& other) { return other->getID() == item->getID(); })) {
                            if (!clearedBorders++) last = std::format("FINAL_MASK_BLOCKER_DIAG position={},{},{} id={} name={} border=true resolvedGround={} resolvedDecor={}\n",
                                pos.x, pos.y, pos.z, item->getID(), item->getName(), unit->plan.items[i], unit->plan.decor.empty() ? 0 : unit->plan.decor[i]);
                        }
                    }
                }
                const auto rawReach = levelReachabilityOf(prepared->levels->plan, {}, rawMask);
                check(prepared->levels->report.allReached(), std::format("Final-mask fixture reachable, seed {}", candidate));
                found = rawReach && !rawReach->allReached() && prepared->levels->report.allReached() && clearedBorders;
                if (!found) continue;
                fixtureSeed = candidate;
                check(prepared->levels->repair.repairs <= 64, "Final-mask total repair budget");
                report << last << std::format("FINAL_MASK_RAW_REGRESSION_PASS seed={} blockingBordersCleared={} repairs={}\n",
                    candidate, clearedBorders, prepared->levels->repair.repairs);
            }
            check(found, "Fixture must require actual post-autoborder repair (seeds 1212..1243)");
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
        check(seed, "Final-mask seed field"); seed->SetValue(std::to_string(regression ? fixtureSeed : 1212));
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
        report << std::format("FINAL_MASK_REPAIR_E2E_PASS seed={} regression={} undoRedo={} status={}\n", regression ? fixtureSeed : 1212, regression, actions, run.status());
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

// Iteration 43: storeys with stairs through the adapter (mask, items on both floors) and the real dialog (Place, Undo, Redo).
void buildingStairsGuiProbe(std::ostream& report) {
    UndoLimits limits;
    check(g_gui.NewMap(), "I43 GUI map");
    auto& editor = *g_gui.GetCurrentEditor();
    constexpr uint32_t side = 128, n = side * side;
    const auto stairs = resolveBuildingStairsBrush("ramp");
    check(bool(stairs), "I43 ramp stairs resolve");
    std::vector<uint16_t> ramps, openings;
    for (const auto& ids : stairs->items) { ramps.push_back(ids.ramp); openings.push_back(ids.opening); }
    // Adapter: the actual blocking mask and both stairs items, with a cap sharing z - 1.
    GenerationConfig config{{100, 100, 7, side, side}, 1001};
    config.algorithm = GenerationAlgorithm::Surface;
    config.surface.buildingDensity = 40; config.surface.repairReach = 1; config.surface.rockPermille = 200;
    config.surface.plateauMinTop = 64; config.surface.plateauMaxPercent = 10; config.surface.plateauLevels = 1;
    config.surface.buildingStoreys = true; config.surface.upperFloorPercent = 60;
    GroundProfile p{{"grass", "dirt", "rocky ground", "sand", "clear water", "clear water", "cobblestone", "stone floor"}, 8};
    p.wall = "stone wall"; p.door = "Normal door";
    p.plateauGround = "grass"; p.plateauRamp = "ramp"; p.bridgeBrush = "bridge"; p.buildingStairs = "ramp";
    auto prepared = prepareTerrain(editor.map, config, p, {1000000, 1100000, 2000000, 8ULL << 30});
    check(prepared && prepared->levels && prepared->plateau && prepared->plan.stairs.size() == n, "I43 adapter preparation");
    if (!prepared || !prepared->levels || !prepared->plateau) return;
    const auto& levels = *prepared->levels;
    // The adapter mask keeps closed building door items blocking (unchanged since iteration 41), so the walk from the
    // start is checked with the building door cells opened; everything else is the adapter's actual mask.
    auto openedDoors = levels.blockingMask;
    size_t doorCells = 0;
    for (uint32_t i = 0; i < n; ++i) if (!prepared->plan.doors.empty() && prepared->plan.doors[i]) { openedDoors[i] = 0; ++doorCells; }
    const auto walk = levelReachabilityOf(levels.plan, {}, openedDoors);
    check(walk && doorCells > 0, "I43 walk with opened doors");
    if (!walk) return;
    const auto& storeyCount = walk->categories[size_t(LevelPoiKind::Storey)];
    std::string unreachable;
    for (const auto& u : walk->firstUnreachable) unreachable += std::format(" {}@{},{},{}", int(u.kind), u.x, u.y, u.z);
    check(storeyCount.total > 0 && storeyCount.reached == storeyCount.total && walk->walkableEdges == walk->edges,
        std::format("I43 every storey cell reachable through door and stairs: storeys {}/{} edges {}/{} first{}",
            storeyCount.reached, storeyCount.total, walk->walkableEdges, walk->edges, unreachable));
    size_t maskCells = 0, adapterStairs = 0;
    for (const auto& poi : levels.plan.pois) if (poi.kind == LevelPoiKind::Storey) {
        ++maskCells;
        check(!levels.blockingMask[poi.at.floor * n + poi.at.cell] && levels.report.walkable[poi.at.floor * n + poi.at.cell], "I43 storey cell open in adapter mask");
    }
    const auto& upper = *prepared->plateau;
    for (uint32_t i = 0; i < n; ++i) {
        const auto ramp = std::ranges::find(ramps, prepared->plan.stairs[i]);
        if (ramp == ramps.end()) continue;
        ++adapterStairs;
        const auto opening = openings[size_t(ramp - ramps.begin())];
        check(upper.plan.plateauFloor[i] == 5 && upper.plan.items[i] == opening, "I43 opening above stairs in upper plan");
        const Position below(100 + int(i % side), 100 + int(i / side), 7), above(below.x, below.y, 6);
        const auto* lower = prepared->preview.getBuffer().getTile(below);
        const auto* top = upper.preview.getBuffer().getTile(above);
        check(lower && std::ranges::any_of(lower->items, [&](const auto& item) { return item->getID() == *ramp; }), "I43 stairs item in ground preview");
        check(top && top->ground && top->ground->getID() == opening, "I43 opening ground in upper preview");
        check(!levels.blockingMask[i], "I43 stairs cell walkable in adapter mask");
    }
    check(adapterStairs > 0, "I43 adapter placed stairs");

    // Dialog: a missing stairs brush disables the option with a message; the real brush enables it.
    auto* dialog = new TerrainGeneratorDialog(g_gui.root);
    const auto spin = [&](const char* name, int value) {
        auto* c = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, dialog));
        check(c, std::format("I43 spin {}", name)); c->SetValue(value);
        wxSpinEvent event(wxEVT_SPINCTRL, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
    };
    const auto tick = [&](const char* name, bool value) {
        auto* c = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByName(name, dialog));
        check(c, std::format("I43 check {}", name)); c->SetValue(value);
        wxCommandEvent event(wxEVT_CHECKBOX, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
        return c;
    };
    auto* style = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-style", dialog));
    check(style, "I43 style control"); style->SetSelection(1);
    wxCommandEvent changed(wxEVT_CHOICE, style->GetId()); changed.SetEventObject(style);
    style->GetEventHandler()->ProcessEvent(changed);
    spin("terrain-width", side); spin("terrain-height", side); spin("terrain-building-density", 40);
    spin("terrain-plateau-percent", 10); spin("terrain-plateau-levels", 1);
    tick("terrain-walls", true); tick("terrain-doors", true);
    auto* storeys = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByName("terrain-building-storeys", dialog));
    auto* brush = dynamic_cast<wxComboBox*>(wxWindow::FindWindowByName("terrain-building-stairs", dialog));
    check(storeys && brush && !storeys->GetValue() && brush->GetValue() == "ramp", "I43 option defaults");
    brush->Clear();
    wxCommandEvent brushChanged(wxEVT_COMBOBOX, brush->GetId()); brushChanged.SetEventObject(brush);
    brush->GetEventHandler()->ProcessEvent(brushChanged);
    check(!storeys->IsEnabled() && !storeys->GetValue(), "Missing stairs brush must disable upper floors");
    brush->Append("ramp"); brush->SetSelection(0);
    brush->GetEventHandler()->ProcessEvent(brushChanged);
    check(storeys->IsEnabled(), "Valid stairs brush must enable upper floors");
    tick("terrain-building-storeys", true);
    spin("terrain-upper-floor-percent", 60);
    auto* seed = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed", dialog));
    check(seed, "I43 seed"); seed->SetValue("1001");
    // Dialog Preview and Validate only: closed building doors stay blocking in the dialog's validator (see above), so
    // its Place asks about unreachable places in a modal box. The adapter result above is committed instead.
    MountainDialogRun run(*dialog, editor, 0, false, true);
    const int modal = dialog->ShowModal(); run.stop();
    check(modal == wxID_CANCEL && run.placed() && run.floors() == 2 && run.status().starts_with("Reachable:"), "I43 GUI preview: " + run.failure() + run.status());
    dialog->Destroy();
    check(bool(DrawOperations::commitTerrainPreview(*prepared, config, p, {750000, 1536ULL << 20})), "I43 commit");
    // Committed map: stairs below, opening above, an open arrival beside it and walls on the storey outline.
    constexpr int sx[] = {0, 0, 1, -1}, sy[] = {-1, 1, 0, 0};
    size_t pairs = 0, storeyWalls = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const int x = 100 + int(i % side), y = 100 + int(i / side);
        if (const auto* t = editor.map.getTile(Position(x, y, 6)); t && t->hasWall()) ++storeyWalls;
        const auto* tile = editor.map.getTile(Position(x, y, 7));
        // Mountain ramps use the same items; building stairs stand on the building floor.
        if (!tile || !tile->ground || brushOf(tile->ground->getID()) != g_brushes.getBrush("stone floor")) continue;
        for (const auto& item : tile->items) {
            const auto ramp = std::ranges::find(ramps, item->getID());
            if (ramp == ramps.end()) continue;
            const auto shape = stairs->shapes[size_t(ramp - ramps.begin())];
            const auto* above = editor.map.getTile(Position(x, y, 6));
            const auto d = size_t(shape.direction);
            const auto* arrival = editor.map.getTile(Position(x + sx[d], y + sy[d], 6));
            check(above && above->ground && above->ground->getID() == openings[size_t(ramp - ramps.begin())], "I43 committed opening above stairs");
            if (!above || !above->ground) continue;
            check(arrival && arrival->ground && !arrival->ground->isBlocking() &&
                std::ranges::none_of(arrival->items, [](const auto& it) { return it->isBlocking(); }), "I43 committed arrival walkable");
            check(above->ground->getDefinition().hasFlag(ItemFlag::FloorChangeDown), "I43 committed opening changes floor");
            ++pairs;
        }
    }
    check(pairs > 0 && storeyWalls > 0, "I43 committed stairs pairs and storey walls");
    std::vector<std::vector<uint32_t>> after;
    for (int z = 4; z <= 8; ++z) after.push_back(stackImage(editor.map, 98, 98, z, side + 4, side + 4));
    const auto actions = editor.actionQueue->getSize();
    for (size_t i = 0; i < actions; ++i) check(editor.actionQueue->undo(), "I43 Undo");
    check(editor.map.getTileCount() == 0, "I43 Undo restores empty map");
    for (size_t i = 0; i < actions; ++i) check(editor.actionQueue->redo(), "I43 Redo");
    for (int z = 4; z <= 8; ++z) check(after[size_t(z - 4)] == stackImage(editor.map, 98, 98, z, side + 4, side + 4), "I43 identical Redo");
    report << std::format("BUILDING_STAIRS_GUI_PASS adapterStairs={} maskCells={} doorCells={} committedPairs={} storeyWalls={} undoRedo={} dialogFloors={} brush=ramp\n",
        adapterStairs, maskCells, doorCells, pairs, storeyWalls, actions, run.floors());
    editor.map.clearChanges();
}

// Iteration 45: dungeons below buildings through the adapter (mask, walls, doors, stairs and opening) and the real
// dialog (missing brush message, Preview, Validate, Place, Undo, Redo).
void dungeonGuiProbe(std::ostream& report) {
    UndoLimits limits;
    check(g_gui.NewMap(), "I45 GUI map");
    auto& editor = *g_gui.GetCurrentEditor();
    constexpr uint32_t side = 192, n = side * side;
    const auto stairs = resolveBuildingStairsBrush("ramp");
    const auto doorItems = resolveCaveDoorBrush("brick wall");
    check(bool(stairs) && bool(doorItems), "I45 dungeon brushes resolve");
    std::vector<uint16_t> ramps, openings;
    for (const auto& ids : stairs->items) { ramps.push_back(ids.ramp); openings.push_back(ids.opening); }
    const auto* floorBrush = g_brushes.getBrush("dark tiled stone floor");
    std::vector<uint16_t> wallIds;
    if (auto* walls = g_brushes.getBrush("brick wall"); walls && walls->as<WallBrush>()) const_cast<WallBrush*>(walls->as<WallBrush>())->getRelatedItems(wallIds);
    check(!wallIds.empty(), "I45 dungeon wall pieces");
    const auto dungeonWall = [&](const Item* item) { return item && std::ranges::find(wallIds, item->getID()) != wallIds.end(); };
    GenerationConfig config{{100, 100, 7, side, side}, 1001};
    config.algorithm = GenerationAlgorithm::Surface;
    config.surface.buildingDensity = 60; config.surface.repairReach = 1; config.surface.rockPermille = 200;
    config.surface.caveEntrances = 16; config.surface.shoreSmoothing = 2;
    config.surface.buildingStoreys = true; config.surface.upperFloorPercent = 50;
    config.surface.buildingDungeons = true; config.surface.dungeonPercent = 60;
    // Impassable rock (as m19) turns on the rock-face entrance clearing that removed building walls beside the stairs down.
    GroundProfile p{{"grass", "dirt", "mountain", "sand", "clear water", "clear water", "cobblestone", "stone floor"}, 8};
    p.wall = "stone wall"; p.door = "Normal door"; p.bridgeBrush = "bridge"; p.buildingStairs = "ramp";
    p.caveEntrance = "cave enter"; p.caveExit = "ramp"; p.caveGround = "cave"; p.caveWall = "mountain";
    p.dungeonFloor = "dark tiled stone floor"; p.dungeonWall = "brick wall"; p.dungeonStairs = "ramp"; p.dungeonDescent = "stairs down";
    check(!resolveCaveDoorBrush("dark stone wall"), "I45c wall brush whose door slots hold plain walls is refused");
    const auto dungeonIds = resolveDungeonBrushes(p);
    check(dungeonIds && g_item_definitions.get(dungeonIds->descent).hasFlag(ItemFlag::FloorChangeDown), "I45b stairs down resolve");
    const uint16_t descent = dungeonIds ? dungeonIds->descent : 0;
    // Iteration 45b: dungeon floor only inside the walls and under walls touching the dungeon by a side; the ring corners
    // carry their wall piece without ground. No ramp opening ("stairs" ground) on the surface.
    const auto outsideFloor = [&](auto tileAt) {
        size_t outside = 0;
        const auto open = [&](int x, int y) {
            const auto* t = tileAt(Position(x, y, 8));
            return t && t->ground && brushOf(t->ground->getID()) == floorBrush && (!t->getWall() || t->getWall()->isBrushDoor());
        };
        for (uint32_t i = 0; i < n; ++i) {
            const int x = 100 + int(i % side), y = 100 + int(i / side);
            const auto* t = tileAt(Position(x, y, 8));
            if (!t || !t->ground || brushOf(t->ground->getID()) != floorBrush || open(x, y)) continue;
            outside += !(open(x - 1, y) || open(x + 1, y) || open(x, y - 1) || open(x, y + 1));
        }
        return outside;
    };
    const auto surfaceOpenings = [&](auto tileAt) {
        size_t holes = 0;
        for (uint32_t i = 0; i < n; ++i) {
            const auto* t = tileAt(Position(100 + int(i % side), 100 + int(i / side), 7));
            holes += t && t->ground && std::ranges::find(openings, t->ground->getID()) != openings.end();
        }
        return holes;
    };
    std::map<uint16_t, size_t> cornerPieces;
    auto prepared = prepareTerrain(editor.map, config, p, {1000000, 1100000, 2000000, 8ULL << 30});
    check(prepared && prepared->levels && prepared->cave, "I45 adapter preparation");
    if (!prepared || !prepared->levels || !prepared->cave) return;
    const auto& levels = *prepared->levels;
    const auto& lower = *prepared->cave;
    const auto& dungeonCount = levels.report.categories[size_t(LevelPoiKind::Dungeon)];
    check(dungeonCount.total > 0 && dungeonCount.reached == dungeonCount.total && levels.report.allReached() &&
        levels.report.walkableEdges == levels.report.edges, std::format("I45 every dungeon cell reachable: {}/{} edges {}/{}",
        dungeonCount.reached, dungeonCount.total, levels.report.walkableEdges, levels.report.edges));
    size_t maskCells = 0, wallCells = 0, maskDoors = 0, adapterStairs = 0;
    for (const auto& poi : levels.plan.pois) if (poi.kind == LevelPoiKind::Dungeon) {
        ++maskCells;
        check(poi.at.floor == 1 && !levels.blockingMask[n + poi.at.cell] && levels.report.walkable[n + poi.at.cell], "I45 dungeon cell open in adapter mask");
    }
    for (uint32_t i = 0; i < n; ++i) {
        const Position below(100 + int(i % side), 100 + int(i / side), 8), above(below.x, below.y, 7);
        const auto* tile = lower.preview.getBuffer().getTile(below);
        if (lower.plan.caveFloor[i] == 3 || lower.plan.caveFloor[i] == 4)
            check(tile && tile->ground && brushOf(tile->ground->getID()) == floorBrush, "I45 dungeon floor ground in lower preview");
        if (lower.plan.caveFloor[i] == 5) {
            check(tile && !tile->ground && dungeonWall(tile->getWall()), "I45b ring corner wall without ground");
            if (tile && tile->getWall()) ++cornerPieces[tile->getWall()->getID()];
        }
        if (lower.plan.caveFloor[i] == 4 && !lower.plan.doors[i]) {
            ++wallCells;
            check(tile && dungeonWall(tile->getWall()), "I45 dungeon wall in lower preview");
        }
        if (lower.plan.caveFloor[i] == 4 && lower.plan.doors[i]) {
            ++maskDoors;
            const auto* door = tile ? tile->getWall() : nullptr;
            check(door && door->isBrushDoor() && !door->isLocked() && !door->isOpen() &&
                (door->getID() == doorItems->horizontal || door->getID() == doorItems->vertical) && door->getDefinition().isDoor(), "I45 closed unlocked dungeon door");
            check(!levels.blockingMask[n + i] && levels.report.walkable[n + i], "I45 dungeon door openable in adapter mask");
        }
        const auto ramp = std::ranges::find(ramps, lower.plan.exits[i]);
        if (lower.plan.caveFloor[i] != 3 || ramp == ramps.end()) continue;
        ++adapterStairs;
        const auto* top = prepared->preview.getBuffer().getTile(above);
        check(prepared->plan.entrances[i] == descent && top && top->ground && top->ground->getID() == descent &&
            top->ground->getDefinition().hasFlag(ItemFlag::FloorChangeDown), "I45b stairs down above the dungeon stairs");
        check(tile && std::ranges::any_of(tile->items, [&](const auto& item) { return item->getID() == *ramp; }), "I45 stairs item in lower preview");
        check(stairs->shapes[size_t(ramp - ramps.begin())].direction == ExitDirection::North &&
            g_item_definitions.get(*ramp).hasFlag(ItemFlag::FloorChangeNorth), "I45c stairs up face north");
        // The stairs down stand inside the building, the building wall south of them stays.
        const auto* wallBelow = prepared->preview.getBuffer().getTile(Position(above.x, above.y + 1, 7));
        check(wallBelow && wallBelow->hasWall(), "I45c building wall south of the stairs down");
        check(!levels.blockingMask[i] && !levels.blockingMask[n + i], "I45 stairs cells walkable in adapter mask");
    }
    check(maskCells > 0 && wallCells > 0 && maskDoors > 0 && adapterStairs > 0 && !cornerPieces.empty(), "I45 adapter placed dungeons");
    // Building outlines: every planned wall (or door) of the surface is on the preview.
    size_t missingWalls = 0;
    for (uint32_t i = 0; i < n; ++i) if (!prepared->plan.walls.empty() && prepared->plan.walls[i]) {
        const auto* t = prepared->preview.getBuffer().getTile(Position(100 + int(i % side), 100 + int(i / side), 7));
        missingWalls += !t || !t->getWall();
    }
    check(missingWalls == 0, std::format("I45c building walls intact: {} missing", missingWalls));
    const auto previewOutside = outsideFloor([&](const Position& at) { return lower.preview.getBuffer().getTile(at); });
    const auto previewHoles = surfaceOpenings([&](const Position& at) { return prepared->preview.getBuffer().getTile(at); });
    check(previewOutside == 0 && previewHoles == 0, std::format("I45b preview: floor outside the walls {}, stairs openings on z 7 {}", previewOutside, previewHoles));
    std::string pieces;
    for (const auto& [id, count] : cornerPieces) pieces += std::format("{}{}x{}", pieces.empty() ? "" : ",", id, count);
    prepared = std::unexpected(PreparationError{PreparationInputError::Cancelled});

    // Dialog: a wall brush without normal doors disables the option with a message; the real brushes enable it.
    auto* dialog = new TerrainGeneratorDialog(g_gui.root);
    const auto spin = [&](const char* name, int value) {
        auto* c = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, dialog));
        check(c, std::format("I45 spin {}", name)); c->SetValue(value);
        wxSpinEvent event(wxEVT_SPINCTRL, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
    };
    const auto tick = [&](const char* name) {
        auto* c = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByName(name, dialog));
        check(c && c->IsEnabled(), std::format("I45 check {}", name)); c->SetValue(true);
        wxCommandEvent event(wxEVT_CHECKBOX, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
    };
    auto* style = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-style", dialog));
    check(style, "I45 style control"); style->SetSelection(1);
    wxCommandEvent changed(wxEVT_CHOICE, style->GetId()); changed.SetEventObject(style);
    style->GetEventHandler()->ProcessEvent(changed);
    spin("terrain-width", side); spin("terrain-height", side); spin("terrain-building-density", 60); spin("terrain-cave-entrances", 16);
    tick("terrain-walls"); tick("terrain-doors");
    auto* dungeons = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByName("terrain-building-dungeons", dialog));
    auto* wall = dynamic_cast<wxComboBox*>(wxWindow::FindWindowByName("terrain-dungeon-wall", dialog));
    auto* floor = dynamic_cast<wxComboBox*>(wxWindow::FindWindowByName("terrain-dungeon-floor", dialog));
    auto* stairsChoice = dynamic_cast<wxComboBox*>(wxWindow::FindWindowByName("terrain-dungeon-stairs", dialog));
    auto* percent = wxWindow::FindWindowByName("terrain-dungeon-percent", dialog);
    check(dungeons && wall && floor && stairsChoice && percent && !dungeons->GetValue() && wall->GetValue() == "brick wall" &&
        floor->GetValue() == "dark tiled stone floor" && stairsChoice->GetValue() == "ramp" && dungeons->IsEnabled(), "I45 option defaults");
    wxCommandEvent dropdown(wxEVT_COMBOBOX_DROPDOWN, wall->GetId()); dropdown.SetEventObject(wall);
    wall->GetEventHandler()->ProcessEvent(dropdown);
    check(wall->SetStringSelection("dark stone wall"), "I45c wall brush without real doors offered");
    wxCommandEvent wallChanged(wxEVT_COMBOBOX, wall->GetId()); wallChanged.SetEventObject(wall);
    wall->GetEventHandler()->ProcessEvent(wallChanged);
    check(!dungeons->IsEnabled() && !dungeons->GetValue(), "Missing dungeon door brush must disable dungeons");
    check(wall->SetStringSelection("brick wall"), "I45 dungeon wall brush offered");
    wall->GetEventHandler()->ProcessEvent(wallChanged);
    check(dungeons->IsEnabled(), "Valid dungeon brushes must enable dungeons");
    tick("terrain-building-dungeons");
    spin("terrain-dungeon-percent", 60);
    auto* seed = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed", dialog));
    check(seed, "I45 seed"); seed->SetValue("1001");
    MountainDialogRun run(*dialog, editor, 0, true);
    const int modal = dialog->ShowModal(); run.stop();
    check(modal == wxID_OK && run.placed(), "I45 GUI flow: " + run.failure() + " " + run.status());
    dialog->Destroy();
    // Committed map: dungeon walls and closed unlocked doors on z + 1, stairs below an opening inside the building.
    constexpr int sx[] = {0, 0, 1, -1}, sy[] = {-1, 1, 0, 0};
    size_t committedDoors = 0, committedWalls = 0, committedStairs = 0;
    const auto walkable = [&](const Position& at) {
        const auto* t = editor.map.getTile(at);
        return t && t->ground && !t->ground->isBlocking() && std::ranges::none_of(t->items, [](const auto& it) { return it->isBlocking(); });
    };
    for (uint32_t i = 0; i < n; ++i) {
        const int x = 100 + int(i % side), y = 100 + int(i / side);
        const auto* tile = editor.map.getTile(Position(x, y, 8));
        if (!tile || !tile->ground || brushOf(tile->ground->getID()) != floorBrush) continue;
        if (const auto* w = tile->getWall()) {
            if (w->isBrushDoor()) {
                ++committedDoors;
                check(!w->isLocked() && !w->isOpen() && w->getDefinition().isDoor() &&
                    (w->getID() == doorItems->horizontal || w->getID() == doorItems->vertical), "I45 committed door item");
                const bool west = walkable({x - 1, y, 8}), east = walkable({x + 1, y, 8}), north = walkable({x, y - 1, 8}), south = walkable({x, y + 1, 8});
                check((west && east && !north && !south) || (north && south && !west && !east), "I45 committed door neighbours");
            } else {
                ++committedWalls;
                check(dungeonWall(w), "I45 committed dungeon wall");
            }
        }
        for (const auto& item : tile->items) {
            const auto ramp = std::ranges::find(ramps, item->getID());
            if (ramp == ramps.end()) continue;
            const auto shape = stairs->shapes[size_t(ramp - ramps.begin())];
            check(shape.direction == ExitDirection::North, "I45c committed stairs up face north");
            const auto d = size_t(shape.direction);
            const auto* above = editor.map.getTile(Position(x, y, 7));
            check(above && above->ground && above->ground->getID() == descent &&
                above->ground->getDefinition().hasFlag(ItemFlag::FloorChangeDown), "I45 committed opening above dungeon stairs");
            const int tx = x + shape.tailDx, ty = y + shape.tailDy;
            check(walkable({x + sx[d], y + sy[d], 7}) && walkable({tx - sx[d], ty - sy[d], 8}), "I45 committed arrival and dungeon entry walkable");
            ++committedStairs;
        }
    }
    check(committedDoors > 0 && committedWalls > 0 && committedStairs > 0, "I45 committed dungeons");
    const auto mapTile = [&](const Position& at) -> const Tile* { return editor.map.getTile(at); };
    const auto committedOutside = outsideFloor(mapTile), committedHoles = surfaceOpenings(mapTile);
    check(committedOutside == 0 && committedHoles == 0, std::format("I45b map: floor outside the walls {}, stairs openings on z 7 {}", committedOutside, committedHoles));
    std::vector<std::vector<uint32_t>> after;
    for (int z = 4; z <= 8; ++z) after.push_back(stackImage(editor.map, 98, 98, z, side + 4, side + 4));
    const auto actions = editor.actionQueue->getSize();
    for (size_t i = 0; i < actions; ++i) check(editor.actionQueue->undo(), "I45 Undo");
    check(editor.map.getTileCount() == 0, "I45 Undo restores empty map");
    for (size_t i = 0; i < actions; ++i) check(editor.actionQueue->redo(), "I45 Redo");
    for (int z = 4; z <= 8; ++z) check(after[size_t(z - 4)] == stackImage(editor.map, 98, 98, z, side + 4, side + 4), "I45 identical Redo");
    report << std::format("DUNGEON_GUI_PASS adapterStairs={} maskCells={} walls={} doors={} committedStairs={} committedWalls={} committedDoors={} "
        "undoRedo={} dialogFloors={} outsideFloor={}/{} surfaceOpenings={}/{} cornerPieces={} missingWalls={} brushes=dark tiled stone floor/brick wall/ramp/stairs down\n",
        adapterStairs, maskCells, wallCells, maskDoors, committedStairs, committedWalls, committedDoors, actions, run.floors(),
        previewOutside, committedOutside, previewHoles, committedHoles, pieces, missingWalls);
    editor.map.clearChanges();
}

// GUI report after iteration 43 (Buildings 100, walls, doors, upper floors, mountains 10 %): "Repairs: 0 ... Unrepaired".
// Seed 1729 at (1300, 850) 512x512 has a cap whose only ramp lands in a surface pocket sealed by the cap and the
// kept-clear rim; the repair stopped at that ramp instead of dropping it and the cap, and processed nothing after it.
void storeySealedRampRepair(std::ostream& report) {
    check(g_gui.NewMap(), "Sealed ramp map");
    auto& editor = *g_gui.GetCurrentEditor();
    GenerationConfig config{{1300, 850, 7, 512, 512}, 1729};
    config.algorithm = GenerationAlgorithm::Surface;
    config.surface.buildingDensity = 100; config.surface.repairReach = 1; config.surface.shoreSmoothing = 2;
    config.surface.plateauMinTop = 64; config.surface.plateauMaxPercent = 10; config.surface.plateauLevels = 1;
    config.surface.buildingStoreys = true; config.surface.upperFloorPercent = 30;
    GroundProfile p{{"grass", "dirt", "rocky ground", "sand", "clear water", "clear water", "cobblestone", "stone floor"}, 8};
    p.wall = "stone wall"; p.door = "Normal door"; p.bridgeBrush = "bridge"; p.buildingStairs = "ramp";
    p.plateauGround = "grass"; p.plateauRamp = "ramp"; p.plateauCliff = "mountain";
    const auto prepared = prepareTerrain(editor.map, config, p, {1000000, 1100000, 2000000, 8ULL << 30});
    check(prepared && prepared->levels, "Sealed ramp preparation");
    const auto& levels = *prepared->levels;
    const auto& storeys = levels.report.categories[size_t(LevelPoiKind::Storey)];
    check(levels.report.allReached() && levels.report.walkableEdges == levels.report.edges && !levels.repair.unrepaired &&
        levels.repair.removedCaps > 0 && storeys.total > 0 && storeys.reached == storeys.total,
        std::format("Sealed ramp repair: repairs {} removedCaps {} unrepaired {} unreachable {}", levels.repair.repairs,
            levels.repair.removedCaps, levels.repair.unrepaired, levels.report.firstUnreachable.size()));
    report << std::format("STOREY_SEALED_RAMP_REPAIR_PASS repairs={} removedCaps={} storeys={}\n",
        levels.repair.repairs, levels.repair.removedCaps, storeys.total);
    editor.map.clearChanges();
}

// Iteration 44: the exact GUI report through the real dialog. Profile m19 (rock brush "mountain", impassable ground),
// x 1000, y 1000, 512 x 512, seed 1729, 10 cave entrances, 3 mountain levels, buildings 100 with walls, Normal doors and
// upper floors 30 % (ramp); everything else at the dialog defaults. Before: about 5200 unreachable places.
void m19RockGuiCase(std::ostream& report) {
    UndoLimits undoLimits;
    const auto ids = loadedClientAssetIds();
    check(bool(ids), "I44 fixture client assets");
    StyleProfile fixture;
    fixture.client = ids->client; fixture.otbSha256 = ids->otbSha256; fixture.datSha256 = ids->datSha256;
    fixture.tuning = SurfaceTuning{192, 300, 12, 8, 20, 8, 240, 8, 300, 6000, 8};
    fixture.decorTuning = DecorTuning{565, 17, 369, 239, 23, 29};
    fixture.palette = {"grass", "dirt", "mountain", "sand", "sea"};
    fixture.decorPalette = {"trees", "grass tufts", "small rocks"};
    const auto encoded = serializeProfile(fixture);
    check(bool(encoded), "I44 fixture serialization");
    const auto directory = styleProfileDirectory();
    const auto relative = std::filesystem::weakly_canonical(directory).lexically_relative(std::filesystem::current_path().parent_path());
    check(!relative.empty() && *relative.begin() != "..", "I44 profile must stay in private probe directory");
    std::filesystem::create_directories(directory);
    const auto file = directory / "i44-m19-fixture.profile";
    check(!std::filesystem::exists(file), "I44 fixture must not overwrite a profile");
    {std::ofstream out(file); out << *encoded; check(bool(out), "I44 profile write");}
    check(g_gui.NewMap(), "I44 GUI map");
    auto& editor = *g_gui.GetCurrentEditor();
    auto* dialog = new TerrainGeneratorDialog(g_gui.root);
    const auto spin = [&](const char* name, int value) {
        auto* c = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, dialog));
        check(c, std::format("I44 spin {}", name)); c->SetValue(value);
        wxSpinEvent event(wxEVT_SPINCTRL, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
    };
    const auto tick = [&](const char* name) {
        auto* c = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByName(name, dialog));
        check(c && c->IsEnabled(), std::format("I44 check {}", name)); c->SetValue(true);
        wxCommandEvent event(wxEVT_CHECKBOX, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
    };
    const auto choose = [&](const char* name, int index) {
        auto* c = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName(name, dialog));
        check(c && index >= 0 && index < int(c->GetCount()), std::format("I44 choice {}", name)); c->SetSelection(index);
        wxCommandEvent event(wxEVT_CHOICE, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
        return c;
    };
    choose("terrain-style", 1);
    auto* profiles = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-profile", dialog));
    check(profiles, "I44 profile control");
    choose("terrain-profile", profiles->FindString("i44-m19-fixture"));
    auto* rock = dynamic_cast<wxComboBox*>(wxWindow::FindWindowByName("terrain-rock-brush", dialog));
    check(rock && rock->GetValue() == "mountain", "I44 m19 rock brush applied");
    spin("terrain-x", 1000); spin("terrain-y", 1000); spin("terrain-width", 512); spin("terrain-height", 512);
    spin("terrain-building-density", 100); spin("terrain-cave-entrances", 10); spin("terrain-plateau-levels", 3);
    tick("terrain-walls"); tick("terrain-doors"); tick("terrain-building-storeys");
    auto* door = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-door-type", dialog));
    check(door && door->GetStringSelection() == "Normal door", "I44 Normal door default");
    auto* seed = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed", dialog));
    check(seed, "I44 seed"); seed->SetValue("1729");
    MountainDialogRun run(*dialog, editor, 0, true, true);
    const int modal = dialog->ShowModal(); run.stop();
    check(modal == wxID_CANCEL && run.placed(), "I44 GUI case: " + run.failure() + " " + run.status());
    std::string status = run.status();
    std::ranges::replace(status, '\n', ' ');
    // Client rock passability of the measured profiles (input of the iteration 44 gate measurement).
    const auto blocks = [](const char* rock) {
        GroundProfile p{{"grass", "dirt", rock, "sand", "sea"}, 5};
        GenerationConfig c; c.algorithm = GenerationAlgorithm::Surface;
        return withRockPassability(c, p).surface.rockBlocks;
    };
    check(blocks("mountain") && !blocks("rocky ground"), "I44 client rock passability");
    report << std::format("M19_ROCK_GUI_CASE_PASS floors={} rockBlocks m06={} m19={} status={}\n", run.floors(),
        int(blocks("mountain top ground")), int(blocks("mountain")), status);
    dialog->Destroy();
    editor.map.clearChanges();
    std::filesystem::remove(file);
}

// Iteration 48b: the look of every cave entrance on a placed map. A "cave enter" item (rock-face entrance) only stands on
// or next to the rock brush's ground; an ordinary entrance off the rock (iteration 48 fallback) is the "cave hole" ground.
// Each entrance carries a floor change down and has a ramp of the exit brush directly below.
struct EntranceLook { size_t rockFace = 0, holes = 0; };
EntranceLook checkEntranceLook(Map& map, int x0, int y0, int z, int width, int height, const Brush* rock, const CaveExitBrush& exits,
    std::string_view label) {
    const auto rockFaceIds = entranceAlternatives("cave enter"), holeIds = entranceAlternatives("cave hole");
    check(!rockFaceIds.empty() && !holeIds.empty() && rock, std::format("{} entrance brushes", label));
    const auto groundIs = [&](int x, int y, const auto& match) {
        const auto* tile = map.getTile(Position(x, y, z));
        return tile && tile->ground && match(tile->ground->getID());
    };
    EntranceLook look;
    for (int y = y0; y < y0 + height; ++y) for (int x = x0; x < x0 + width; ++x) {
        const auto* surface = map.getTile(Position(x, y, z));
        if (!surface || !surface->ground) continue;
        const bool rockFace = std::ranges::any_of(surface->items, [&](const auto& item) {
            return std::ranges::find(rockFaceIds, item->getID()) != rockFaceIds.end(); });
        const bool hole = std::ranges::find(holeIds, surface->ground->getID()) != holeIds.end();
        if (!rockFace && !hole) continue;
        const auto at = std::format("{} at ({},{},{})", label, x, y, z);
        check(!(rockFace && hole), "Entrance with both a rock face and a hole " + at);
        const auto isRock = [&](uint16_t id) { return brushOf(id) == rock; };
        check(!rockFace || groundIs(x, y, isRock) || groundIs(x, y - 1, isRock) || groundIs(x - 1, y, isRock) ||
            groundIs(x + 1, y, isRock) || groundIs(x, y + 1, isRock), "\"cave enter\" off the rock " + at);
        const auto down = [](uint16_t id) { return g_item_definitions.get(id).hasFlag(ItemFlag::FloorChangeDown); };
        check(down(surface->ground->getID()) || std::ranges::any_of(surface->items, [&](const auto& item) { return down(item->getID()); }),
            "Entrance without a floor change down " + at);
        const auto* lower = map.getTile(Position(x, y, z + 1));
        check(lower && std::ranges::any_of(lower->items, [&](const auto& item) {
            return std::ranges::any_of(exits.items, [&](const auto& ids) { return ids.ramp == item->getID(); }); }),
            "Entrance without a ramp below " + at);
        look.rockFace += rockFace; look.holes += hole;
    }
    return look;
}

// Iteration 46 (D26): the GUI report of a cave ramp without an entrance above it at (1018, 1237, 7). Real dialog, its
// default settings plus 10 cave entrances, profiles m06 and m19 (tuning and palettes of the g4 profiles; one case may
// have no ramp when m19's impassable rock leaves no entrance on the start land, but m19 as a whole must), the 512 x 512
// rectangle centred on that point, surface on z 6 (cave and ramp on z 7) and on z 7, several seeds, far exits on (the
// default) and off. On the placed map every cave ramp needs its entrance directly above, except far exits, whose
// surface cell must be walkable; the dialog must report every place reachable.
void d26RampCase(std::ostream& report) {
    UndoLimits undoLimits;
    const auto ids = loadedClientAssetIds();
    check(bool(ids), "D26 fixture client assets");
    struct Fixture { const char* name; SurfaceTuning tuning; DecorTuning decor; StylePalette palette; StyleDecorPalette decorPalette; };
    const Fixture fixtures[] = {
        {"i46-m19-fixture", SurfaceTuning{192, 300, 12, 8, 20, 8, 240, 8, 300, 6000, 8}, DecorTuning{565, 17, 369, 239, 23, 29},
            {"grass", "dirt", "mountain", "sand", "sea"}, {"trees", "grass tufts", "small rocks"}},
        {"i46-m06-fixture", SurfaceTuning{192, 300, 12, 8, 20, 8, 80, 8, 300, 6000, 8}, DecorTuning{613, 6, 87, 142, 8, 20},
            {"grass", "jungle dirt", "mountain top ground", "sand", "sea"}, {"trees", "bushes", "kamienie"}}};
    const auto directory = styleProfileDirectory();
    const auto relative = std::filesystem::weakly_canonical(directory).lexically_relative(std::filesystem::current_path().parent_path());
    check(!relative.empty() && *relative.begin() != "..", "D26 profile must stay in private probe directory");
    std::filesystem::create_directories(directory);
    const auto* exitDoodad = g_brushes.getBrush("ramp")->as<DoodadBrush>();
    check(exitDoodad, "D26 ramp brush");
    const auto exitBrush = resolveCaveExitBrush(*exitDoodad);
    check(bool(exitBrush), "D26 ramp brush items");
    auto entranceIds = entranceAlternatives("cave enter");
    std::ranges::copy(entranceAlternatives("cave hole"), std::back_inserter(entranceIds));
    size_t runs = 0, paired = 0, farRamps = 0, m19Paired = 0;
    EntranceLook looks;
    for (const auto& fixture : fixtures) {
        StyleProfile profile;
        profile.client = ids->client; profile.otbSha256 = ids->otbSha256; profile.datSha256 = ids->datSha256;
        profile.tuning = fixture.tuning; profile.decorTuning = fixture.decor;
        profile.palette = fixture.palette; profile.decorPalette = fixture.decorPalette;
        const auto encoded = serializeProfile(profile);
        check(bool(encoded), "D26 fixture serialization");
        const auto file = directory / (std::string(fixture.name) + ".profile");
        check(!std::filesystem::exists(file), "D26 fixture must not overwrite a profile");
        {std::ofstream out(file); out << *encoded; check(bool(out), "D26 profile write");}
        for (const int z : {6, 7})
        for (const bool farExits : {true, false})
        for (const char* seedText : {"1729", "1001", "1002", "1003", "1004"}) {
            const auto combination = std::format("{} z={} farExits={} seed={}", fixture.name, z, int(farExits), seedText);
            check(g_gui.NewMap(), "D26 GUI map");
            auto& editor = *g_gui.GetCurrentEditor();
            auto* dialog = new TerrainGeneratorDialog(g_gui.root);
            const auto spin = [&](const char* name, int value) {
                auto* c = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, dialog));
                check(c, std::format("D26 spin {}", name)); c->SetValue(value);
                wxSpinEvent event(wxEVT_SPINCTRL, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
            };
            const auto choose = [&](const char* name, int index) {
                auto* c = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName(name, dialog));
                check(c && index >= 0 && index < int(c->GetCount()), std::format("D26 choice {}", name)); c->SetSelection(index);
                wxCommandEvent event(wxEVT_CHOICE, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
            };
            choose("terrain-style", 1);
            auto* profiles = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-profile", dialog));
            check(profiles, "D26 profile control");
            choose("terrain-profile", profiles->FindString(fixture.name));
            spin("terrain-x", 1018 - 256); spin("terrain-y", 1237 - 256); spin("terrain-z", z);
            spin("terrain-width", 512); spin("terrain-height", 512); spin("terrain-cave-entrances", 10);
            auto* farCheck = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByName("terrain-cave-far-exits", dialog));
            check(farCheck && farCheck->IsEnabled() && farCheck->GetValue(), "D26 far exits on by default");
            if (!farExits) {
                farCheck->SetValue(false);
                wxCommandEvent event(wxEVT_CHECKBOX, farCheck->GetId()); event.SetEventObject(farCheck); farCheck->GetEventHandler()->ProcessEvent(event);
            }
            auto* seed = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed", dialog));
            check(seed, "D26 seed"); seed->SetValue(seedText);
            MountainDialogRun run(*dialog, editor, 0, true);
            dialog->ShowModal(); run.stop();
            check(run.placed(), "D26 GUI case " + combination + ": " + run.failure() + " " + run.status());
            size_t ramps = 0, withEntrance = 0, entrances = 0;
            for (int y = 1237 - 256; y < 1237 + 256; ++y) for (int x = 1018 - 256; x < 1018 + 256; ++x) {
                const auto* surface = editor.map.getTile(Position(x, y, z));
                const auto* lower = editor.map.getTile(Position(x, y, z + 1));
                const auto has = [](const Tile* tile, const auto& match) {
                    if (!tile) return false;
                    if (tile->ground && match(tile->ground->getID())) return true;
                    return std::ranges::any_of(tile->items, [&](const auto& item) { return match(item->getID()); });
                };
                const bool entrance = has(surface, [&](uint16_t id) { return std::ranges::find(entranceIds, id) != entranceIds.end(); });
                const bool ramp = has(lower, [&](uint16_t id) {
                    return std::ranges::any_of(exitBrush->items, [&](const auto& items) { return items.ramp == id; });
                });
                entrances += entrance;
                if (!ramp) {
                    check(!entrance, std::format("D26 entrance without a ramp below {} at ({},{},{})", combination, x, y, z));
                    continue;
                }
                ++ramps;
                if (entrance) { ++withEntrance; continue; }
                check(farExits, std::format("D26 ramp without an entrance above {} at ({},{},{})", combination, x, y, z + 1));
                check(surface && surface->ground && !surface->isBlocking(),
                    std::format("D26 far exit under a blocked or empty surface cell {} at ({},{},{})", combination, x, y, z));
            }
            paired += withEntrance; farRamps += ramps - withEntrance; ++runs;
            if (std::string_view(fixture.name) == "i46-m19-fixture") m19Paired += withEntrance;
            const auto look = checkEntranceLook(editor.map, 1018 - 256, 1237 - 256, z, 512, 512, g_brushes.getBrush(fixture.palette[2]),
                *exitBrush, "D26 " + combination);
            check(look.rockFace + look.holes == entrances, "D26 entrance look count " + combination);
            looks.rockFace += look.rockFace; looks.holes += look.holes;
            report << std::format("D26_RAMP_CASE {} ramps={} withEntrance={} farExits={} rockFace={} holes={} status={}\n", combination, ramps,
                withEntrance, ramps - withEntrance, look.rockFace, look.holes, run.status().substr(0, run.status().find('\n')));
            dialog->Destroy();
            editor.map.clearChanges();
            g_gui.CloseCurrentEditor();
        }
        std::filesystem::remove(file);
    }
    check(paired > 0, "D26 placed no cave ramp in any case");
    check(m19Paired > 0, "D26 placed no m19 cave ramp");
    report << std::format("D26_RAMP_ENTRANCE_PASS runs={} pairedRamps={} m19PairedRamps={} farExitRamps={} rockFaceEntrances={} holeEntrances={}\n",
        runs, paired, m19Paired, farRamps, looks.rockFace, looks.holes);
}

// GUI report after iteration 46: m19, x 1000, y 1000, 512 x 512, seed 1729, 10 rock-face cave entrances, everything else
// at the dialog defaults gave no cave at all. Real dialog and Place; the placed map needs at least one entrance with
// its ramp and cave ground on z + 1. The core counts (candidates, entrances before and after the start-land filter)
// show where entrances go missing.
void m19NoCavesCase(std::ostream& report) {
    UndoLimits undoLimits;
    const auto ids = loadedClientAssetIds();
    check(bool(ids), "M19 caves fixture client assets");
    StyleProfile fixture;
    fixture.client = ids->client; fixture.otbSha256 = ids->otbSha256; fixture.datSha256 = ids->datSha256;
    fixture.tuning = SurfaceTuning{192, 300, 12, 8, 20, 8, 240, 8, 300, 6000, 8};
    fixture.decorTuning = DecorTuning{565, 17, 369, 239, 23, 29};
    fixture.palette = {"grass", "dirt", "mountain", "sand", "sea"};
    fixture.decorPalette = {"trees", "grass tufts", "small rocks"};
    const auto encoded = serializeProfile(fixture);
    check(bool(encoded), "M19 caves fixture serialization");
    const auto directory = styleProfileDirectory();
    const auto relative = std::filesystem::weakly_canonical(directory).lexically_relative(std::filesystem::current_path().parent_path());
    check(!relative.empty() && *relative.begin() != "..", "M19 caves profile must stay in private probe directory");
    std::filesystem::create_directories(directory);
    const auto file = directory / "m19-caves-fixture.profile";
    check(!std::filesystem::exists(file), "M19 caves fixture must not overwrite a profile");
    {std::ofstream out(file); out << *encoded; check(bool(out), "M19 caves profile write");}
    // Core view of the same surface: candidates of the relaxed rock-face rule, entrances without and with the filter.
    SurfaceConfig core{{1000, 1000, 7, 512, 512}, 1729};
    core.tuning = *fixture.tuning;
    core.tuning.caveEntrances = 10; core.tuning.repairReach = 1; core.tuning.shoreSmoothing = 2;
    const auto open = generateSurface(core, MapExtent{65001, 65001, 16}, {600000, 256u << 20, 1u << 30});
    core.tuning.rockBlocks = true;
    const auto closed = generateSurface(core, MapExtent{65001, 65001, 16}, {600000, 256u << 20, 1u << 30});
    check(open && closed, "M19 caves core surface");
    size_t candidates = 0;
    for (uint32_t i = 0; i < open->cells.size(); ++i)
        candidates += caveEntranceAllowed(open->cells, 512, 512, i, CaveEntranceStyle::RockFace, true);
    check(g_gui.NewMap(), "M19 caves GUI map");
    auto& editor = *g_gui.GetCurrentEditor();
    auto* dialog = new TerrainGeneratorDialog(g_gui.root);
    const auto spin = [&](const char* name, int value) {
        auto* c = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, dialog));
        check(c, std::format("M19 caves spin {}", name)); c->SetValue(value);
        wxSpinEvent event(wxEVT_SPINCTRL, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
    };
    const auto choose = [&](const char* name, int index) {
        auto* c = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName(name, dialog));
        check(c && index >= 0 && index < int(c->GetCount()), std::format("M19 caves choice {}", name)); c->SetSelection(index);
        wxCommandEvent event(wxEVT_CHOICE, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
    };
    choose("terrain-style", 1);
    auto* profiles = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-profile", dialog));
    check(profiles, "M19 caves profile control");
    choose("terrain-profile", profiles->FindString("m19-caves-fixture"));
    spin("terrain-x", 1000); spin("terrain-y", 1000); spin("terrain-width", 512); spin("terrain-height", 512);
    spin("terrain-cave-entrances", 10);
    auto* style = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-cave-entrance-style", dialog));
    check(style && style->GetSelection() == 0, "M19 caves rock-face style by default");
    auto* seed = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed", dialog));
    check(seed && seed->GetValue() == "1729", "M19 caves default seed");
    MountainDialogRun run(*dialog, editor, 0, true);
    dialog->ShowModal(); run.stop();
    check(run.placed(), "M19 caves GUI case: " + run.failure() + " " + run.status());
    const auto entranceIds = entranceAlternatives("cave enter");
    const auto* exitDoodad = g_brushes.getBrush("ramp")->as<DoodadBrush>();
    check(exitDoodad, "M19 caves ramp brush");
    const auto exitBrush = resolveCaveExitBrush(*exitDoodad);
    check(bool(exitBrush), "M19 caves ramp items");
    const auto* caveGround = g_brushes.getBrush("cave");
    size_t entrances = 0, paired = 0, caveCells = 0;
    for (int y = 1000; y < 1512; ++y) for (int x = 1000; x < 1512; ++x) {
        const auto* surface = editor.map.getTile(Position(x, y, 7));
        const auto* lower = editor.map.getTile(Position(x, y, 8));
        const bool entrance = surface && std::ranges::any_of(surface->items, [&](const auto& item) {
            return std::ranges::find(entranceIds, item->getID()) != entranceIds.end(); });
        const bool ramp = lower && std::ranges::any_of(lower->items, [&](const auto& item) {
            return std::ranges::any_of(exitBrush->items, [&](const auto& items) { return items.ramp == item->getID(); }); });
        entrances += entrance;
        paired += entrance && ramp;
        caveCells += lower && lower->ground && brushOf(lower->ground->getID()) == caveGround;
    }
    std::string status = run.status();
    std::ranges::replace(status, '\n', ' ');
    report << std::format("M19_CAVES_GUI_CASE candidates={} coreEntrances={} startLandEntrances={} mapEntrances={} withRamp={} caveCells={} status={}\n",
        candidates, open->entrances.size(), closed->entrances.size(), entrances, paired, caveCells, status);
    check(entrances >= 1 && paired == entrances && caveCells > 0, "M19 caves GUI case placed no cave");
    report << "M19_CAVES_GUI_PASS\n";
    dialog->Destroy();
    editor.map.clearChanges();
    g_gui.CloseCurrentEditor();
    std::filesystem::remove(file);
}

// Iteration 48b: the GUI report of an ordinary fallback entrance drawn as the rock-face doodad on grass. Real dialog, m19
// fixture, x 1000, y 1000, 512 x 512, seed 1729, rock-face style, 10 entrances per 65536 cells (40 wanted, the user's
// case) and 15 (60 wanted, more than the rock walls hold, so the ordinary fallback must appear); everything else at the
// dialog defaults. Every entrance passes checkEntranceLook and the dialog removes no place.
void fallbackEntranceLookCase(std::ostream& report) {
    UndoLimits undoLimits;
    const auto ids = loadedClientAssetIds();
    check(bool(ids), "I48b fixture client assets");
    StyleProfile fixture;
    fixture.client = ids->client; fixture.otbSha256 = ids->otbSha256; fixture.datSha256 = ids->datSha256;
    fixture.tuning = SurfaceTuning{192, 300, 12, 8, 20, 8, 240, 8, 300, 6000, 8};
    fixture.decorTuning = DecorTuning{565, 17, 369, 239, 23, 29};
    fixture.palette = {"grass", "dirt", "mountain", "sand", "sea"};
    fixture.decorPalette = {"trees", "grass tufts", "small rocks"};
    const auto encoded = serializeProfile(fixture);
    check(bool(encoded), "I48b fixture serialization");
    const auto directory = styleProfileDirectory();
    const auto relative = std::filesystem::weakly_canonical(directory).lexically_relative(std::filesystem::current_path().parent_path());
    check(!relative.empty() && *relative.begin() != "..", "I48b profile must stay in private probe directory");
    std::filesystem::create_directories(directory);
    const auto file = directory / "i48b-m19-fixture.profile";
    check(!std::filesystem::exists(file), "I48b fixture must not overwrite a profile");
    {std::ofstream out(file); out << *encoded; check(bool(out), "I48b profile write");}
    const auto* exitDoodad = g_brushes.getBrush("ramp")->as<DoodadBrush>();
    check(exitDoodad, "I48b ramp brush");
    const auto exitBrush = resolveCaveExitBrush(*exitDoodad);
    check(bool(exitBrush), "I48b ramp items");
    EntranceLook total;
    for (const int perArea : {10, 15}) {
        check(g_gui.NewMap(), "I48b GUI map");
        auto& editor = *g_gui.GetCurrentEditor();
        auto* dialog = new TerrainGeneratorDialog(g_gui.root);
        const auto spin = [&](const char* name, int value) {
            auto* c = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, dialog));
            check(c, std::format("I48b spin {}", name)); c->SetValue(value);
            wxSpinEvent event(wxEVT_SPINCTRL, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
        };
        const auto choose = [&](const char* name, int index) {
            auto* c = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName(name, dialog));
            check(c && index >= 0 && index < int(c->GetCount()), std::format("I48b choice {}", name)); c->SetSelection(index);
            wxCommandEvent event(wxEVT_CHOICE, c->GetId()); event.SetEventObject(c); c->GetEventHandler()->ProcessEvent(event);
        };
        choose("terrain-style", 1);
        auto* profiles = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-profile", dialog));
        check(profiles, "I48b profile control");
        choose("terrain-profile", profiles->FindString("i48b-m19-fixture"));
        spin("terrain-x", 1000); spin("terrain-y", 1000); spin("terrain-width", 512); spin("terrain-height", 512);
        spin("terrain-cave-entrances", perArea);
        auto* style = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-cave-entrance-style", dialog));
        check(style && style->GetSelection() == 0, "I48b rock-face style by default");
        auto* seed = dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed", dialog));
        check(seed, "I48b seed"); seed->SetValue("1729");
        MountainDialogRun run(*dialog, editor, 0, true);
        dialog->ShowModal(); run.stop();
        const auto label = std::format("I48b entrances={}", perArea);
        check(run.placed(), label + ": " + run.failure() + " " + run.status());
        std::string status = run.status();
        std::ranges::replace(status, '\n', ' ');
        check(status.find("removed places: 0.") != std::string::npos, label + " removed places: " + status);
        const auto look = checkEntranceLook(editor.map, 1000, 1000, 7, 512, 512, g_brushes.getBrush("mountain"), *exitBrush, label);
        check(look.rockFace > 0, label + " placed no rock-face entrance");
        if (perArea == 15) check(look.holes > 0, label + " placed no ordinary fallback entrance");
        total.rockFace += look.rockFace; total.holes += look.holes;
        report << std::format("FALLBACK_ENTRANCE_LOOK_CASE {} rockFace={} holes={} status={}\n", label, look.rockFace, look.holes, status);
        dialog->Destroy();
        editor.map.clearChanges();
        g_gui.CloseCurrentEditor();
    }
    std::filesystem::remove(file);
    report << std::format("FALLBACK_ENTRANCE_LOOK_PASS rockFace={} holes={}\n", total.rockFace, total.holes);
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
        m19NoCavesCase(report);
        fallbackEntranceLookCase(report);
        d26RampCase(report);
        dungeonGuiProbe(report);
        buildingStairsGuiProbe(report);
        storeySealedRampRepair(report);
        m19RockGuiCase(report);
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
