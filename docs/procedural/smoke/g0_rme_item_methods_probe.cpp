// Test-only module, linked separately from the canonical editor.
#include "app/main.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "ui/map_tab.h"
#include "app/managers/version_manager.h"
#include "editor/editor.h"
#include "editor/editor_factory.h"
#include "editor/persistence/map_load_options.h"
#include "map/map.h"
#include "game/complexitem.h"
#include "lua/lua_script_manager.h"
#include <wx/module.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace {
namespace fs = std::filesystem;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void run() {
    const auto root = fs::current_path();
    std::ofstream report(root / "snapshot-probe.log");
    report << std::unitbuf;
    try {
        check(root.filename() == "run" && fs::is_regular_file(root / ".g0-item-methods") &&
            fs::is_regular_file(root / ".g0-snapshot-fixture"), "Private item-method fixture required");
        check(wxApp::IsMainLoopRunning() && g_gui.root->IsEnabled(), "Main loop must be ready");
        wxString error;
        std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Client load failed");
        MapLoadOptions options;
        options.selected_client_id = "1098-BlackTek";
        auto owned = EditorFactory::LoadFromFile(g_gui.copybuffer,
            FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
        check(owned && owned->map.getTileCount() == 402, "Approved fixture not loaded");
        auto* editor = owned.get();
        g_gui.FinishWelcomeDialog();
        // wxWidgets owns this child window; MapTab owns the transferred Editor.
        new MapTab(g_gui.tabbook, owned.release());
        check(g_gui.GetCurrentEditor() == editor, "Fixture is not the active editor");
        check(g_luaScripts.isInitialized() || g_luaScripts.initialize(), "Lua initialization failed");
        auto& engine = g_luaScripts.getEngine();
        engine.setPrintCallback([&report](const std::string& text) { report << text << '\n'; });
        auto oracle = engine.getState().create_named_table("g0_methods");
        // Read-only C++ oracle: the public Lua API has no Podium outfit getters.
        oracle["podiumState"] = [editor](sol::this_state state) {
            const auto* tile = editor->map.getTile(110, 110, 7);
            check(tile != nullptr, "Missing method-test tile");
            const Podium* podium = nullptr;
            for (const auto& item : tile->items) {
                if (item->getID() == 1443) podium = item->asPodium();
            }
            check(podium != nullptr, "XML loader/factory did not create a Podium");
            const auto& outfit = podium->getOutfit();
            sol::state_view lua(state);
            auto result = lua.create_table();
            result["lookType"] = outfit.lookType;
            result["lookItem"] = outfit.lookItem;
            result["lookMount"] = outfit.lookMount;
            result["lookAddon"] = outfit.lookAddon;
            result["lookHead"] = outfit.lookHead;
            result["lookBody"] = outfit.lookBody;
            result["lookLegs"] = outfit.lookLegs;
            result["lookFeet"] = outfit.lookFeet;
            result["lookMountHead"] = outfit.lookMountHead;
            result["lookMountBody"] = outfit.lookMountBody;
            result["lookMountLegs"] = outfit.lookMountLegs;
            result["lookMountFeet"] = outfit.lookMountFeet;
            result["direction"] = podium->getDirection();
            result["showOutfit"] = podium->getShowOutfit();
            result["showMount"] = podium->getShowMount();
            result["showPlatform"] = podium->getShowPlatform();
            return result;
        };
        if (!engine.executeFile((root / "test_item_methods.lua").string())) {
            throw std::runtime_error(engine.getLastError());
        }
        engine.getState()["g0_methods"] = sol::nil;
        engine.setPrintCallback({});
        g_gui.CloseCurrentEditor();
        check(g_gui.GetCurrentEditor() != editor, "Test editor did not close");
        report << "ITEM_METHODS_PASS\n";
        report.close();
        std::_Exit(0); // Tested editor destroyed; global application shutdown is outside this probe.
    } catch (const std::exception& error) {
        report << "FAILED\t" << error.what() << '\n';
        report.close();
        std::_Exit(9);
    }
}
}
class G0ItemMethodsModule : public wxModule {
public:
    bool OnInit() override {
        wxTheApp->Bind(wxEVT_IDLE, &G0ItemMethodsModule::OnIdle, this);
        return true;
    }
    void OnExit() override {}
    void OnIdle(wxIdleEvent& event) {
        event.Skip();
        if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
        wxTheApp->Unbind(wxEVT_IDLE, &G0ItemMethodsModule::OnIdle, this);
        wxTheApp->CallAfter([] { run(); });
    }
private:
    wxDECLARE_DYNAMIC_CLASS(G0ItemMethodsModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G0ItemMethodsModule, wxModule);
