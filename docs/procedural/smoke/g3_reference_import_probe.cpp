// Test-only module linked into a separate executable, never the normal rme.exe.
// G3-A: read-only import of a private copy of a reference OTBM; writes an aggregate JSON report.
#include "app/main.h"
#include "ui/gui.h"
#include "app/managers/version_manager.h"
#include "io/iomap_otbm.h"
#include "map/map.h"
#include "procedural/adapter/reference_import.h"
#include "procedural/core/feature_report.h"
#include "procedural/core/features.h"
#include "procedural/core/semantic_image.h"
#include "procedural/core/similarity.h"
#include "procedural/core/topology.h"
#include <wx/module.h>
#include <wx/log.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <stop_token>
#include <string>
#include <thread>

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
        auto readSetting = [&](const char* name) {
            std::ifstream file(root / name);
            std::string value;
            std::getline(file, value);
            return value;
        };
        const std::string clientId = readSetting("client-id.txt");
        const bool clientLoaded = !clientId.empty() && g_version.LoadVersion(clientId, error, warnings);
        if (!clientLoaded) log << "CLIENT_ERROR\t" << error.ToStdString() << '\n';
        for (const auto& warning : warnings) log << "CLIENT_WARNING\t" << warning << '\n';
        check(clientLoaded, "Client load failed");
        if (g_version.GetCurrentVersionIDView() != clientId) {
            log << "CLIENT_MISMATCH\twanted " << clientId << ", loaded " << std::string(g_version.GetCurrentVersionIDView()) << '\n';
        }
        check(g_version.GetCurrentVersionIDView() == clientId, "A different client than requested is loaded");
        ReferenceImportLimits limits;
        limits.maxFileBytes = std::stoull(readSetting("limits.txt"));
        limits.maxProcessBytes = std::stoull(readSetting("memory-limit.txt"));
        if (const std::string factor = readSetting("estimate-factor.txt"); !factor.empty()) limits.loadBytesPerFileByte = std::stoull(factor);
        std::stop_source stopSource;
        limits.stop = stopSource.get_token();
        const auto before = hashReferenceFile(map);
        check(before.has_value(), "Hash before failed");
        // Cancellation test mode: a second thread requests stop after the given delay and the
        // latency from that request to normalizeReference returning is measured.
        const std::string cancelSetting = readSetting("cancel-after-ms.txt");
        std::atomic<long long> requestedAt {0};
        std::jthread canceller;
        if (!cancelSetting.empty()) {
            canceller = std::jthread([&, delay = std::stoul(cancelSetting)] {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                requestedAt = std::chrono::steady_clock::now().time_since_epoch().count();
                stopSource.request_stop();
            });
        }
        ImportMeasurements measurements;
        const auto normalized = normalizeReference(map, limits, SemanticBudget {100'000'000}, &measurements);
        const long long returnedAt = std::chrono::steady_clock::now().time_since_epoch().count();
        canceller = {};
        const auto mib = [](uint64_t bytes) { return bytes >> 20; };
        log << std::format("MEASURE\tlimit={}MiB before={}MiB estimate={}MiB peakAfterLoad={}MiB peakAfterNormalize={}MiB loadMs={} normalizeMs={}\n",
            mib(limits.maxProcessBytes), mib(measurements.processBytesBefore), mib(measurements.estimatedLoadBytes),
            mib(measurements.peakBytesAfterLoad), mib(measurements.peakBytesAfterNormalize), measurements.loadMs, measurements.normalizeMs);
        if (!cancelSetting.empty()) {
            auto cancelPass = [&](const char* stage, long long returnedNs) {
                const long long latencyMs = (returnedNs - requestedAt) / 1'000'000;
                log << std::format("CANCEL_PASS\tstage={} after={}ms latency={}ms loadMs={} normalizeMs={}\n", stage, cancelSetting, latencyMs,
                    measurements.loadMs, measurements.normalizeMs);
                log.close();
                std::_Exit(0);
            };
            if (!normalized && normalized.error() == ReferenceImportError::Cancelled && requestedAt != 0) cancelPass("import", returnedAt);
            if (!normalized) {
                log << "CANCEL_MISSED\t" << referenceImportErrorName(normalized.error()) << '\n';
                check(false, "Import failed instead of being cancelled");
            }
            // The import finished before the request; the analysis stages poll the same token.
            const auto cancelTopology = analyzeTopology(normalized->map, normalized->semantic, TopologyOptions {}, stopSource.get_token());
            const long long topologyReturned = std::chrono::steady_clock::now().time_since_epoch().count();
            if (!cancelTopology && cancelTopology.error() == TopologyError::Cancelled && requestedAt != 0) cancelPass("topology", topologyReturned);
            if (cancelTopology) {
                const auto cancelFeatures = extractFeatures(normalized->map, *cancelTopology, FeatureOptions {}, stopSource.get_token());
                const long long featuresReturned = std::chrono::steady_clock::now().time_since_epoch().count();
                if (!cancelFeatures && cancelFeatures.error() == FeatureError::Cancelled && requestedAt != 0) cancelPass("features", featuresReturned);
            }
            log << "CANCEL_MISSED\tall stages finished before the request\n";
            check(false, "Cancellation was not observed");
        }
        if (!normalized) {
            log << "IMPORT_ERROR\t" << referenceImportErrorName(normalized.error()) << '\n';
            check(false, "Import failed");
        }
        const auto& summary = normalized->summary;
        const auto after = hashReferenceFile(map);
        check(after && *after == *before && *before == summary.sha256, "Input copy changed during import");
        // Optional: the same file through the loader without any abort hook (the editor's path) must
        // give the same number of tiles as the hooked import above.
        if (!readSetting("nohook-check.txt").empty()) {
            Map plain;
            IOMapOTBM plainLoader(plain.getVersion());
            check(plainLoader.loadMap(plain, FileName(wxString(map.wstring()))), "Load without abort hook failed");
            check(!plainLoader.wasAborted(), "Loader reports an abort without a hook");
            log << "NOHOOK_TILES\t" << plain.getTileCount() << "\tIMPORT_TILES\t" << summary.tileCount << '\n';
            check(plain.getTileCount() == summary.tileCount, "Tile count differs without the abort hook");
        }
        { std::ofstream report(root / "reference-report.json", std::ios::binary); report << render(summary); }
        { std::ofstream report(root / "semantic-report.json", std::ios::binary); report << renderSemantic(*normalized); }
        const TopologyOptions topologyOptions;
        const auto topology = analyzeTopology(normalized->map, normalized->semantic, topologyOptions, stopSource.get_token());
        check(topology.has_value(), "Topology analysis failed");
        { std::ofstream report(root / "topology-report.json", std::ios::binary); report << renderTopology(*normalized, *topology, topologyOptions); }
        const auto features = extractFeatures(normalized->map, *topology, FeatureOptions {}, stopSource.get_token());
        check(features.has_value(), "Feature extraction failed");
        const ReportInput reportInput = makeReportInput(*normalized, *topology, *features, topologyOptions);
        fs::create_directory(root / "analysis");
        auto writeText = [&](const std::string& name, const std::string& text) {
            std::ofstream file(root / "analysis" / name, std::ios::binary);
            file << text;
        };
        writeText("reference-analysis.json", renderJson(reportInput));
        writeText("reference-analysis.md", renderMarkdown(reportInput));
        for (const CsvTable table : allCsvTables) writeText(std::string(csvTableName(table)) + ".csv", renderCsv(reportInput, table));
        const auto images = renderSemanticImages(normalized->map, *topology, stopSource.get_token());
        check(images.has_value(), "Semantic image export failed");
        fs::create_directories(root / "analysis" / "semantic");
        writeText("semantic/legend.json", renderSemanticLegendJson(*images));
        for (const auto& floorImage : images->floors) {
            writeText(std::format("semantic/floor-{:02}.png", floorImage.bounds.z),
                std::string(floorImage.png.begin(), floorImage.png.end()));
        }
        // G4-A: shift-invariant window fingerprint for the similarity test and the family split.
        const auto fingerprint = computeFingerprint(normalized->map, FingerprintOptions {}, stopSource.get_token());
        check(fingerprint.has_value(), "Fingerprint failed");
        writeText("fingerprint.fp", serializeFingerprint(*fingerprint));
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
