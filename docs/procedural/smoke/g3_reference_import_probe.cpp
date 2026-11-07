// Test-only module linked into a separate executable, never the normal rme.exe.
// G3-A: read-only import of a private copy of a reference OTBM; writes an aggregate JSON report.
#include "app/main.h"
#include "ui/gui.h"
#include "app/managers/version_manager.h"
#include "procedural/adapter/reference_import.h"
#include "procedural/core/topology.h"
#include <wx/module.h>
#include <wx/log.h>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace {
namespace fs = std::filesystem;
using namespace rme::procedural;

// Records every wx log record to a file so no modal dialog can block the unattended run.
class FileLog : public wxLog {
public:
    FileLog() : out_(std::filesystem::current_path() / "wx-log.txt", std::ios::app) {}
protected:
    void DoLogRecord(wxLogLevel level, const wxString& message, const wxLogRecordInfo&) override {
        out_ << level << "\t" << message.ToStdString() << std::endl;
    }
private:
    std::ofstream out_;
};

void installFileLog() { delete wxLog::SetActiveTarget(new FileLog); }

void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

std::string render(const ReferenceImportSummary& s) {
    std::string out = "{\n  \"schema\": \"g3-import-report/1\",\n";
    out += std::format("  \"client\": \"{}\",\n  \"sha256\": \"{}\",\n", s.clientId, s.sha256);
    out += std::format("  \"header\": {{\"otbm_version\": {}, \"width\": {}, \"height\": {}, \"items_major\": {}, \"items_minor\": {}, \"compressed\": {}, \"file_bytes\": {}}},\n",
        s.header.otbmVersion, s.header.width, s.header.height, s.header.itemsMajor, s.header.itemsMinor,
        s.header.compressed ? "true" : "false", s.header.fileBytes);
    out += std::format("  \"compatibility\": \"{}\",\n", compatibilityName(s.compatibility));
    out += std::format("  \"tiles\": {},\n  \"items\": {},\n  \"unknown_ground\": {},\n  \"unknown_items\": {},\n  \"loader_warnings\": {},\n",
        s.tileCount, s.itemCount, s.unknownGround, s.unknownItems, s.loaderWarnings);
    out += "  \"tiles_per_floor\": [";
    for (size_t i = 0; i < s.tilesPerFloor.size(); ++i) out += std::format("{}{}", i ? ", " : "", s.tilesPerFloor[i]);
    out += "],\n  \"unknown_ids\": [";
    bool first = true;
    for (const auto& [id, count] : s.unknownIds) {
        out += std::format("{}{{\"id\": {}, \"count\": {}}}", first ? "" : ", ", id, count);
        first = false;
    }
    out += "]\n}\n";
    return out;
}

std::string renderSemantic(const NormalizedReference& n) {
    std::string out = "{\n  \"schema\": \"g3-semantic-report/1\",\n";
    out += std::format("  \"sha256\": \"{}\",\n  \"cells\": {},\n", n.summary.sha256, n.map.cellCount());
    out += "  \"floors\": [";
    bool first = true;
    for (const auto& floor : n.map.floors()) {
        out += std::format("{}{{\"z\": {}, \"width\": {}, \"height\": {}}}", first ? "" : ", ", floor.bounds.z, floor.bounds.width, floor.bounds.height);
        first = false;
    }
    out += "],\n  \"tiles_per_trait\": {";
    for (size_t t = 0; t < traitCount; ++t) out += std::format("{}\"{}\": {}", t ? ", " : "", traitName(t), n.semantic.tilesPerTrait[t]);
    out += "},\n  \"ground_keys\": [";
    const auto& keys = n.map.groundKeys();
    for (size_t i = 0; i < keys.size(); ++i) {
        const auto& stat = n.semantic.groundKeyStats[i];
        out += std::format("{}{{\"key\": \"{}\", \"tiles\": {}, \"blocking_tiles\": {}}}", i ? ", " : "", keys[i], stat.tiles, stat.blockingTiles);
    }
    out += "]\n}\n";
    return out;
}

