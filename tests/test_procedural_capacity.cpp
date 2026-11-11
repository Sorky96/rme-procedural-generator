// SPDX-License-Identifier: GPL-3.0-or-later
// Opt-in loaded-client capacity probe. Uses a private synthetic empty map only.
#include "app/main.h"
#include "procedural_probe_warnings.h"
#include "app/managers/version_manager.h"
#include "app/settings.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/operations/draw_operations.h"
#include "map/map.h"
#include "procedural/adapter/snapshot_equal.h"
#include "ui/dialogs/terrain_generator_limits.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include <wx/module.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace rme::procedural;
namespace limits = rme::ui::terrain_generator;

void check(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

void capacity(std::ostream& report) {
    uint32_t side = 0;
    std::ifstream("capacity-side.txt") >> side;
    check(side >= 500 && side <= uint32_t(limits::maxSide), "Invalid capacity side");
    check(g_gui.NewMap(), "Capacity source map");
    auto& editor = *g_gui.GetCurrentEditor();
    editor.map.setWidth(int(side + 200));
    editor.map.setHeight(int(side + 200));
    GenerationConfig config {{100, 100, 7, side, side}, 1729};
    config.algorithm = GenerationAlgorithm::Surface;
    config.surface.rockPermille = 300;
    config.surface.plateauMinTop = 64;
    config.surface.plateauMaxPercent = 10;
    config.surface.plateauLevels = 3;
    config.surface.caveEntrances = 4;
    config.surface.repairReach = 1;
    config.vegetation = true;
    GroundProfile profile {
        {"grass", "dirt", "rocky ground", "sand", "sea", "sea", "cobblestone", "stone floor"}, GroundProfile::surfaceWithFloorCount
    };
    profile.decor = {"small rocks", "small rocks", "small rocks"};
    profile.decorCount = 3;
    profile.plateauGround = "grass";
    profile.plateauRamp = "ramp";
    profile.caveEntrance = "cave enter";
    profile.caveExit = "ramp";
    profile.caveGround = "cave";
    profile.caveWall = "mountain";

    const auto started = std::chrono::steady_clock::now();
    report << std::format("CAPACITY_START\tside={}\n", side);
    auto prepared = prepareTerrain(editor.map, config, profile, limits::preparationBudget);
    check(bool(prepared), prepared ? "" : std::format("Capacity preparation domain {} error {}", prepared.error().index(),
        std::visit([](auto e) { return int(e); }, prepared.error())));
    std::vector<const PreparedTerrain*> units {&*prepared};
    if (prepared->cave) units.push_back(prepared->cave.get());
    for (auto* p = prepared->plateau.get(); p; p = p->plateau.get()) units.push_back(p);
    uint64_t changes = 0;
    for (const auto* unit : units) {
        changes += unit->preview.getChanges().size();
        report << std::format("CAPACITY_FLOOR\tz={}\tchanges={}\n", unit->plan.config.region.z, unit->preview.getChanges().size());
    }
    report << std::format("CAPACITY_PREPARED\tside={}\tfloors={}\tchanges={}\testimate={}\tms={}\n", side, units.size(), changes,
        prepared->memory.totalBytes, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());

    // Private test settings; never change the user's preferences.
    g_settings.setInteger(Config::UNDO_MEM_SIZE, 4096);
    g_settings.setInteger(Config::UNDO_SIZE, 400);
    const auto commitStarted = std::chrono::steady_clock::now();
    auto committed = DrawOperations::commitTerrainPreview(*prepared, config, profile, limits::commitBudget);
    check(bool(committed), committed ? "" : std::format("Capacity commit domain {} error {}", committed.error().index(),
        std::holds_alternative<TerrainCommitFailure>(committed.error()) ? int(std::get<TerrainCommitFailure>(committed.error())) : -1));
    report << std::format("CAPACITY_COMMITTED\testimate={}\ttransition={}\tms={}\n", committed->totalBytes, committed->transitionBytes,
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - commitStarted).count());
    const auto checkPayload = [&] {
        for (const auto* unit : units) for (const auto& pos : unit->preview.getChanges())
            check(snapshotTilesEqual(editor.map.getTile(pos), unit->preview.getBuffer().getTile(pos)), "Capacity payload differs from preview");
    };
    checkPayload();
    const auto after = editor.map.getTileCount();
    for (size_t i = 0; i < units.size(); ++i) check(editor.actionQueue->undo(), "Capacity Undo");
    check(editor.map.getTileCount() == 0, "Capacity Undo did not restore empty source");
    for (size_t i = 0; i < units.size(); ++i) check(editor.actionQueue->redo(), "Capacity Redo");
    check(editor.map.getTileCount() == after, "Capacity Redo tile count");
    checkPayload();
    editor.actionQueue->clear();
    editor.map.clearChanges();
    report << "CAPACITY_PASS\n";
}

void run() {
    std::ofstream report("resolver-probe.log");
    report << std::unitbuf;
    try {
        check(std::filesystem::is_regular_file(".g1-resolver-probe"), "Private capacity probe required");
        wxString error;
        std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Capacity client load");
        capacity(report);
        report.close();
        wxTheApp->CallAfter([] { g_gui.root->Close(); });
    } catch (const std::exception& error) {
        report << "FAILED\t" << error.what() << '\n';
        report.close();
        std::_Exit(9);
    }
}

class CapacityModule final : public wxModule {
public:
    bool OnInit() override { wxTheApp->Bind(wxEVT_IDLE, &CapacityModule::idle, this); return true; }
    void OnExit() override {}
    void idle(wxIdleEvent& event) {
        event.Skip();
        if (dismissStartupWarningsDialog()) return;
        if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
        wxTheApp->Unbind(wxEVT_IDLE, &CapacityModule::idle, this);
        wxTheApp->CallAfter([] { run(); });
    }
private:
    wxDECLARE_DYNAMIC_CLASS(CapacityModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(CapacityModule, wxModule);
} // namespace
