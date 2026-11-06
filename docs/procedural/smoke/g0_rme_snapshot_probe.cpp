// Test-only module linked into a separate executable, never the normal rme.exe.
#include "app/main.h"
#include "ui/gui.h"
#include "app/managers/version_manager.h"
#include "editor/editor.h"
#include "editor/editor_factory.h"
#include "editor/persistence/map_load_options.h"
#include "editor/persistence/map_backup_service.h"
#include "editor/persistence/editor_persistence.h"
#include "app/settings.h"
#include "game/waypoints.h"
#include "game/item.h"
#include "map/map.h"
#include <wx/module.h>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <thread>
#include <tuple>
#include <cstdlib>

namespace {
namespace fs = std::filesystem;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
fs::path pathOf(const std::string& value) { return fs::path(wxstr(value).ToStdWstring()); }
auto metadata(Map& map) {
    return std::tuple(map.getFilename(), map.getName(), map.getSpawnFilename(), map.getHouseFilename(),
        map.getMonsterFilename(), map.getNpcFilename(), map.getWaypointFilename(), map.getZoneFilename(),
        map.hasChanged(), map.isUnnamed(), map.getTileCount());
}
std::string readFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void saveAsBothCases(const fs::path& root, const MapLoadOptions& options, std::ostream& report) {
    const auto previousFormat = g_settings.getInteger(Config::WAYPOINT_STORAGE_FORMAT);
    g_settings.setInteger(Config::WAYPOINT_STORAGE_FORMAT, static_cast<int>(Config::WaypointStorageFormat::BOTH));
    for (const bool absolute : {false, true}) {
        const auto directory = root / (absolute ? "saveas-absolute" : "saveas-relative");
        check(fs::create_directory(directory), "Save As case requires a new private directory");
        auto editor = EditorFactory::LoadFromFile(g_gui.copybuffer,
            FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
        if (absolute) {
            editor->map.setHouseFilename(nstr(wxString((root / "house-roundtrip-house.xml").wstring())));
            editor->map.setSpawnFilename(nstr(wxString((root / "house-roundtrip-spawn.xml").wstring())));
        }
        editor->map.waypoints.addWaypoint(std::make_unique<Waypoint>("G0 Both", Position(160,160,7)));
        auto ground = editor->map.getTile(160,160,7)->ground.get();
        check(ground != nullptr, "Metadata ground missing");
        ground->setText("G0 Save As text");
        ground->setDescription("G0 Save As description");
        editor->map.doChange();
        { std::ofstream sentinel(directory / "copy-house.xml"); sentinel << "KEEP_HOUSE"; }
        { std::ofstream sentinel(directory / "copy-spawn.xml"); sentinel << "KEEP_SPAWN"; }
        const auto target = directory / "copy.otbm";
        report << "SAVE_AS_START\t" << (absolute ? "absolute" : "relative") << '\n';
        check(EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false), "Save As failed");
        check(!editor->map.hasChanged(), "Save As did not clear dirty state");
        check(editor->map.getHouseFilename() == "copy-1-house.xml" &&
            editor->map.getSpawnFilename() == "copy-1-spawn.xml", "Save As did not avoid companion collisions");
        check(readFile(directory / "copy-house.xml") == "KEEP_HOUSE" &&
            readFile(directory / "copy-spawn.xml") == "KEEP_SPAWN", "Save As overwrote sentinels");
        check(fs::is_regular_file(directory / "copy-waypoint.xml"), "BOTH waypoint XML missing");
        auto reopened = EditorFactory::LoadFromFile(g_gui.copybuffer, FileName(wxString(target.wstring())), options);
        check(reopened->map.getTileCount() == 402, "Save As readback tile count changed");
        const auto waypoint = reopened->map.waypoints.getWaypoint("G0 Both");
        check(waypoint && waypoint->pos == Position(160,160,7), "BOTH waypoint readback failed");
        auto restored = reopened->map.getTile(160,160,7)->ground.get();
        check(restored && restored->getText() == "G0 Save As text" &&
            restored->getDescription() == "G0 Save As description" &&
            restored->getActionID() == 12007 && restored->getUniqueID() == 12107,
            "Changed attributes were not preserved by Save As");
        editor->map.doChange();
        const auto beforeBlocked = metadata(editor->map);
        { std::ofstream sentinel(directory / "blocked-waypoint.xml"); sentinel << "KEEP_WAYPOINT"; }
        check(!EditorPersistence::saveMap(*editor, FileName(wxString((directory / "blocked.otbm").wstring())), false),
            "BOTH waypoint collision accepted");
        check(metadata(editor->map) == beforeBlocked && !fs::exists(directory / "blocked.otbm") &&
            readFile(directory / "blocked-waypoint.xml") == "KEEP_WAYPOINT", "Blocked Save As changed state/files");
        report << "SAVE_AS_BOTH_PASS\t" << (absolute ? "absolute" : "relative") << '\t' << nstr(wxString(target.wstring())) << '\n';
    }
    g_settings.setInteger(Config::WAYPOINT_STORAGE_FORMAT, previousFormat);
}
void runSnapshotProbe() {
    const auto root = fs::current_path();
    std::ofstream report(root / "snapshot-probe.log");
    report << std::unitbuf;
    try {
        check(fs::is_regular_file(root / ".g0-snapshot-fixture"), "Missing isolated fixture guard");
        check(root.filename() == "run", "Use the isolated run directory");
        wxString error;
        std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Client load failed");
        report << "VERSION loaded; material warnings=" << warnings.size() << '\n';
        MapLoadOptions options;
        options.selected_client_id = "1098-BlackTek";
        auto editor = EditorFactory::LoadFromFile(g_gui.copybuffer,
            FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
        check(editor && editor->map.getTileCount() == 402, "Expected complete 402-tile fixture");
        auto original = metadata(editor->map);
        report << "FIXTURE loaded\n";
        std::string firstPath;
        for (int i = 0; i < 4; ++i) {
            if (i == 3) editor->map.doChange();
            const auto expected = metadata(editor->map);
            std::string output, failure;
            check(MapBackupService::CreateSnapshot(*editor, output, failure, "g0_success"), "Snapshot failed");
            check(metadata(editor->map) == expected, "Successful snapshot changed map metadata/state");
            check(fs::is_regular_file(pathOf(output)), "Snapshot OTBM missing");
            check(output != firstPath, "Repeated snapshot reused path");
            if (i == 0) firstPath = output;
            auto reopened = EditorFactory::LoadFromFile(g_gui.copybuffer, FileName(wxstr(output)), options);
            check(reopened && reopened->map.getTileCount() == 402, "Snapshot readback failed");
            report << "SUCCESS\t" << output << '\n';
        }
        editor->map.clearChanges();
        check(metadata(editor->map) == original, "Successful dirty-state probe did not restore initial state");
        std::string output, failure;
        check(!MapBackupService::CreateSnapshot(*editor, output, failure, "g0:invalid"), "Invalid directory accepted");
        check(!failure.empty() && metadata(editor->map) == original, "Reservation failure lost state/error");
        report << "RESERVATION_FAILURE\t" << failure << '\n';
        editor->map.doChange();
        const auto dirtyBeforeFailure = metadata(editor->map);
        bool writeFailureObserved = false;
        for (int attempt = 0; attempt < 20 && !writeFailureObserved; ++attempt) {
            const std::string tag = "g0_failure" + std::to_string(attempt);
            std::atomic_bool ready = false;
            std::atomic_bool locked = false;
            std::jthread locker([&](std::stop_token stop) {
                HANDLE held = INVALID_HANDLE_VALUE;
                ready = true;
                while (!stop.stop_requested()) {
                    if (held == INVALID_HANDLE_VALUE) {
                        for (const auto& entry : fs::directory_iterator(root)) {
                            const auto directory = entry.path().filename().string();
                            if (!entry.is_directory() || directory.find("_" + tag + "_") == std::string::npos) continue;
                            const auto suffix = directory.rfind("_snapshot");
                            if (suffix == std::string::npos) continue;
                            const auto house = entry.path() / (directory.substr(0, suffix) + "-house.xml");
                            held = CreateFileW(house.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                            if (held != INVALID_HANDLE_VALUE) { locked = true; break; }
                        }
                    }
                    std::this_thread::yield();
                }
                if (held != INVALID_HANDLE_VALUE) CloseHandle(held);
            });
            while (!ready) std::this_thread::yield();
            output.clear(); failure.clear();
            const bool saved = MapBackupService::CreateSnapshot(*editor, output, failure, tag);
            locker.request_stop(); locker.join();
            check(metadata(editor->map) == dirtyBeforeFailure, "Write attempt changed map metadata/state");
            report << "WRITE_ATTEMPT\t" << attempt << '\t' << saved << '\t' << locked << '\t' << output << '\t' << failure << '\n';
            if (!saved && locked) {
                check(!failure.empty() && fs::is_regular_file(pathOf(output)), "Failed snapshot did not retain partial OTBM/error");
                writeFailureObserved = true;
            }
        }
        check(writeFailureObserved, "No actual locked-file write failure observed; test incomplete");
        editor->map.clearChanges();
        check(metadata(editor->map) == original, "Write failure dirty-state probe did not restore initial state");
        editor.reset();
        report << "SNAPSHOT_RUNTIME_PASS\n";
        saveAsBothCases(root, options, report);
        report << "SAVE_AS_RUNTIME_PASS\n";
        report.close();
        std::_Exit(0); // Test ends after local editors, before unrelated app/global shutdown.
    } catch (const std::exception& error) {
        report << "FAILED\t" << error.what() << '\n';
        report.close();
        std::_Exit(9);
    }
}
}

class G0SnapshotProbeModule : public wxModule {
public:
    bool OnInit() override {
        wxTheApp->Bind(wxEVT_IDLE, &G0SnapshotProbeModule::OnIdle, this);
        return true;
    }
    void OnExit() override {}
    void OnIdle(wxIdleEvent& event) {
        event.Skip();
        if (!g_gui.root) return;
        wxTheApp->Unbind(wxEVT_IDLE, &G0SnapshotProbeModule::OnIdle, this);
        wxTheApp->CallAfter([] { runSnapshotProbe(); });
    }
private:
    wxDECLARE_DYNAMIC_CLASS(G0SnapshotProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G0SnapshotProbeModule, wxModule);
