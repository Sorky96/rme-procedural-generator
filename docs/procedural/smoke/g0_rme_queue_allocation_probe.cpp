// Test-only allocator and virtual factory subclasses; never linked into normal RME.
#include "app/main.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "ui/map_tab.h"
#include "app/managers/version_manager.h"
#include "app/settings.h"
#include "editor/editor.h"
#include "editor/editor_factory.h"
#include "editor/action.h"
#include "editor/action_queue.h"
#include "editor/persistence/map_load_options.h"
#include "editor/persistence/map_backup_service.h"
#include "map/map.h"
#include "game/item.h"
#include "lua/lua_script_manager.h"
#include <wx/module.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <new>
#include <vector>

namespace {
thread_local bool recording = false, faultObserved = false;
thread_local int allocationCount = 0, failOrdinal = 0;
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
struct PublicationGate { bool requested = false; int ordinal = 0; };
class ProbeBatch final : public BatchAction {
public:
    ProbeBatch(Editor& editor, ActionIdentifier type, PublicationGate& gate) : BatchAction(editor, type), gate(gate) {}
    void forceMergeWindow() { timestamp = static_cast<int>(time(nullptr)) + 3600; }
protected:
    void commit() override {
        BatchAction::commit();
        if (gate.requested) {
            gate.requested = false;
            allocationCount = 0;
            faultObserved = false;
            failOrdinal = gate.ordinal;
            recording = true;
        }
    }
private:
    PublicationGate& gate;
};
struct QueueState {
    size_t index, memory;
    std::vector<const BatchAction*> identities;
    std::vector<std::string> labels;
    bool operator==(const QueueState&) const = default;
};
class ProbeQueue final : public ActionQueue {
public:
    explicit ProbeQueue(Editor& editor) : ActionQueue(editor) {}
    std::unique_ptr<BatchAction> createBatch(ActionIdentifier type) override {
        return std::make_unique<ProbeBatch>(editor, type, gate);
    }
    QueueState state() const {
        QueueState result {current, memory_size, {}, {}};
        for (const auto& action : actions) {
            result.identities.push_back(action.get());
            result.labels.push_back(action->getLabel());
        }
        return result;
    }
    void forceMergeWindow() {
        check(current > 0, "Missing merge predecessor");
        auto* predecessor = dynamic_cast<ProbeBatch*>(actions[current - 1].get());
        check(predecessor != nullptr, "Wrong batch factory");
        predecessor->forceMergeWindow();
    }
    PublicationGate gate;
};
struct SettingsGuard {
    const int count = g_settings.getInteger(Config::UNDO_SIZE);
    const int memory = g_settings.getInteger(Config::UNDO_MEM_SIZE);
    const int group = g_settings.getInteger(Config::GROUP_ACTIONS);
    void spacious() const {
        g_settings.setInteger(Config::UNDO_SIZE, 1000);
        g_settings.setInteger(Config::UNDO_MEM_SIZE, 256);
        g_settings.setInteger(Config::GROUP_ACTIONS, 0);
    }
    ~SettingsGuard() {
        recording = false;
        g_settings.setInteger(Config::UNDO_SIZE, count);
        g_settings.setInteger(Config::UNDO_MEM_SIZE, memory);
        g_settings.setInteger(Config::GROUP_ACTIONS, group);
    }
};
void execute(LuaEngine& engine, const std::string& code) {
    if (!engine.executeString(code, "G0 history allocation")) throw std::runtime_error(engine.getLastError());
}
uint16_t aid(const Editor& editor) {
    const auto tile = editor.map.getTile(110,110,7);
    check(tile && tile->ground, "History test lost its ground");
    return tile->ground->getActionID();
}
int oneCase(const fs::path& root, const std::string& scenario, int ordinal, std::ostream& report) {
    SettingsGuard settings;
    settings.spacious();
    MapLoadOptions options;
    options.selected_client_id = "1098-BlackTek";
    auto owned = EditorFactory::LoadFromFile(g_gui.copybuffer,
        FileName(wxString((root / "house-roundtrip.otbm").wstring())), options);
    check(owned && owned->map.getTileCount() == 402, "Approved fixture not loaded");
    auto* editor = owned.get();
    const auto originalAid = aid(*editor);
    auto originalTile = editor->map.getTile(110,110,7)->deepCopy();
    auto replacement = std::make_unique<ProbeQueue>(*editor);
    auto* queue = replacement.get();
    editor->actionQueue = std::move(replacement);
    g_gui.FinishWelcomeDialog();
    new MapTab(g_gui.tabbook, owned.release());
    check(g_gui.GetCurrentEditor() == editor, "Wrong current test editor");
    auto& engine = g_luaScripts.getEngine();
    const int prefix = scenario == "merge-redo" ? 2 : 14;
    for (int i = 1; i <= prefix; ++i) {
        execute(engine, "app.transaction('P" + std::to_string(i) + "', function() "
            "app.map:getTile(110,110,7).ground.actionId = " + std::to_string(1000 + i) + " end)");
    }
    check(queue->getCurrentIndex() == prefix && queue->getSize() == prefix, "Prefix actions were merged/trimmed");
    if (scenario == "merge-redo") {
        check(queue->undo() && aid(*editor) == 1001, "Could not prepare actual Redo branch");
        g_settings.setInteger(Config::GROUP_ACTIONS, 1);
        // Existing merge condition also covers wall-clock rollback. Force its
        // timestamp window without sleeping or changing the production API.
        queue->forceMergeWindow();
    } else if (scenario == "count-limit") {
        g_settings.setInteger(Config::UNDO_SIZE, 1);
    } else if (scenario == "memory-limit") {
        g_settings.setInteger(Config::UNDO_MEM_SIZE, 0);
    }
    const auto before = queue->state();
    const auto baselineAid = aid(*editor);
    const bool baselineDirty = editor->map.hasChanged();
    queue->gate.ordinal = ordinal;
    queue->gate.requested = true;
    execute(engine, "g0_queue_ok, g0_queue_error = pcall(function() "
        "app.transaction('target', function() app.map:getTile(110,110,7).ground.actionId = 50000 end) end)");
    recording = false;
    const int allocations = allocationCount;
    check(!queue->gate.requested, "Publication gate was not reached");
    const bool success = engine.getState()["g0_queue_ok"].get<bool>();
    const auto after = queue->state();
    report << "QUEUE_ALLOCATION_CASE\t" << scenario << '\t' << ordinal << "\tallocations=" << allocations
        << "\tfault=" << faultObserved << "\tsuccess=" << success << "\thistory=" << queue->getCurrentIndex()
        << '/' << queue->getSize() << "\tmemory=" << after.memory << '\n';
    if (!success) {
        report << "QUEUE_ALLOCATION_ERROR\t" << engine.getState()["g0_queue_error"].get<std::string>() << '\n';
        check(faultObserved && aid(*editor) == baselineAid && editor->map.hasChanged() == baselineDirty,
            "Failed publication did not restore map/dirty state");
        check(after == before, "Failed publication changed history identities, labels, index or memory accounting");
        while (queue->canRedo()) {
            const auto expected = 1001 + queue->getCurrentIndex();
            check(queue->redo() && aid(*editor) == expected, "Original Redo branch was not usable after failure");
        }
    } else {
        check(aid(*editor) == 50000, "Successful publication lost its target");
        const size_t expected = scenario == "merge-redo" || scenario == "memory-limit" ? 1 :
            scenario == "count-limit" ? prefix : prefix + 1;
        check(queue->getCurrentIndex() == expected && queue->getSize() == expected,
            "Successful append/merge/prune did not match the existing history policy");
        const auto undoAid = scenario == "merge-redo" ? originalAid : baselineAid;
        check(queue->undo() && aid(*editor) == undoAid, "Published target Undo failed");
        check(queue->redo() && aid(*editor) == 50000, "Published target Redo failed");
        check(queue->undo() && aid(*editor) == undoAid, "Repeated target Undo failed");
    }
    while (queue->canUndo()) {
        const auto label = queue->getActionName(queue->getCurrentIndex() - 1);
        check(label.starts_with("P"), "Unexpected retained history label");
        const auto number = std::stoi(label.substr(1));
        const auto expected = number == 1 ? originalAid : 999 + number;
        check(queue->undo() && aid(*editor) == expected, "Retained prefix Undo was corrupted");
    }
    settings.spacious();
    if (aid(*editor) != originalAid) {
        // A numeric zero setter is not the same state as an absent attribute.
        // Restore the complete original tile through a normal undoable action.
        auto restore = queue->createAction(ACTION_CHANGE_PROPERTIES);
        restore->addChange(std::make_unique<Change>(std::move(originalTile), Position(110,110,7)));
        editor->addAction(std::move(restore), 0);
    }
    check(aid(*editor) == originalAid, "Could not restore disposable map after intended eviction");
    std::string snapshot, error;
    check(MapBackupService::CreateSnapshot(*editor, snapshot, error, "queue_" + scenario + '_' + std::to_string(ordinal)),
        "History fixture snapshot failed");
    report << "QUEUE_ALLOCATION_SNAPSHOT\t" << scenario << '\t' << ordinal << '\t' << snapshot << '\n';
    queue->clear();
    check(queue->getCurrentIndex() == 0 && queue->getSize() == 0 && queue->state().memory == 0 &&
        aid(*editor) == originalAid, "Clear retained stale history accounting or changed the map");
    report << "QUEUE_ALLOCATION_CLEAR_PASS\t" << scenario << '\t' << ordinal << '\n';
    originalTile.reset();
    engine.getState()["g0_queue_ok"] = sol::nil;
    engine.getState()["g0_queue_error"] = sol::nil;
    g_gui.CloseCurrentEditor();
    report << "QUEUE_ALLOCATION_RESTORED\t" << scenario << '\t' << ordinal << '\n';
    return allocations;
}
void run() {
    const auto root = fs::current_path();
    std::ofstream report(root / "snapshot-probe.log");
    report << std::unitbuf;
    try {
        check(root.filename() == "run" && fs::is_regular_file(root / ".g0-queue-allocation") &&
            fs::is_regular_file(root / ".g0-snapshot-fixture"), "Private queue fixture required");
        check(wxApp::IsMainLoopRunning() && g_gui.root->IsEnabled(), "Main loop must be ready");
        wxString error;
        std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Client load failed");
        check(g_luaScripts.isInitialized() || g_luaScripts.initialize(), "Lua initialization failed");
        for (const std::string scenario : {"grow", "count-limit", "merge-redo", "memory-limit"}) {
            const int baseline = oneCase(root, scenario, 0, report);
            check(baseline > 0 && baseline < 32, "Unexpected publication allocation count");
            report << "QUEUE_ALLOCATION_BASELINE\t" << scenario << '\t' << baseline << '\n';
            for (int ordinal = 1; ordinal <= baseline + 4; ++ordinal) oneCase(root, scenario, ordinal, report);
        }
        report << "QUEUE_ALLOCATION_PASS\n";
        report.close();
        std::_Exit(0);
    } catch (const std::exception& error) {
        recording = false;
        report << "FAILED\t" << error.what() << '\n';
        report.close();
        std::_Exit(9);
    }
}
}
class G0QueueAllocationModule : public wxModule {
public:
    bool OnInit() override {
        wxTheApp->Bind(wxEVT_IDLE, &G0QueueAllocationModule::OnIdle, this);
        return true;
    }
    void OnExit() override {}
    void OnIdle(wxIdleEvent& event) {
        event.Skip();
        if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
        wxTheApp->Unbind(wxEVT_IDLE, &G0QueueAllocationModule::OnIdle, this);
        wxTheApp->CallAfter([] { run(); });
    }
private:
    wxDECLARE_DYNAMIC_CLASS(G0QueueAllocationModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G0QueueAllocationModule, wxModule);
