// Test-only module: cancel actual serialization through its wx dialog event.
#include "app/main.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "app/managers/version_manager.h"
#include "editor/editor.h"
#include "editor/editor_factory.h"
#include "editor/persistence/map_load_options.h"
#include "editor/persistence/editor_persistence.h"
#include "map/map.h"
#include "util/file_system.h"
#include <wx/module.h>
#include <wx/eventfilter.h>
#include <wx/generic/progdlgg.h>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <cstdlib>

namespace {
namespace fs = std::filesystem;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::string readFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
auto metadata(Map& map) {
    return std::tuple(map.getFilename(), map.getName(), map.getHouseFilename(), map.getSpawnFilename(),
        map.getMonsterFilename(), map.getNpcFilename(), map.getZoneFilename(), map.getWaypointFilename(),
        map.hasChanged(), map.isUnnamed(), map.getTileCount());
}
class CancelAfterCreation final : public wxEventFilter {
public:
    explicit CancelAfterCreation(fs::path target) : target(std::move(target)) { wxEvtHandler::AddFilter(this); }
    ~CancelAfterCreation() override { wxEvtHandler::RemoveFilter(this); }
    int FilterEvent(wxEvent& incoming) override {
        std::error_code ec;
        if (fired || incoming.GetEventType() != wxEVT_PAINT || !fs::is_regular_file(target, ec) ||
            fs::file_size(target, ec) > 4 || ec) return Event_Skip;
        for (auto node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext()) {
            auto* dialog = dynamic_cast<wxGenericProgressDialog*>(node->GetData());
            if (!dialog || !dialog->IsShown() || dialog->GetTitle() != "Saving") continue;
            auto* button = dialog->FindWindow(wxID_CANCEL);
            if (!button || !button->IsEnabled()) continue;
            fired = true; // Reentrant command dispatch must not send twice.
            wxCommandEvent event(wxEVT_BUTTON, wxID_CANCEL);
            event.SetEventObject(button);
            handled = button->GetEventHandler()->ProcessEvent(event);
            break;
        }
        return Event_Skip;
    }
    bool fired = false;
    bool handled = false;
private:
    fs::path target;
};
void runCases(const fs::path& root, std::ostream& report) {
    MapLoadOptions options;
    options.selected_client_id = "1098-BlackTek";
    auto editor = EditorFactory::LoadFromFile(g_gui.copybuffer,
        FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
    check(editor && editor->map.getTileCount() == 402, "Expected approved synthetic map");
    const auto directory = root / "cancel-save";
    check(fs::create_directory(directory), "Expected new private directory");
    const auto target = directory / "cancelled.otbm";
    const auto house = directory / "cancelled-house.xml";
    const auto spawn = directory / "cancelled-spawn.xml";
    const auto marker = fs::path(FileSystem::GetLocalDataDirectory().ToStdWstring()) / ".saving.txt";
    auto& map = editor->map;
    map.doChange();
    const auto beforeNew = metadata(map);
    {
        CancelAfterCreation cancel(target);
        check(!EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), true), "New-target cancellation unexpectedly succeeded");
        check(cancel.fired && cancel.handled, "Actual wx Cancel command was not handled after target creation");
    }
    check(metadata(map) == beforeNew && !fs::exists(target) && !fs::exists(house) && !fs::exists(spawn) &&
        !fs::exists(marker) && !g_gui.IsLoading() && !g_gui.WasLoadCancelled(), "New-target cancel did not restore state/cleanup/reset cancellation");
    report << "SAVE_CANCEL_NEW_PASS\n";
    check(EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false), "Retry after new-target cancellation failed");
    auto reopened = EditorFactory::LoadFromFile(g_gui.copybuffer, FileName(wxString(target.wstring())), options);
    check(reopened && reopened->map.getTileCount() == 402 && reopened->map.houses.getHouse(1), "Retry readback failed");
    report << "SAVE_CANCEL_NEW_RETRY_PASS\n";
    const auto originalMap = readFile(target), originalHouse = readFile(house), originalSpawn = readFile(spawn);
    map.doChange();
    const auto beforeExisting = metadata(map);
    {
        CancelAfterCreation cancel(target);
        check(!EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), true), "Existing-target cancellation unexpectedly succeeded");
        check(cancel.fired && cancel.handled, "Existing-target wx Cancel command was not handled");
    }
    check(metadata(map) == beforeExisting && readFile(target) == originalMap && readFile(house) == originalHouse &&
        readFile(spawn) == originalSpawn && !fs::exists(marker) && !g_gui.IsLoading() && !g_gui.WasLoadCancelled(),
        "Existing-target cancel failed byte/state restoration");
    for (const auto& path : {target, house, spawn}) check(!fs::exists(fs::path(path.string() + "~")), "Cancelled save left backup");
    report << "SAVE_CANCEL_EXISTING_PASS\n";
    check(EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false), "Retry after existing-target cancellation failed");
    reopened = EditorFactory::LoadFromFile(g_gui.copybuffer, FileName(wxString(target.wstring())), options);
    check(reopened && reopened->map.getTileCount() == 402 && !map.hasChanged(), "Final retry/readback failed");
    report << "SAVE_CANCEL_EXISTING_RETRY_PASS\t" << target.string() << '\n';
}
void run() {
    const auto root = fs::current_path();
    std::ofstream report(root / "snapshot-probe.log");
    report << std::unitbuf;
    try {
        check(fs::is_regular_file(root / ".g0-snapshot-fixture") && fs::is_regular_file(root / ".g0-save-cancel") &&
            root.filename() == "run" && wxApp::IsMainLoopRunning(), "Use isolated runner after main-loop start");
        wxString error;
        std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Client load failed");
        runCases(root, report);
        report << "SAVE_CANCEL_PASS\n";
        report.close();
        std::_Exit(0); // Local editors destroyed; global shutdown is outside scope.
    } catch (const std::exception& error) {
        report << "FAILED\t" << error.what() << '\n';
        report.close();
        std::_Exit(9);
    }
}
}
class G0SaveCancelProbeModule : public wxModule {
public:
    bool OnInit() override { wxTheApp->Bind(wxEVT_IDLE, &G0SaveCancelProbeModule::OnIdle, this); return true; }
    void OnExit() override {}
    void OnIdle(wxIdleEvent& event) {
        event.Skip();
        if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
        wxTheApp->Unbind(wxEVT_IDLE, &G0SaveCancelProbeModule::OnIdle, this);
        wxTheApp->CallAfter([] { run(); });
    }
private:
    wxDECLARE_DYNAMIC_CLASS(G0SaveCancelProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G0SaveCancelProbeModule, wxModule);
