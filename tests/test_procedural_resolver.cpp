// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only wxModule linked with the current RME objects, never into normal RME.
#include "app/main.h"
#include "procedural/adapter/ground_resolver.h"
#include "app/managers/version_manager.h"
#include "brushes/ground/ground_brush.h"
#include "map/map.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include <wx/module.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <thread>

std::mt19937& getRandomGenerator();

namespace {
using namespace rme::procedural;
void check(bool condition, const char* message) {
	if (!condition) throw std::runtime_error(message);
}

void oneSize(uint32_t side, std::ostream& report) {
	Map map;
	map.initializeEmpty();
	const GenerationConfig config {{100, 100, 7, side, side}, 1729};
	const GroundProfile profile {{{"grass", "dirt", "clear water"}}};
	constexpr CoreBudget coreBudget {250000, 1024 * 1024};
	constexpr uint64_t inputBudget = 1024 * 1024, planBudget = 1024 * 1024;
	std::vector<uint8_t> mask(size_t(side) * side, 1);
	for (size_t i = 0; i < mask.size(); i += 11) mask[i] = 0;
	map.createTileL(105, 105, 7)->increaseTownCount();
	auto* protectedTile = map.createTile(106, 105, 7);
	const auto* grassBase = g_brushes.getBrush(profile.brushes[0]);
	const auto* grass = grassBase ? grassBase->as<GroundBrush>() : nullptr;
	check(grass && !grass->getBorderItems().empty(), "Missing grass fixture brush");
	protectedTile->setGround(Item::Create(grass->getBorderItems().front().id));
	check(bool(protectedTile->ground), "Fixture ground creation failed");
	protectedTile->ground->setUniqueID(7001);
	protectedTile->ground->setActionID(6001);
	protectedTile->mapflags = TILESTATE_PROTECTIONZONE;
	const auto tileCount = map.getTileCount(), revision = map.getChangeTracker().getGeneration();
	const auto input = captureGenerationInput(map, config, coreBudget, inputBudget, mask);
	check(bool(input), "Capture failed");
	const auto semantic = generateTerrain(config, input->extent, coreBudget, input->mask, input->protection);
	check(bool(semantic), "Semantic generation failed");
	const auto started = std::chrono::steady_clock::now();
	const auto randomBefore = getRandomGenerator();
	const auto plan = resolveGroundPlan(*input, *semantic, profile, planBudget);
	const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
	if (!plan) report << "RESOLVE_ERROR\t" << int(plan.error()) << '\n';
	check(bool(plan), "Loaded-client ground resolution failed");
	check(getRandomGenerator() == randomBefore, "Resolver advanced the global brush RNG");
	const auto again = resolveGroundPlan(*input, *semantic, profile, planBudget);
	check(again && plan->items == again->items && plan->fingerprint() == again->fingerprint(), "Concrete placements not deterministic");
	check(plan->client == "1098-BlackTek" && plan->mapGeneration == map.getGeneration() && plan->mapRevision == revision &&
		plan->assetRevision == g_version.GetAssetRevision(),
		"Plan provenance missing");
	for (size_t i = 0; i < plan->items.size(); ++i) {
		check((plan->items[i] != 0) == (input->mask[i] && !input->protection[i]), "Plan wrote an excluded/protected cell");
		if (!plan->items[i]) continue;
		const auto terrain = size_t(semantic->cells[i]) - 1;
		const auto* brush = g_brushes.getBrush(profile.brushes[terrain])->as<GroundBrush>();
		check(std::ranges::any_of(brush->getBorderItems(), [&](const auto& variant) { return variant.id == plan->items[i]; }),
			"Plan chose an item outside the mapped brush");
	}
	const auto fullInput = captureGenerationInput(map, config, coreBudget, inputBudget);
	check(bool(fullInput), "Full-mask capture failed");
	const auto fullSemantic = generateTerrain(config, fullInput->extent, coreBudget, fullInput->mask, fullInput->protection);
	check(bool(fullSemantic), "Full-mask semantic generation failed");
	const auto fullPlan = resolveGroundPlan(*fullInput, *fullSemantic, profile, planBudget);
	check(bool(fullPlan), "Full-mask resolution failed");
	for (size_t i = 0; i < plan->items.size(); ++i) {
		if (plan->items[i]) check(plan->items[i] == fullPlan->items[i], "Mask changed another cell's variant");
	}
	const uint64_t exactBytes = sizeof(GroundPlan) + uint64_t(plan->items.size()) * sizeof(uint16_t) +
		plan->client.size() + 1 + profile.brushes[0].size() + profile.brushes[1].size() + profile.brushes[2].size() + 3;
	check(bool(resolveGroundPlan(*input, *semantic, profile, exactBytes)), "Exact plan budget rejected");
	const auto shortPlan = resolveGroundPlan(*input, *semantic, profile, exactBytes - 1);
	check(!shortPlan && shortPlan.error() == ResolveError::PlanLimit, "Plan budget not enforced");
	auto badProfile = profile;
	badProfile.brushes[0] = "__missing_g1_brush__";
	const auto missing = resolveGroundPlan(*input, *semantic, badProfile, planBudget);
	check(!missing && missing.error() == ResolveError::InvalidProfile, "Missing brush silently substituted");
	const auto invalidId = grass->getBorderItems().front().id;
	g_item_definitions.setMetaItem(invalidId, true);
	const auto invalidItem = resolveGroundPlan(*input, *semantic, profile, planBudget);
	g_item_definitions.setMetaItem(invalidId, false);
	check(!invalidItem && invalidItem.error() == ResolveError::InvalidItem, "Invalid mapped item silently substituted");
	auto badGrid = *semantic;
	badGrid.cells[0] = Terrain::Grass; // Mask excludes this cell.
	const auto escaped = resolveGroundPlan(*input, badGrid, profile, planBudget);
	check(!escaped && escaped.error() == ResolveError::InvalidInput, "Tampered mask output accepted");
	std::stop_source cancel;
	cancel.request_stop();
	const auto cancelled = resolveGroundPlan(*input, *semantic, profile, planBudget, cancel.get_token());
	check(!cancelled && cancelled.error() == ResolveError::Cancelled, "Cancelled resolution returned a plan");
	bool workerRejected = false;
	std::jthread worker([&] {
		const auto result = resolveGroundPlan(*input, *semantic, profile, planBudget);
		workerRejected = !result && result.error() == ResolveError::WrongThread;
	});
	worker.join();
	check(workerRejected, "Resolver accessed loaded assets from worker");
	check(map.getTileCount() == tileCount && map.getChangeTracker().getGeneration() == revision &&
		protectedTile->mapflags == TILESTATE_PROTECTIONZONE && protectedTile->ground->getUniqueID() == 7001 &&
		protectedTile->ground->getActionID() == 6001 && map.getTileL(105,105,7)->getTownCount() == 1,
		"Resolution mutated live map/protected data");
	report << "RESOLVER_SIZE_PASS\t" << side << '\t' << plan->items.size() << '\t' << elapsed << '\t' << plan->fingerprint() << '\n';
}

void assetRevision(std::ostream& report) {
	VersionManager freshManager;
	check(freshManager.GetAssetRevision() == 0, "Revision not initialized");
	Map map;
	const GenerationConfig config {{100,100,7,4,3}, 1729};
	const GroundProfile profile {{{"grass", "dirt", "clear water"}}};
	const CoreBudget budget {12,1024};
	const auto before = g_version.GetAssetRevision();
	wxString error;
	std::vector<std::string> warnings;
	check(g_version.LoadVersion("1098-BlackTek", error, warnings), "No-op load failed");
	check(before == g_version.GetAssetRevision(), "No-op load changed revision");
	const auto input = captureGenerationInput(map, config, budget, 4096);
	check(bool(input), "Revision input capture failed");
	const auto semantic = generateTerrain(config, input->extent, budget, input->mask, input->protection);
	check(bool(semantic), "Revision semantic generation failed");
	const auto original = resolveGroundPlan(*input, *semantic, profile, 4096);
	check(bool(original), "Revision original plan failed");
	check(g_version.LoadVersion("1098-BlackTek", error, warnings, true), "Forced same-client reload failed");
	check(g_version.GetAssetRevision() > before, "Forced reload retained revision");
	const auto stale = resolveGroundPlan(*input, *semantic, profile, 4096);
	check(!stale && stale.error() == ResolveError::StaleAssets, "Pre-reload input accepted");
	const auto refreshed = captureGenerationInput(map, config, budget, 4096);
	check(bool(refreshed), "Refreshed capture failed");
	const auto repeated = resolveGroundPlan(*refreshed, *semantic, profile, 4096);
	check(repeated && original->items == repeated->items && original->fingerprint() == repeated->fingerprint(),
		"Same assets changed deterministic placements after reload");
	// Both absolute paths are explicit children of the checked private probe directory.
	const auto root = std::filesystem::current_path();
	check(std::filesystem::is_regular_file(root / ".g1-resolver-probe"), "Private reload directory required");
	const auto assets = root / "data" / "1098", held = root / "data" / "1098.g1-hold";
	check(assets.is_absolute() && held.is_absolute() && assets.parent_path() == root / "data" &&
		held.parent_path() == assets.parent_path() && !std::filesystem::exists(held), "Unsafe private reload paths");
	const auto loadedRevision = g_version.GetAssetRevision();
	std::filesystem::rename(assets, held);
	bool loaded = false;
	try { loaded = g_version.LoadVersion("1098-BlackTek", error, warnings, true); }
	catch (...) { std::filesystem::rename(held, assets); throw; }
	std::filesystem::rename(held, assets);
	check(!loaded && !g_version.IsVersionLoaded() && g_version.GetAssetRevision() > loadedRevision,
		"Failed reload did not invalidate revision/client");
	const auto failed = resolveGroundPlan(*refreshed, *semantic, profile, 4096);
	check(!failed && failed.error() == ResolveError::NoClient, "Failed reload retained usable old input");
	check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Reload recovery failed");
	const auto restored = g_version.GetAssetRevision();
	g_version.UnloadVersion();
	check(g_version.GetAssetRevision() > restored, "Explicit unload did not advance revision");
	const auto unloaded = g_version.GetAssetRevision();
	g_version.UnloadVersion();
	check(g_version.GetAssetRevision() > unloaded, "Empty unload did not advance revision");
	report << "ASSET_REVISION_PASS\t" << before << '\t' << g_version.GetAssetRevision() << '\n';
}

void run() {
	const auto root = std::filesystem::current_path();
	std::ofstream report(root / "resolver-probe.log");
	report << std::unitbuf;
	try {
		check(std::filesystem::is_regular_file(root / ".g1-resolver-probe"), "Private probe required");
		g_version.UnloadVersion();
		const auto noClient = resolveGroundPlan({}, {}, {}, 1024);
		check(!noClient && noClient.error() == ResolveError::NoClient, "Missing client accepted");
		wxString error;
		std::vector<std::string> warnings;
		check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Private client load failed");
		oneSize(100, report);
		oneSize(500, report);
		assetRevision(report);
		report << "RESOLVER_PASS\n";
		report.close();
		wxTheApp->CallAfter([] { g_gui.root->Close(); });
	} catch (const std::exception& error) {
		report << "FAILED\t" << error.what() << '\n';
		report.close();
		std::_Exit(9);
	}
}
}

class G1ResolverModule : public wxModule {
public:
	bool OnInit() override {
		wxTheApp->Bind(wxEVT_IDLE, &G1ResolverModule::OnIdle, this);
		return true;
	}
	void OnExit() override {}
	void OnIdle(wxIdleEvent& event) {
		event.Skip();
		if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
		wxTheApp->Unbind(wxEVT_IDLE, &G1ResolverModule::OnIdle, this);
		wxTheApp->CallAfter([] { run(); });
	}
private:
	wxDECLARE_DYNAMIC_CLASS(G1ResolverModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G1ResolverModule, wxModule);