std::string renderTopology(const NormalizedReference& n, const TopologyReport& t, const TopologyOptions& o) {
    std::string out = "{\n  \"schema\": \"g3-topology-report/1\",\n";
    out += std::format("  \"sha256\": \"{}\",\n  \"options\": {{\"impassable_percent\": {}, \"small_component_cells\": {}}},\n",
        n.summary.sha256, o.impassablePercent, o.smallComponentCells);
    out += "  \"floors\": [\n";
    for (size_t i = 0; i < t.floors.size(); ++i) {
        const auto& f = t.floors[i];
        const auto& w = f.walkable;
        out += std::format("    {{\"z\": {}, \"width\": {}, \"height\": {}, \"tiles\": {}, \"walkable_cells\": {}, \"unknown_cells\": {}, \"floor_change_cells\": {}, \"teleport_cells\": {}, ",
            f.bounds.z, f.bounds.width, f.bounds.height, f.tiles, f.walkableCells, f.unknownCells, f.floorChangeCells, f.teleportCells);
        out += std::format("\"components\": {}, \"largest\": {}, \"small_components\": {}, \"small_cells\": {}, \"top\": [", w.count, w.largest, w.smallCount, w.smallCells);
        for (size_t k = 0; k < w.top.size(); ++k) out += std::format("{}{}", k ? ", " : "", w.top[k]);
        out += "], \"size_buckets\": [";
        for (size_t k = 0; k < w.buckets.size(); ++k) out += std::format("{}{}", k ? ", " : "", w.buckets[k]);
        out += std::format("]}}{}\n", i + 1 < t.floors.size() ? "," : "");
    }
    out += "  ],\n  \"ground_regions\": [\n";
    const auto& keys = n.map.groundKeys();
    for (size_t i = 0; i < keys.size(); ++i) {
        const auto& k = t.keys[i];
        out += std::format("    {{\"key\": \"{}\", \"impassable\": {}, \"tiles\": {}, \"regions\": {}, \"largest\": {}, \"enclosed\": {}}}{}\n",
            keys[i], k.impassable ? "true" : "false", k.tiles, k.regions, k.largest, k.enclosed, i + 1 < keys.size() ? "," : "");
    }
    out += "  ]\n}\n";
    return out;
}

void runProbe() {
    const auto root = fs::current_path();
    std::ofstream log(root / "g3-probe.log");
    log << std::unitbuf;
    try {
        check(fs::is_regular_file(root / ".g3-reference-run"), "Missing private run guard");
        const auto map = root / "reference.otbm";
        wxString error;
        std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Client load failed");
        ReferenceImportLimits limits;
        limits.maxFileBytes = 64ull << 20;
        const auto before = hashReferenceFile(map);
        check(before.has_value(), "Hash before failed");
        const auto normalized = normalizeReference(map, limits, SemanticBudget {200ull << 20});
        if (!normalized) {
            log << "IMPORT_ERROR\t" << referenceImportErrorName(normalized.error()) << '\n';
            check(false, "Import failed");
        }
        const auto& summary = normalized->summary;
        const auto after = hashReferenceFile(map);
        check(after && *after == *before && *before == summary.sha256, "Input copy changed during import");
        { std::ofstream report(root / "reference-report.json", std::ios::binary); report << render(summary); }
        { std::ofstream report(root / "semantic-report.json", std::ios::binary); report << renderSemantic(*normalized); }
        const TopologyOptions topologyOptions;
        const auto topology = analyzeTopology(normalized->map, normalized->semantic, topologyOptions, {});
        check(topology.has_value(), "Topology analysis failed");
        { std::ofstream report(root / "topology-report.json", std::ios::binary); report << renderTopology(*normalized, *topology, topologyOptions); }
        log << "IMPORT_PASS\t" << compatibilityName(summary.compatibility) << '\n';
        log.close();
        std::_Exit(0);
    } catch (const std::exception& failure) {
        log << "FAILED\t" << failure.what() << '\n';
        log.close();
        std::_Exit(9);
    }
}
}

class G3ReferenceImportModule : public wxModule {
public:
    bool OnInit() override {
        installFileLog();
        wxTheApp->Bind(wxEVT_IDLE, &G3ReferenceImportModule::OnIdle, this);
        return true;
    }
    void OnExit() override {}
    void OnIdle(wxIdleEvent& event) {
        event.Skip();
        if (!g_gui.root) return;
        installFileLog();
        wxTheApp->Unbind(wxEVT_IDLE, &G3ReferenceImportModule::OnIdle, this);
        wxTheApp->CallAfter([] { runProbe(); });
    }
private:
    wxDECLARE_DYNAMIC_CLASS(G3ReferenceImportModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G3ReferenceImportModule, wxModule);
