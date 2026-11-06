// Test-only module for actual house property actions; never linked into rme.exe.
#include "app/main.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "app/managers/version_manager.h"
#include "app/settings.h"
#include "editor/editor.h"
#include "editor/editor_factory.h"
#include "editor/action_queue.h"
#include "editor/persistence/map_load_options.h"
#include "editor/persistence/map_backup_service.h"
#include "game/house.h"
#include "map/map.h"
#include <wx/module.h>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <cstdlib>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
auto properties(const House& house) {
    return std::tuple(house.getID(), house.getGeneration(), house.name, house.rent,
        house.townid, house.guildhall, house.getExit(), house.size());
}
void editHouse(Editor& editor, uint32_t currentId, uint32_t targetId, const std::string& name, int rent) {
    const auto house = editor.map.houses.getHouse(currentId);
    check(house != nullptr, "House to edit missing");
    auto action = editor.actionQueue->createAction(ACTION_HOUSE_PROPERTIES);
    action->addChange(std::make_unique<Change>(HousePropertiesChangeData {
        currentId, house->getGeneration(), targetId, name, rent, house->townid, !house->guildhall }));
    editor.addAction(std::move(action), 0);
}
House* addConflict(Map& map, uint32_t id) {
    auto conflict = std::make_unique<House>(map);
    conflict->setID(id);
    conflict->name = "G0 Replacement";
    conflict->rent = 7777;
    auto pointer = conflict.get();
    check(map.houses.addHouse(std::move(conflict)), "Could not create private conflict");
    return pointer;
}
bool exitContains(Map& map, Position pos, uint32_t id);
void houseHistory(const std::filesystem::path& root, const MapLoadOptions& options, std::ostream& report) {
    auto editor = EditorFactory::LoadFromFile(g_gui.copybuffer,
        FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
    check(editor && editor->map.getTileCount() == 402, "Expected complete fixture");
    auto& map = editor->map;
    auto& queue = *editor->actionQueue;
    auto house = map.houses.getHouse(1);
    check(house && house->rent == 4321 && house->size() == 1, "House fixture missing");
    const auto original = properties(*house);
    const auto tile = map.getTile(104,102,7);
    check(tile && tile->getHouseID() == 1, "House tile missing");
    check(queue.getCurrentIndex() == 0 && queue.getSize() == 0, "Expected empty history");
    editHouse(*editor, 1, 2, "G0 Changed", 6789);
    check(map.hasChanged() && map.houses.getHouse(2) == house && !map.houses.getHouse(1) &&
        tile->getHouseID() == 2 && house->size() == 1 && house->getExit() == std::get<6>(original),
        "House ID edit did not preserve relationships/dirty");
    const auto changed = properties(*house);
    check(exitContains(map, house->getExit(), 2) && !exitContains(map, house->getExit(), 1),
        "Property ID change left a stale exit index");
    check(queue.getCurrentIndex() == 1 && queue.getSize() == 1, "House edit history size wrong");
    auto conflict = addConflict(map, 1);
    const auto conflictState = properties(*conflict);
    check(!queue.undo() && queue.getCurrentIndex() == 1 && queue.getSize() == 1 &&
        properties(*house) == changed && properties(*conflict) == conflictState && tile->getHouseID() == 2,
        "Conflicting Undo changed map/history");
    map.houses.removeHouse(conflict);
    map.clearChanges();
    check(queue.undo() && queue.getCurrentIndex() == 0 && properties(*house) == original &&
        tile->getHouseID() == 1 && map.hasChanged() && exitContains(map, house->getExit(), 1),
        "Undo retry after conflict removal failed");
    report << "HOUSE_UNDO_RETRY_PASS\n";
    conflict = addConflict(map, 2);
    const auto redoConflict = properties(*conflict);
    check(!queue.redo() && queue.getCurrentIndex() == 0 && properties(*house) == original &&
        properties(*conflict) == redoConflict && tile->getHouseID() == 1,
        "Conflicting Redo changed map/history");
    map.houses.removeHouse(conflict);
    check(queue.redo() && queue.getCurrentIndex() == 1 && properties(*house) == changed &&
        tile->getHouseID() == 2, "Redo retry after conflict removal failed");
    report << "HOUSE_REDO_RETRY_PASS\n";

    // Deliberately bypass history in this disposable editor to test replacement
    // identity. houseLifecycle below exercises the normal presence actions.
    const auto generation = house->getGeneration();
    map.houses.removeHouse(house);
    check(!queue.undo() && queue.getCurrentIndex() == 1 && map.houses.count() == 0 &&
        tile->getHouseID() == 0, "Undo of deleted house was not safely rejected");
    auto replacement = addConflict(map, 2);
    const auto replacementState = properties(*replacement);
    check(replacement->getGeneration() != generation, "House generations unexpectedly reused");
    check(!queue.undo() && queue.getCurrentIndex() == 1 && queue.getSize() == 1 &&
        properties(*replacement) == replacementState && tile->getHouseID() == 0,
        "Old action changed replacement house");
    report << "HOUSE_REPLACEMENT_REJECTED_PASS\n";
}
void removeHouse(Editor& editor, House& house) {
    auto action = editor.actionQueue->createAction(ACTION_HOUSE_PRESENCE);
    action->addChange(std::make_unique<Change>(HousePresenceChangeData {
        house.getID(), house.getGeneration(), nullptr }));
    editor.addAction(std::move(action), 0);
}
bool exitContains(Map& map, Position pos, uint32_t id) {
    const auto tile = map.getTile(pos);
    const auto exits = tile ? tile->getHouseExits() : nullptr;
    return exits && std::ranges::count(*exits, id) == 1;
}
void houseLifecycle(const std::filesystem::path& root, const MapLoadOptions& options, std::ostream& report) {
    auto editor = EditorFactory::LoadFromFile(g_gui.copybuffer,
        FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
    auto& map = editor->map;
    auto& queue = *editor->actionQueue;
    auto created = std::make_unique<House>(map);
    created->setID(2);
    created->name = "G0 Created";
    auto pointer = created.get();
    const auto createdProperties = properties(*pointer);
    auto action = queue.createAction(ACTION_HOUSE_PRESENCE);
    action->addChange(std::make_unique<Change>(HousePresenceChangeData {
        2, pointer->getGeneration(), std::move(created) }));
    editor->addAction(std::move(action), 0);
    check(map.houses.getHouse(2) == pointer && queue.getCurrentIndex() == 1 && map.hasChanged(),
        "Add House action did not change map/history");
    map.clearChanges();
    check(queue.undo() && !map.houses.getHouse(2) && map.houses.count() == 1 && map.hasChanged(),
        "Add House Undo failed");
    check(queue.redo() && map.houses.getHouse(2) == pointer && properties(*pointer) == createdProperties,
        "Add House Redo did not restore original identity");
    editHouse(*editor, 2, 3, "G0 Created Edited", 1234);
    const auto edited = properties(*pointer);
    removeHouse(*editor, *pointer);
    check(map.houses.count() == 1 && !map.houses.getHouse(3) && queue.getCurrentIndex() == 3 && queue.getSize() == 3,
        "Add/edit/remove actions were not separate");
    check(queue.undo() && map.houses.getHouse(3) == pointer && properties(*pointer) == edited,
        "Delete Undo lost created house identity/properties");
    check(queue.undo() && map.houses.getHouse(2) == pointer && properties(*pointer) == createdProperties,
        "Earlier property action failed after Delete Undo");
    check(queue.undo() && map.houses.count() == 1, "Earlier Add Undo failed after Delete Undo");
    check(queue.redo() && queue.redo() && queue.redo() && map.houses.count() == 1 && queue.getCurrentIndex() == 3,
        "Add/edit/remove Redo chain failed");
    report << "HOUSE_ADD_REMOVE_CHAIN_PASS\n";

    queue.clear();
    auto existing = map.houses.getHouse(1);
    const auto original = properties(*existing);
    const auto originalTile = map.getTile(104,102,7);
    const auto originalFlags = originalTile->getMapFlags();
    const auto exit = existing->getExit();
    check(exitContains(map, exit, 1), "Original exit index missing");
    removeHouse(*editor, *existing);
    check(map.houses.count() == 0 && originalTile->getHouseID() == 0 && !exitContains(map, exit, 1) &&
        originalTile->getMapFlags() == originalFlags, "Delete did not detach only house metadata");
    auto conflict = addConflict(map, 1);
    const auto conflictState = properties(*conflict);
    check(!queue.undo() && queue.getCurrentIndex() == 1 && properties(*conflict) == conflictState &&
        originalTile->getHouseID() == 0, "Delete Undo overwrote conflicting house");
    map.houses.removeHouse(conflict);
    check(queue.undo() && map.houses.getHouse(1) == existing && properties(*existing) == original &&
        originalTile->getHouseID() == 1 && exitContains(map, exit, 1), "Delete Undo did not restore metadata/index");
    check(queue.redo() && queue.undo() && exitContains(map, exit, 1), "Delete repeat duplicated/missed exit index");
    report << "HOUSE_DELETE_RESTORE_PASS\n";
    std::string snapshot, failure;
    check(MapBackupService::CreateSnapshot(*editor, snapshot, failure, "g0_house_restored"),
        "Restored house snapshot failed");
    auto reopened = EditorFactory::LoadFromFile(g_gui.copybuffer, FileName(wxstr(snapshot)), options);
    check(reopened && reopened->map.houses.count() == 1 && reopened->map.houses.getHouse(1)->rent == 4321 &&
        reopened->map.getTile(104,102,7)->getHouseID() == 1 && exitContains(reopened->map, exit, 1),
        "Restored house snapshot readback failed");
    report << "HOUSE_ROUNDTRIP\t" << snapshot << '\n';
    // Clearing history while a removed house is owned by it must be safe too.
    check(queue.redo() && map.houses.count() == 0, "Final Delete Redo failed");
    queue.clear();
    check(queue.getSize() == 0 && map.houses.count() == 0, "Removed-house history cleanup failed");
    report << "HOUSE_HISTORY_CLEANUP_PASS\n";
}
void houseLiveGuard(const std::filesystem::path& root, const MapLoadOptions& options, std::ostream& report) {
    auto editor = EditorFactory::LoadFromFile(g_gui.copybuffer,
        FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
    auto& map = editor->map;
    const auto house = map.houses.getHouse(1);
    const auto original = properties(*house);
    // StartServer constructs the real LiveServer and networked queue. No bind(),
    // connection thread, listener or peer is started by this test.
    check(editor->live_manager.StartServer() != nullptr && editor->live_manager.IsLive(),
        "Live manager did not enter server state");
    map.clearChanges();
    editHouse(*editor, 1, 2, "G0 Forbidden Live Edit", 9999);
    removeHouse(*editor, *house);
    auto created = std::make_unique<House>(map);
    created->setID(2);
    auto action = editor->actionQueue->createAction(ACTION_HOUSE_PRESENCE);
    action->addChange(std::make_unique<Change>(HousePresenceChangeData {
        2, created->getGeneration(), std::move(created) }));
    editor->addAction(std::move(action));
    check(map.houses.count() == 1 && map.houses.getHouse(1) == house && !map.houses.getHouse(2) &&
        properties(*house) == original && !map.hasChanged() &&
        editor->actionQueue->getCurrentIndex() == 0 && editor->actionQueue->getSize() == 0 &&
        map.getTile(104,102,7)->getHouseID() == 1 && exitContains(map, house->getExit(), 1),
        "Live guard allowed house metadata/history mutation");
    editor->live_manager.CloseServer();
    check(!editor->live_manager.IsLive(), "Live manager did not return to local state");
    editHouse(*editor, 1, 1, "G0 Allowed Local Edit", 9876);
    check(house->rent == 9876 && editor->actionQueue->undo() && properties(*house) == original,
        "Local house editing did not resume after live state ended");
    report << "HOUSE_LIVE_GUARD_PASS\n";
}
void run() {
    const auto root = std::filesystem::current_path();
    std::ofstream report(root / "snapshot-probe.log");
    report << std::unitbuf;
    try {
        check(std::filesystem::is_regular_file(root / ".g0-snapshot-fixture") && root.filename() == "run",
            "Use the isolated runner directory");
        check(wxApp::IsMainLoopRunning(), "Probe must run after OnInit finishes");
        wxString error;
        std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Client load failed");
        report << "VERSION loaded; material warnings=" << warnings.size() << '\n';
        MapLoadOptions options;
        options.selected_client_id = "1098-BlackTek";
        houseHistory(root, options, report);
        houseLifecycle(root, options, report);
        houseLiveGuard(root, options, report);
        report << "HOUSE_HISTORY_PASS\n";
        report.close();
        std::_Exit(0); // Local editors destroyed; unrelated app/global shutdown omitted.
    } catch (const std::exception& error) {
        report << "FAILED\t" << error.what() << '\n';
        report.close();
        std::_Exit(9);
    }
}
}
class G0HouseHistoryProbeModule : public wxModule {
public:
    bool OnInit() override {
        wxTheApp->Bind(wxEVT_IDLE, &G0HouseHistoryProbeModule::OnIdle, this);
        return true;
    }
    void OnExit() override {}
    void OnIdle(wxIdleEvent& event) {
        event.Skip();
        if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
        wxTheApp->Unbind(wxEVT_IDLE, &G0HouseHistoryProbeModule::OnIdle, this);
        wxTheApp->CallAfter([] { run(); });
    }
private:
    wxDECLARE_DYNAMIC_CLASS(G0HouseHistoryProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G0HouseHistoryProbeModule, wxModule);
