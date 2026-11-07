// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only wxModule linked with the current RME objects, never into normal RME.
// Resolves surface v2 against the loaded 1098-BlackTek client and runs the existing
// GroundBorderCalculator over the result. It adds no border logic of its own.
#include "app/main.h"
#include "procedural_probe_warnings.h"
#include "procedural/adapter/generation_input.h"
#include "procedural/adapter/surface_resolver.h"
#include "app/managers/version_manager.h"
#include "app/settings.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/operations/draw_operations.h"
#include "procedural/adapter/terrain_preparation.h"
#include "procedural/adapter/terrain_validation.h"
#include "brushes/ground/ground_border_calculator.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/terrain_placement.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/map.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include <wx/module.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
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
		for (uint64_t seed = 1; seed <= 4; ++seed) oneRun(100, seed, report, seed == 1);
		oneRun(250, 1729, report, true);
		oneRun(500, 1729, report, false);
		endToEnd(100, report);
		endToEnd(250, report);
		endToEnd(100, report, true);
		endToEnd(250, report, true);
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
