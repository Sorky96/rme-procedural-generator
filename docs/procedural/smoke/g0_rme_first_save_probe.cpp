// Test-only first-save integration module; never linked into normal rme.exe.
#include "app/main.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "app/managers/version_manager.h"
#include "editor/editor.h"
#include "editor/editor_factory.h"
#include "editor/persistence/map_load_options.h"
#include "editor/persistence/map_backup_service.h"
#include "editor/persistence/editor_persistence.h"
#include "editor/persistence/recovery_marker.h"
#include "map/map.h"
#include "util/file_system.h"
#include <wx/module.h>
#include <wx/utils.h>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <cstdlib>
#include <algorithm>

namespace {
namespace fs = std::filesystem;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
auto metadata(Map& map) {
    return std::tuple(map.getFilename(), map.getName(), map.getSpawnFilename(), map.getHouseFilename(),
        map.getMonsterFilename(), map.getNpcFilename(), map.getWaypointFilename(), map.getZoneFilename(),
        map.hasChanged(), map.isUnnamed(), map.getTileCount());
}
std::string readFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void firstSave(const fs::path& root, const MapLoadOptions& options, std::ostream& report) {
    auto reference = EditorFactory::LoadFromFile(g_gui.copybuffer,
        FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
    check(reference && reference->map.getTileCount() == 402, "Expected approved synthetic fixture");
    // A real new Editor, not a loaded map with its unnamed flag toggled.
    auto editor = std::make_unique<Editor>(g_gui.copybuffer, reference->map.getVersion());
    auto& map = editor->map;
    check(map.isUnnamed() && map.getTileCount() == 0, "Expected a new empty map");
    map.doChange();
    const auto before = metadata(map);
    const auto local = fs::path(FileSystem::GetLocalDataDirectory().ToStdWstring());
    const auto relative = local.lexically_relative(root);
    check(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..",
        "Recovery directory must be inside private fixture");
    const auto marker = local / ".saving.txt";
    check(!fs::exists(marker), "Private marker already exists");
    std::vector<std::string> markerPaths(7);
    markerPaths.push_back((root / "marker-only-target.otbm").string());
    { std::ofstream output(marker, std::ios::binary); output << "KEEP_MARKER\n"; }
    check(!MapBackupService::WriteCrashMarker(markerPaths) && readFile(marker) == "KEEP_MARKER\n",
        "Crash marker creation overwrote an existing marker");
    const auto blocked = root / "blocked-first.otbm";
    check(!EditorPersistence::saveMap(*editor, FileName(wxString(blocked.wstring())), false) &&
        metadata(map) == before && !fs::exists(blocked) && readFile(marker) == "KEEP_MARKER\n",
        "First save ignored recovery marker or changed map");
    check(MapBackupService::ClearCrashMarker(), "Could not remove own marker");
    { std::ofstream output(marker, std::ios::binary); }
    check(!EditorPersistence::saveMap(*editor, FileName(wxString(blocked.wstring())), false) &&
        metadata(map) == before && !fs::exists(blocked) && fs::file_size(marker) == 0,
        "First save ignored empty marker");
    check(MapBackupService::ClearCrashMarker(), "Could not remove own empty marker");
    report << "FIRST_SAVE_MARKER_GUARD_PASS\n";
    check(MapBackupService::WriteCrashMarker(markerPaths), "Could not create versioned marker");
    std::ifstream written(marker, std::ios::binary);
    const auto parsed = rme::persistence::readRecoveryMarker(written);
    written.close();
    check(parsed && !parsed->legacy && parsed->files == markerPaths,
        "Actual marker writer/reader did not preserve complete paths");
    check(MapBackupService::ClearCrashMarker(), "Could not remove own versioned marker");
    report << "FIRST_SAVE_VERSIONED_MARKER_PASS\n";

    const auto missing = root / "missing-parent" / "first.otbm";
    check(!fs::exists(missing.parent_path()), "Failure requires missing private destination parent");
    // Current production code displays an error even for showdialog=false.
    // Dismiss only this private probe's Error dialog; do not automate other RME windows.
    report << "FIRST_SAVE_FAILURE_BEGIN\t" << marker.string() << '\n';
    check(!EditorPersistence::saveMap(*editor, FileName(wxString(missing.wstring())), false),
        "Save into missing parent unexpectedly succeeded");
    check(metadata(map) == before && !fs::exists(marker) && !fs::exists(missing.parent_path()),
        "Failed first save did not restore map/clear marker/leave destination absent");
    report << "FIRST_SAVE_ROLLBACK_PASS\n";

    const auto directory = root / "first-save-success";
    check(fs::create_directory(directory), "Success requires new private directory");
    const auto target = directory / "first.otbm";
    check(EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false),
        "First save after failure did not succeed");
    check(!map.isUnnamed() && !map.hasChanged() && map.getHouseFilename() == "first-house.xml" &&
        map.getSpawnFilename() == "first-spawn.xml" && !fs::exists(marker),
        "Successful first save left wrong state or marker");
    check(!fs::exists(directory / "first.otbm~") && !fs::exists(directory / "first-house.xml~") &&
        !fs::exists(directory / "first-spawn.xml~"), "First save left backup files");
    auto reopened = EditorFactory::LoadFromFile(g_gui.copybuffer, FileName(wxString(target.wstring())), options);
    check(reopened && reopened->map.getTileCount() == 0 && reopened->map.houses.count() == 0,
        "First-save readback failed");
    report << "FIRST_SAVE_SUCCESS_PASS\t" << target.string() << '\n';
    if (fs::is_regular_file(root / ".g0-rollback-removal")) {
        const auto denied = root / "rollback-denied";
        check(fs::create_directory(denied), "Expected new private ACL directory");
        const auto partial = denied / "partial.otbm";
        check(EditorPersistence::saveMap(*editor, FileName(wxString(partial.wstring())), false),
            "Could not establish private named map");
        for (const auto& name : {"partial.otbm", "partial-house.xml", "partial-spawn.xml"})
            check(fs::remove(denied / name), "Could not remove own initial output");
        const auto house = denied / "partial-house.xml";
        { std::ofstream output(house); output << "PRIVATE_HOUSE_SENTINEL"; }
        map.doChange();
        const auto beforeFailure = metadata(map);
        const auto command = "icacls \"" + wxString(denied.wstring()) + "\"";
        struct PermissionGuard {
            wxString command;
            ~PermissionGuard() { wxExecute(command + " /remove:d *S-1-1-0", wxEXEC_SYNC | wxEXEC_HIDE_CONSOLE); }
        } guard{command};
        check(wxExecute(command + " /deny *S-1-1-0:(OI)(IO)(DE) *S-1-1-0:(DC)",
            wxEXEC_SYNC | wxEXEC_HIDE_CONSOLE) == 0, "Could not apply private delete denial");
        struct HouseLock {
            HANDLE handle;
            ~HouseLock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        } lock{CreateFileW(house.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        check(lock.handle != INVALID_HANDLE_VALUE, "Could not lock own house file against writes");
        // Backup can read the existing house, but serialization/restoration
        // cannot write it. New OTBM/spawn files inherit the delete denial.
        report << "REMOVAL_FAILURE_BEGIN\t" << partial.string() << '\n';
        check(!EditorPersistence::saveMap(*editor, FileName(wxString(partial.wstring())), false),
            "Locked house unexpectedly saved");
        check(metadata(map) == beforeFailure && fs::is_regular_file(partial) &&
            fs::is_regular_file(denied / "partial-spawn.xml") && readFile(house) == "PRIVATE_HOUSE_SENTINEL" &&
            readFile(denied / "partial-house.xml~") == "PRIVATE_HOUSE_SENTINEL",
            "Failed new-target removal did not preserve state and partial files");
        const auto markerBytes = readFile(marker);
        std::ifstream retained(marker, std::ios::binary);
        const auto recovery = rme::persistence::readRecoveryMarker(retained);
        check(recovery && !recovery->legacy && recovery->files.size() == 9,
            "Failed removal lost complete recovery marker");
        check(fs::path(recovery->files[1]) == denied / "partial-house.xml~", "Missing house backup record");
        for (size_t i = 0; i < 7; ++i) if (i != 1)
            check(recovery->files[i].empty(), "Unexpected old-file backup");
        for (const auto& name : {"partial.otbm", "partial-spawn.xml"}) {
            const auto expected = (denied / name).lexically_normal();
            check(std::ranges::any_of(recovery->files, [&expected](const auto& entry) {
                return fs::path(entry).lexically_normal() == expected;
            }), "Marker omitted a new target");
        }
        const auto partialBytes = readFile(partial);
        check(!EditorPersistence::saveMap(*editor, FileName(wxString(partial.wstring())), false) &&
            metadata(map) == beforeFailure && readFile(marker) == markerBytes && readFile(partial) == partialBytes,
            "Retained marker did not block another save");
        report << "REMOVAL_FAILURE_PASS\t" << partial.string() << '\n';
    }
}
void nestedCompanions(const fs::path& root, const MapLoadOptions& options, std::ostream& report) {
    auto reference = EditorFactory::LoadFromFile(g_gui.copybuffer,
        FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
    check(reference != nullptr, "Nested test requires approved fixture version");
    auto editor = std::make_unique<Editor>(g_gui.copybuffer, reference->map.getVersion());
    const auto directory = root / "nested-map";
    check(fs::create_directory(directory), "Nested test must own a new directory");
    const auto target = directory / "nested.otbm";
    const auto marker = fs::path(FileSystem::GetLocalDataDirectory().ToStdWstring()) / ".saving.txt";
    check(EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false), "Initial nested map save failed");
    auto& map = editor->map;
    map.setHouseFilename("nested/houses.xml");
    map.setSpawnFilename("nested/spawns.xml");
    map.doChange();
    check(EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false), "Nested relative save failed");
    const auto house = directory / "nested/houses.xml";
    const auto spawn = directory / "nested/spawns.xml";
    check(fs::is_regular_file(house) && fs::is_regular_file(spawn) && !fs::exists(marker), "Nested writer targeted wrong files");
    auto reopened = EditorFactory::LoadFromFile(g_gui.copybuffer, FileName(wxString(target.wstring())), options);
    check(reopened && reopened->map.getHouseFilename() == map.getHouseFilename() &&
        reopened->map.getSpawnFilename() == map.getSpawnFilename(), "Nested relative readback failed");
    report << "NESTED_RELATIVE_PASS\t" << target.string() << '\n';
    const auto originalMap = readFile(target), originalHouse = readFile(house), originalSpawn = readFile(spawn);
    map.doChange();
    const auto before = metadata(map);
    {
        struct ReadLock {
            HANDLE handle;
            ~ReadLock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        } lock{CreateFileW(house.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        check(lock.handle != INVALID_HANDLE_VALUE, "Could not lock nested house");
        report << "NESTED_ROLLBACK_BEGIN\n";
        check(!EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false), "Locked nested house save succeeded");
        check(metadata(map) == before && readFile(target) == originalMap && readFile(house) == originalHouse &&
            readFile(spawn) == originalSpawn, "Nested rollback changed original bytes or editor state");
        std::ifstream input(marker, std::ios::binary);
        const auto recovery = rme::persistence::readRecoveryMarker(input);
        const std::vector<std::string> expected{target.string() + "~", house.string() + "~", spawn.string() + "~", "", "", "", ""};
        check(recovery && recovery->files.size() == expected.size(), "Nested marker has wrong record count");
        for (size_t i = 0; i < expected.size(); ++i)
            check(fs::path(recovery->files[i]).lexically_normal() == fs::path(expected[i]).lexically_normal(), "Nested marker targeted wrong file");
        check(readFile(fs::path(target.string() + "~")) == originalMap &&
            readFile(fs::path(house.string() + "~")) == originalHouse &&
            readFile(fs::path(spawn.string() + "~")) == originalSpawn, "Nested backup bytes wrong");
    }
    // All retained private backups have just been checked against unchanged
    // originals. Resolve only this test's marker/copies before the next case.
    check(MapBackupService::ClearCrashMarker(), "Could not resolve own checked nested marker");
    for (const auto& path : {target, house, spawn}) check(fs::remove(fs::path(path.string() + "~")), "Could not remove own checked backup");
    report << "NESTED_ROLLBACK_PASS\n";
    map.setHouseFilename(house.string());
    check(EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false), "Absolute inside-directory save failed");
    reopened = EditorFactory::LoadFromFile(g_gui.copybuffer, FileName(wxString(target.wstring())), options);
    check(reopened && reopened->map.getHouseFilename() == "nested/houses.xml" && !fs::exists(marker), "Absolute readback failed");
    report << "NESTED_ABSOLUTE_PASS\n";
    const auto outside = root / "outside-house.xml";
    { std::ofstream output(outside); output << "PRIVATE_OUTSIDE_SENTINEL"; }
    map.setHouseFilename("../outside-house.xml");
    check(EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false), "Constrained companion save failed");
    check(readFile(outside) == "PRIVATE_OUTSIDE_SENTINEL" && fs::is_regular_file(directory / "outside-house.xml") &&
        !fs::exists(marker) && !fs::exists(directory / "outside-house.xml~"), "Traversal constraint or cleanup failed");
    reopened = EditorFactory::LoadFromFile(g_gui.copybuffer, FileName(wxString(target.wstring())), options);
    check(reopened && reopened->map.getTileCount() == 0, "Constrained companion readback failed");
    // Export a final fixture with an ordinary in-directory header for the
    // independent reader, which deliberately does not emulate GUI path policy.
    map.setHouseFilename("outside-house.xml");
    check(EditorPersistence::saveMap(*editor, FileName(wxString(target.wstring())), false), "Final in-directory header save failed");
    report << "NESTED_CONSTRAINED_PASS\t" << target.string() << '\n';
}
void startupMarkerCase(const fs::path& root, std::ostream& report) {
    const auto mode = readFile(root / ".g0-marker-startup");
    const auto marker = root / "data/user/data/.saving.txt";
    const auto expectedMarker = readFile(root / "expected-marker.txt");
    const bool restored = mode != "invalid";
    const std::pair<const char*, const char*> names[] = {
        {"recovery-target.otbm", "house-roundtrip.otbm"},
        {"recovery-target-house.xml", "house-roundtrip-house.xml"},
        {"recovery-target-spawn.xml", "house-roundtrip-spawn.xml"}
    };
    for (const auto& [target, source] : names) {
        const auto original = readFile(root / source);
        check(readFile(root / target) == (restored ? original : "PARTIAL_TARGET"),
            "Startup recovery changed the wrong target bytes");
        const auto backup = root / (std::string(target) + "~");
        if (mode == "versioned") {
            check(!fs::exists(backup), "Versioned recovery did not clean completed backup");
        } else {
            check(fs::is_regular_file(backup) && readFile(backup) == original,
                "Invalid/legacy recovery changed a retained backup");
        }
    }
    if (mode == "versioned") {
        check(!fs::exists(marker), "Versioned recovery left completed marker");
    } else {
        check(fs::is_regular_file(marker) && readFile(marker) == expectedMarker,
            "Invalid/legacy marker was not retained byte-for-byte");
    }
    if (restored) {
        const auto editor = g_gui.GetCurrentEditor();
        check(editor && editor->map.getTileCount() == 402 && editor->map.houses.getHouse(1),
            "Startup did not open restored fixture with its correct profile");
    }
    report << "MARKER_STARTUP_PASS\t" << mode << '\n';
}
void run() {
    const auto root = fs::current_path();
    std::ofstream report(root / "snapshot-probe.log");
    report << std::unitbuf;
    try {
        check(fs::is_regular_file(root / ".g0-snapshot-fixture") && root.filename() == "run",
            "Use the isolated runner directory");
        check(wxApp::IsMainLoopRunning(), "Probe must run after OnInit finishes");
        if (fs::is_regular_file(root / ".g0-marker-startup")) {
            startupMarkerCase(root, report);
            report.close();
            std::_Exit(0); // Startup's GUI editor/global cleanup is outside this probe.
        }
        wxString error;
        std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Client load failed");
        report << "VERSION loaded; material warnings=" << warnings.size() << '\n';
        MapLoadOptions options;
        options.selected_client_id = "1098-BlackTek";
        firstSave(root, options, report);
        if (fs::is_regular_file(root / ".g0-nested-companions")) nestedCompanions(root, options, report);
        report << "FIRST_SAVE_PASS\n";
        report.close();
        std::_Exit(0); // All local editors destroyed; global app shutdown omitted.
    } catch (const std::exception& error) {
        report << "FAILED\t" << error.what() << '\n';
        report.close();
        std::_Exit(9);
    }
}
}
class G0FirstSaveProbeModule : public wxModule {
public:
    bool OnInit() override {
        wxTheApp->Bind(wxEVT_IDLE, &G0FirstSaveProbeModule::OnIdle, this);
        return true;
    }
    void OnExit() override {}
    void OnIdle(wxIdleEvent& event) {
        event.Skip();
        if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
        wxTheApp->Unbind(wxEVT_IDLE, &G0FirstSaveProbeModule::OnIdle, this);
        wxTheApp->CallAfter([] { run(); });
    }
private:
    wxDECLARE_DYNAMIC_CLASS(G0FirstSaveProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G0FirstSaveProbeModule, wxModule);
