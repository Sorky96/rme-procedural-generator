// Test-only scalar allocator replacement; never link into the normal editor.
#include "app/main.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "ui/map_tab.h"
#include "app/managers/version_manager.h"
#include "editor/editor.h"
#include "editor/editor_factory.h"
#include "editor/action_queue.h"
#include "editor/persistence/map_load_options.h"
#include "editor/persistence/map_backup_service.h"
#include "editor/persistence/editor_persistence.h"
#include "map/map.h"
#include "game/item.h"
#include "game/house.h"
#include "lua/lua_script_manager.h"
#include <wx/module.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <new>
#include <array>

namespace {
thread_local int failOrdinal = 0;
thread_local int allocationCount = 0;
thread_local bool recording = false;
thread_local bool faultObserved = false;
}
void* operator new(std::size_t size) {
    if (recording && ++allocationCount == failOrdinal) {
        faultObserved = true;
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
namespace fs = std::filesystem;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
const std::array<Position, 5> positions {{
    {100,100,7}, {104,102,7}, {110,110,7}, {160,160,6}, {160,160,7}
}};
int oneCase(const fs::path& root, int ordinal, std::ostream& report) {
    MapLoadOptions options;
    options.selected_client_id = "1098-BlackTek";
    auto owned = EditorFactory::LoadFromFile(g_gui.copybuffer,
        FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
    check(owned && owned->map.getTileCount() == 402, "Approved fixture not loaded");
    auto* editor = owned.get();
    std::array<uint16_t, positions.size()> originalAids {};
    std::array<uint32_t, positions.size()> originalHouses {};
    const bool originalDirty = editor->map.hasChanged();
    for (size_t i = 0; i < positions.size(); ++i) {
        auto tile = editor->map.getTile(positions[i]);
        check(tile && tile->ground, "Missing baseline ground");
        originalAids[i] = tile->ground->getActionID();
        originalHouses[i] = tile->getHouseID();
    }
    g_gui.FinishWelcomeDialog();
    new MapTab(g_gui.tabbook, owned.release()); // wx parent owns the tab, which owns Editor.
    check(g_gui.GetCurrentEditor() == editor, "Wrong active test editor");
    auto& engine = g_luaScripts.getEngine();
    auto fault = engine.getState().create_named_table("g0_fault");
    fault["arm"] = [ordinal] {
        allocationCount = 0;
        faultObserved = false;
        failOrdinal = ordinal;
        recording = true;
    };
    fault["disarm"] = [] { recording = false; };
    const std::string code = R"lua(
        g0_case_ok, g0_case_error = pcall(function()
            app.transaction("allocation fixture", function()
                local map = app.map
                for _, p in ipairs({{100,100,7},{104,102,7},{110,110,7},{160,160,6},{160,160,7}}) do
                    map:getTile(p[1],p[2],p[3]).ground.actionId = 12345
                end
                g0_fault.arm()
            end)
        end)
        g0_fault.disarm()
    )lua";
    const bool executed = engine.executeString(code, "g0 allocation case");
    recording = false;
    const int allocations = allocationCount;
    check(executed, "Outer protected test script failed");
    const bool success = engine.getState()["g0_case_ok"].get<bool>();
    report << "LUA_ALLOCATION_CASE\t" << ordinal << "\tallocations=" << allocations
        << "\tfault=" << faultObserved << "\tsuccess=" << success
        << "\ttiles=" << editor->map.getTileCount()
        << "\tdirty=" << editor->map.hasChanged()
        << "\thistory=" << editor->actionQueue->getCurrentIndex() << '/' << editor->actionQueue->getSize() << '\n';
    if (!success) {
        report << "LUA_ALLOCATION_ERROR\t" << engine.getState()["g0_case_error"].get<std::string>() << '\n';
    }
    if (success) {
        // A reported GUI notification failure may leave a complete, undoable
        // commit. The runner also requires the production diagnostic in stderr.
        for (const auto& pos : positions) {
            auto tile = editor->map.getTile(pos);
            check(tile && tile->ground && tile->ground->getActionID() == 12345,
                "Successful callback did not apply every intended setter");
        }
        check(editor->actionQueue->getCurrentIndex() == 1 && editor->actionQueue->getSize() == 1,
            "Successful commit did not create exactly one action");
        check(editor->actionQueue->undo(), "Successful baseline Undo failed");
    }
    // Snapshot even a damaged result, so the independent reader can inspect the failure.
    std::string snapshot, error;
    check(MapBackupService::CreateSnapshot(*editor, snapshot, error, "lua_alloc_" + std::to_string(ordinal)),
        "Could not snapshot the actual post-case map");
    report << "LUA_ALLOCATION_SNAPSHOT\t" << ordinal << '\t' << snapshot << '\n';
    check(editor->map.getTileCount() == 402, "Allocation failure or Undo lost an existing tile");
    for (size_t i = 0; i < positions.size(); ++i) {
        auto tile = editor->map.getTile(positions[i]);
        check(tile && tile->ground && tile->ground->getActionID() == originalAids[i] &&
            tile->getHouseID() == originalHouses[i],
            "Allocation failure or Undo did not restore original ground attributes/house ID");
    }
    const auto house = editor->map.houses.getHouse(1);
    check(house && house->getTiles().size() == 1 && house->size() == 1 &&
        house->getTiles().front() == Position(104,102,7), "House index was lost or duplicated");
    if (!success) {
        check(faultObserved && editor->actionQueue->getCurrentIndex() == 0 && editor->actionQueue->getSize() == 0,
            "Failed transaction changed history");
        check(editor->map.hasChanged() == originalDirty, "Failed transaction changed map dirty state");
    }
    check(engine.executeString("app.transaction('after allocation', function() end)", "g0 allocation retry"),
        "Transaction remained active after allocation case");
    engine.getState()["g0_fault"] = sol::nil;
    engine.getState()["g0_case_ok"] = sol::nil;
    engine.getState()["g0_case_error"] = sol::nil;
    g_gui.CloseCurrentEditor();
    report << "LUA_ALLOCATION_RESTORED\t" << ordinal << '\n';
    return allocations;
}
void smoke400(const fs::path& root, std::ostream& report) {
    auto owned = EditorFactory::CreateEmpty(g_gui.copybuffer);
    check(owned && owned->map.getTileCount() == 0, "Expected a real new empty smoke map");
    auto* editor = owned.get();
    new MapTab(g_gui.tabbook, owned.release());
    auto& engine = g_luaScripts.getEngine();
    engine.setPrintCallback([&report](const std::string& text) { report << text << '\n'; });
    const auto execute = [&engine, &root](const char* filename) {
        if (!engine.executeFile((root / filename).string())) throw std::runtime_error(engine.getLastError());
    };
    execute("g0_smoke.lua");
    check(editor->map.getTileCount() == 400 && editor->actionQueue->getCurrentIndex() == 1 &&
        editor->actionQueue->getSize() == 1, "400-tile smoke was not one action");
    const auto ground = editor->map.getTile(100,100,7)->ground->getID();
    const auto target = root / "g0-smoke.otbm";
    check(!fs::exists(target) && EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false),
        "Could not save private 400-tile smoke through normal persistence");
    execute("g0_verify_saved.lua");
    check(editor->actionQueue->undo(), "400-tile Undo failed");
    execute("g0_smoke.lua");
    check(editor->map.getTileCount() == 0, "400-tile Undo retained tiles");
    check(editor->actionQueue->redo(), "400-tile Redo failed");
    execute("g0_smoke.lua");
    execute("g0_verify_saved.lua");
    check(editor->actionQueue->undo(), "Final 400-tile Undo failed");
    engine.getState()["G0_SMOKE_MAP"] = sol::nil;
    engine.getState()["G0_SMOKE_STAGE"] = sol::nil;
    g_gui.CloseCurrentEditor();
    MapLoadOptions options;
    options.selected_client_id = "1098-BlackTek";
    auto reopened = EditorFactory::LoadFromFile(g_gui.copybuffer, FileName(wxString(target.wstring())), options);
    check(reopened && reopened->map.getTileCount() == 400, "400-tile saved readback failed");
    new MapTab(g_gui.tabbook, reopened.release());
    execute("g0_verify_saved.lua");
    g_gui.CloseCurrentEditor();
    engine.setPrintCallback({});
    report << "LUA_ALLOCATION_SMOKE_400_PASS\t" << ground << '\t' << target.string() << '\n';
}
void run() {
    const auto root = fs::current_path();
    std::ofstream report(root / "snapshot-probe.log");
    report << std::unitbuf;
    try {
        check(root.filename() == "run" && fs::is_regular_file(root / ".g0-lua-allocation") &&
            fs::is_regular_file(root / ".g0-snapshot-fixture"), "Private Lua allocation fixture required");
        check(wxApp::IsMainLoopRunning() && g_gui.root->IsEnabled(), "Main loop must be ready");
        wxString error;
        std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Client load failed");
        check(g_luaScripts.isInitialized() || g_luaScripts.initialize(), "Lua initialization failed");
        const int baseline = oneCase(root, 0, report);
        check(baseline > 0 && baseline < 256, "Unexpected allocation count; inspect before expanding the sweep");
        for (int ordinal = 1; ordinal <= baseline + 8; ++ordinal) oneCase(root, ordinal, report);
        smoke400(root, report);
        report << "LUA_ALLOCATION_PASS\n";
        report.close();
        std::_Exit(0); // Tested editors destroyed; global shutdown is outside this probe.
    } catch (const std::exception& error) {
        recording = false;
        report << "FAILED\t" << error.what() << '\n';
        report.close();
        std::_Exit(9);
    }
}
}
class G0LuaAllocationModule : public wxModule {
public:
    bool OnInit() override {
        wxTheApp->Bind(wxEVT_IDLE, &G0LuaAllocationModule::OnIdle, this);
        return true;
    }
    void OnExit() override {}
    void OnIdle(wxIdleEvent& event) {
        event.Skip();
        if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
        wxTheApp->Unbind(wxEVT_IDLE, &G0LuaAllocationModule::OnIdle, this);
        wxTheApp->CallAfter([] { run(); });
    }
private:
    wxDECLARE_DYNAMIC_CLASS(G0LuaAllocationModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G0LuaAllocationModule, wxModule);
