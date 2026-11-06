// SPDX-License-Identifier: GPL-3.0-or-later
// Private normal-entry observer. UI commands come exclusively from native operator input.
// The sampler reads only this process' memory counters; it never accesses RME/GUI objects.
#include "app/main.h"
#include "app/managers/version_manager.h"
#include "brushes/managers/brush_manager.h"
#include "brushes/ground/ground_brush.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "map/map.h"
#include "procedural/adapter/snapshot_equal.h"
#include "ui/dialogs/terrain_generator_dialog.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include <wx/eventfilter.h>
#include <wx/module.h>
#include <wx/spinctrl.h>
#include <wx/timer.h>
#include <windows.h>
#include <psapi.h>
#pragma comment(lib,"psapi.lib")
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
using Clock=std::chrono::steady_clock;
using namespace rme::procedural;
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
double millis(Clock::duration value) {return std::chrono::duration<double,std::milli>(value).count();}
struct Memory {uint64_t ws=0,committed=0;};
Memory memory() {
    PROCESS_MEMORY_COUNTERS_EX counters {};
    check(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters)),
        "Process memory counters unavailable");
    return {counters.WorkingSetSize,counters.PrivateUsage};
}
struct Measurement {
    Memory baseline,peak;
    Clock::time_point start,lastSample;
    uint64_t samples=0;
    double maxSampleGap=0,maxHeartbeatGap=0;
};
class ResourceObservation final : public wxEvtHandler,public wxEventFilter {
public:
    ResourceObservation():timer(this) {
        check(std::filesystem::is_regular_file(".g1-dialog-resources"),"Private resource marker required");
        cancelDuringPreview=std::filesystem::is_regular_file(".g1-dialog-cancel-preview");
        report.open("dialog-probe.log");report<<std::unitbuf;
        csv.open("resources.csv");
        csv<<"cycle,side,command,elapsed_ms,max_heartbeat_gap_ms,baseline_ws,peak_ws,end_ws,baseline_private,peak_private,end_private,samples,max_sample_gap_ms\n";
        wxString error;std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek",error,warnings),"Private client load failed");
        check(g_gui.NewMap(),"Private synthetic editor creation failed");
        editor=g_gui.GetCurrentEditor();check(editor,"Private editor missing");
        auto* brush=g_brushes.getBrush("grass");check(brush,"Private grass unavailable");
        auto* grass=brush->as<GroundBrush>();check(grass && !grass->getBorderItems().empty(),"Private grass items unavailable");
        auto* tile=editor->map.createTile(900,900,7);
        tile->setGround(Item::Create(grass->getBorderItems().front().id));tile->ground->setUniqueID(32100);
        sentinel=tile->deepCopy();editor->actionQueue->clear();editor->map.clearChanges();
        generation=editor->map.getGeneration();revision=editor->map.getChangeTracker().getGeneration();
        MEMORYSTATUSEX hostMemory {};hostMemory.dwLength=sizeof(hostMemory);
        check(GlobalMemoryStatusEx(&hostMemory),"Host memory metadata unavailable");
        report<<"RESOURCE_HOST\tlogical_cpus="<<std::thread::hardware_concurrency()<<"\tphysical_bytes="<<hostMemory.ullTotalPhys<<'\n';
        wxEvtHandler::AddFilter(this);Bind(wxEVT_TIMER,&ResourceObservation::tick,this);
        timer.Start(20);lastTick=Clock::now();
        sampler=std::jthread([this](std::stop_token stop){
            try {
                while(!stop.stop_requested()) {
                    {std::scoped_lock lock(metricsMutex);if(active)sample();}
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            } catch(...) {samplerFailed=true;}
        });
        readyMarker();
    }
    ~ResourceObservation() override {timer.Stop();wxEvtHandler::RemoveFilter(this);}
    int FilterEvent(wxEvent& event) override {
        try {
            if(done || event.GetEventType()!=wxEVT_BUTTON)return Event_Skip;
            auto* control=dynamic_cast<wxWindow*>(event.GetEventObject());if(!control)return Event_Skip;
            const auto name=control->GetName();
            if(name=="terrain-preview") {
                check(phase==0,"Unexpected Preview; follow the resource sequence");
                auto* dialog=dynamic_cast<TerrainGeneratorDialog*>(control->GetParent());check(dialog,"Preview parent missing");
                for(const auto* field:{"terrain-width","terrain-height"}) {
                    auto* spin=dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(field,dialog));
                    check(spin && spin->GetValue()==side(),"Wrong resource case dimensions");
                }
                unchanged();begin();phase=1;report<<"RESOURCE_PREVIEW_STARTED\t"<<cycle+1<<'\t'<<side()<<'\n';
            } else if(name=="terrain-validate") {
                check(phase==2,"Validate requires observed Preview");begin();phase=3;
            } else if(name=="terrain-cancel") {
                if(cancelDuringPreview && cycle==0 && phase==1 && !cancelObserved) {
                    auto* status=wxWindow::FindWindowByName("terrain-status",control->GetParent());
                    check(status && status->GetLabel().StartsWith("Preparing preview"),"Cancel missed active Preview");
                    finish("interrupted-preview");begin();phase=7;
                } else {
                    check(phase==4,"Cancel requires observed Validate");begin();phase=5;
                }
            }
        }catch(const std::exception& error){fail(error.what());}
        return Event_Skip;
    }
private:
    int side() const {return cancelDuringPreview || cycle>=3?500:100;}
    void readyMarker(){report<<"RESOURCE_CASE_READY\t"<<cycle+1<<'\t'<<side()<<'\n';}
    void fail(const char* message){report<<"FAILED\t"<<message<<'\n';report.close();csv.flush();std::_Exit(8);}
    void unchanged() {
        check(g_gui.GetCurrentEditor()==editor && editor->map.getGeneration()==generation,"Source identity changed");
        check(editor->map.getTileCount()==1 && editor->actionQueue->getSize()==0 && !editor->map.hasChanged() &&
            editor->map.getChangeTracker().getGeneration()==revision,"Preview/Validate/Cancel changed source/history");
        check(snapshotTilesEqual(editor->map.getTile(900,900,7),sentinel.get()),"Protected sentinel changed");
    }
    void sample() {
        const auto now=Clock::now();const auto current=memory();
        metrics.peak.ws=std::max(metrics.peak.ws,current.ws);
        metrics.peak.committed=std::max(metrics.peak.committed,current.committed);
        metrics.maxSampleGap=std::max(metrics.maxSampleGap,millis(now-metrics.lastSample));
        metrics.lastSample=now;++metrics.samples;
    }
    void begin() {
        std::scoped_lock lock(metricsMutex);
        const auto now=Clock::now();const auto current=memory();
        metrics={current,current,now,now};active=true;
    }
    void finish(const char* command) {
        Measurement result;Memory end;
        double elapsed=0;
        {std::scoped_lock lock(metricsMutex);sample();active=false;end=memory();
            metrics.peak.ws=std::max(metrics.peak.ws,end.ws);metrics.peak.committed=std::max(metrics.peak.committed,end.committed);
            result=metrics;elapsed=millis(Clock::now()-metrics.start);}
        csv<<cycle+1<<','<<side()<<','<<command<<','<<elapsed<<','<<result.maxHeartbeatGap<<','
            <<result.baseline.ws<<','<<result.peak.ws<<','<<end.ws<<','<<result.baseline.committed<<','
            <<result.peak.committed<<','<<end.committed<<','<<result.samples<<','<<result.maxSampleGap<<'\n';csv.flush();
        report<<"RESOURCE_MEASURED\t"<<cycle+1<<'\t'<<side()<<'\t'<<command<<'\t'<<elapsed<<'\n';
    }
    void tick(wxTimerEvent&) {
        try {
            if(done)return;
            const auto now=Clock::now();const double gap=millis(now-lastTick);lastTick=now;
            {std::scoped_lock lock(metricsMutex);if(active)metrics.maxHeartbeatGap=std::max(metrics.maxHeartbeatGap,gap);}
            check(!samplerFailed,"Memory sampling failed");
            check(now-started<std::chrono::minutes(25),"Resource UI run timed out");unchanged();
            TerrainGeneratorDialog* dialog=nullptr;
            for(auto* window:wxTopLevelWindows)if(auto* candidate=dynamic_cast<TerrainGeneratorDialog*>(window);candidate && candidate->IsShown())dialog=candidate;
            auto* status=dialog?wxWindow::FindWindowByName("terrain-status",dialog):nullptr;
            if(phase==7 && status && status->GetLabel().StartsWith("Preview cancelled.")) {
                for(const auto* field:{"terrain-validate","terrain-place"}) {
                    auto* button=wxWindow::FindWindowByName(field,dialog);
                    check(button && !button->IsEnabled(),"Cancelled preview exposed an action");
                }
                finish("interrupt");phase=0;cancelObserved=true;
                report<<"RESOURCE_IN_FLIGHT_CANCEL_PASS\n";
            } else if(phase==1 && status && status->GetLabel().StartsWith("Preview ready:")) {
                check(status->GetLabel().Contains(side()==100?"10000 tiles":"250000 tiles"),"Wrong preview size");
                if(cancelDuringPreview && cycle==0) {
                    check(cancelObserved && status->GetLabel().Contains("(seed 1728)"),"Retry did not publish the new seed");
                    report<<"RESOURCE_CANCEL_RETRY_PASS\n";
                }
                finish("preview");phase=2;report<<"RESOURCE_PREVIEW_ESTIMATE\t"<<cycle+1<<'\t'<<status->GetLabel().ToStdString()<<'\n';
                report<<"RESOURCE_PREVIEW_READY\t"<<cycle+1<<'\n';
            } else if(phase==3 && status && status->GetLabel().StartsWith("Preview validated.")) {
                finish("validate");phase=4;report<<"RESOURCE_VALIDATE_PASS\t"<<cycle+1<<'\n';
            } else if(phase==5 && !dialog) {
                finish("cancel");begin();settleStarted=now;phase=6;
            } else if(phase==6 && now-settleStarted>=std::chrono::seconds(1)) {
                finish("settled");report<<"RESOURCE_CANCEL_PASS\t"<<cycle+1<<'\n';
                ++cycle;phase=0;
                if(cycle<(cancelDuringPreview?3:6)){readyMarker();return;}
                done=true;timer.Stop();report<<"RESOURCE_PASS\n";csv.close();report.close();
                editor->map.clearChanges();editor=nullptr;
                check(g_gui.CloseAllEditors(),"Private editor close failed");
                wxTheApp->CallAfter([]{g_gui.root->Close();});
            }
        }catch(const std::exception& error){fail(error.what());}
    }
    wxTimer timer;
    std::ofstream report,csv;
    Editor* editor=nullptr;
    std::unique_ptr<Tile> sentinel;
    uint64_t generation=0,revision=0;
    int cycle=0,phase=0;
    bool done=false;
    bool cancelDuringPreview=false,cancelObserved=false;
    Clock::time_point started=Clock::now(),lastTick,settleStarted;
    std::mutex metricsMutex;
    Measurement metrics;
    bool active=false;
    std::atomic<bool> samplerFailed=false;
    // Destroyed first: stop/join before any state used by the sampler is destroyed.
    std::jthread sampler;
};
class ResourceProbeModule final : public wxModule {
public:
    bool OnInit() override {wxTheApp->Bind(wxEVT_IDLE,&ResourceProbeModule::idle,this);return true;}
    void OnExit() override {observation.reset();}
    void idle(wxIdleEvent& event) {
        event.Skip();
        if(!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled() || observation)return;
        try{observation=std::make_unique<ResourceObservation>();}
        catch(const std::exception& error){std::ofstream("dialog-probe.log")<<"FAILED\t"<<error.what()<<'\n';std::_Exit(8);}
    }
private:
    std::unique_ptr<ResourceObservation> observation;
    wxDECLARE_DYNAMIC_CLASS(ResourceProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(ResourceProbeModule,wxModule);
}
