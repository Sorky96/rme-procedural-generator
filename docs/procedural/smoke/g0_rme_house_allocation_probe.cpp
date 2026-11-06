// Test-only allocator replacement. Never link this module into normal rme.exe.
#include "app/main.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "map/map.h"
#include "game/house.h"
#include <wx/module.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <new>

namespace {
thread_local bool failNextAllocation = false;
}
void* operator new(std::size_t size) {
    if (failNextAllocation) {
        failNextAllocation = false;
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void allocationTest(std::ostream& report) {
    Map map;
    auto held = std::make_unique<House>(map);
    held->setID(99);
    held->rent = 4321;
    held->name = "Allocation fixture";
    auto* identity = held.get();
    const auto generation = held->getGeneration();
    bool threw = false;
    failNextAllocation = true;
    try { map.houses.addHouse(std::move(held)); }
    catch (const std::bad_alloc&) { threw = true; }
    const bool consumed = !failNextAllocation;
    failNextAllocation = false;
    check(threw && consumed, "Did not intercept the actual map-node allocation");
    report << "HOUSE_ALLOCATION_THROW_OBSERVED\n";
    check(held.get() == identity && map.houses.count() == 0 && !map.houses.getHouse(99),
        "Map-node allocation failure destroyed or transferred the caller-owned house");
    check(held->getGeneration() == generation && held->rent == 4321 && held->name == "Allocation fixture",
        "House data changed on failed insertion");
    check(map.houses.getEmptyID() == 1, "Failed insertion advanced the house ID allocator");
    report << "HOUSE_ALLOCATION_OWNER_PASS\n";
    check(map.houses.addHouse(std::move(held)) && !held && map.houses.getHouse(99) == identity &&
        identity->getGeneration() == generation, "Retry did not preserve the original house identity");
    report << "HOUSE_ALLOCATION_RETRY_PASS\n";
    auto duplicate = std::make_unique<House>(map);
    duplicate->setID(99);
    auto* duplicateIdentity = duplicate.get();
    check(!map.houses.addHouse(std::move(duplicate)) && duplicate.get() == duplicateIdentity &&
        map.houses.getHouse(99) == identity && map.houses.count() == 1,
        "Duplicate ID refusal consumed the caller-owned house or changed the map");
    report << "HOUSE_ALLOCATION_CONFLICT_PASS\n";
}
void run() {
    namespace fs = std::filesystem;
    const auto root = fs::current_path();
    std::ofstream report(root / "snapshot-probe.log");
    report << std::unitbuf;
    try {
        check(root.filename() == "run" && fs::is_regular_file(root / ".g0-house-allocation") &&
            fs::is_regular_file(root / ".g0-snapshot-fixture"), "Private allocation fixture required");
        check(wxApp::IsMainLoopRunning() && g_gui.root->IsEnabled(), "Main loop must be ready");
        allocationTest(report); // Local map and both ownership states destroyed before success.
        report << "HOUSE_ALLOCATION_PASS\n";
        report.close();
        std::_Exit(0);
    } catch (const std::exception& error) {
        failNextAllocation = false;
        report << "FAILED\t" << error.what() << '\n';
        report.close();
        std::_Exit(9);
    }
}
}
class G0HouseAllocationModule : public wxModule {
public:
    bool OnInit() override {
        wxTheApp->Bind(wxEVT_IDLE, &G0HouseAllocationModule::OnIdle, this);
        return true;
    }
    void OnExit() override {}
    void OnIdle(wxIdleEvent& event) {
        event.Skip();
        if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
        wxTheApp->Unbind(wxEVT_IDLE, &G0HouseAllocationModule::OnIdle, this);
        wxTheApp->CallAfter([] { run(); });
    }
private:
    wxDECLARE_DYNAMIC_CLASS(G0HouseAllocationModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G0HouseAllocationModule, wxModule);
