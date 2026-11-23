// SPDX-License-Identifier: GPL-3.0-or-later
// Private application-entry probe: real client, production controls/handlers and ActionQueue.
#include "app/main.h"
#include "procedural_probe_warnings.h"
#include "app/managers/version_manager.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "procedural/adapter/snapshot_equal.h"
#include "procedural/adapter/style_profile_store.h"
#include "ui/dialogs/terrain_generator_dialog.h"
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/module.h>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <format>
#include "ui/gui.h"
#include "ui/main_frame.h"
#include <wx/checkbox.h>

namespace {
using namespace rme::procedural;
void check(bool b, const std::string& message) { if (!b) throw std::runtime_error(message); }
template<class T> T& control(TerrainGeneratorDialog& d, const char* name) {
    auto* c=dynamic_cast<T*>(wxWindow::FindWindowByName(name,&d));
    check(c!=nullptr,std::string("Missing control: ")+name); return *c;
}
void click(TerrainGeneratorDialog& d,const char* name) {
    auto& c=control<wxButton>(d,name);
    wxCommandEvent e(wxEVT_BUTTON,c.GetId()); e.SetEventObject(&c); c.GetEventHandler()->ProcessEvent(e);
}
void changed(wxWindow& c, wxEventType type=wxEVT_CHOICE) {
    wxCommandEvent e(type,c.GetId());e.SetEventObject(&c);c.GetEventHandler()->ProcessEvent(e);
}
void wait(TerrainGeneratorDialog& d) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(180);
    while (!control<wxButton>(d,"terrain-preview").IsEnabled()) {
        check(std::chrono::steady_clock::now()<end,"Candidate dialog timed out");
        wxYieldIfNeeded();std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void unchanged(Editor& e,uint64_t revision) {
    check(e.map.getTileCount()==0 && e.actionQueue->getSize()==0 &&
        e.map.getChangeTracker().getGeneration()==revision,"Generation or selection changed map/history");
}
void verify(Editor& e,const PreparedTerrain& expected) {
    const auto floor=[&](const PreparedTerrain& p) {
        for (const auto& pos:p.preview.getChanges()) {
            const auto* actual=e.map.getTile(pos);
            const auto* expected=p.preview.getBuffer().getTile(pos);
            // Rendering may refresh derived flags/minimap caches after EndModal.
            check(snapshotTileContentsEqual(actual,expected),std::format(
                "Place/Redo payload differs at {},{},{}: ground {}/{}, items {}/{}",
                pos.x,pos.y,pos.z,actual&&actual->ground?actual->ground->getID():0,
                expected&&expected->ground?expected->ground->getID():0,
                actual?actual->items.size():0,expected?expected->items.size():0));
        }
    };
    floor(expected);if(expected.cave)floor(*expected.cave);
    for(auto* p=expected.plateau.get();p;p=p->plateau.get())floor(*p);
}
void oneRun(uint32_t count,const char* policy,std::ostream& report, const char* copyIndex = "") {
    check(g_gui.NewMap(),"New private map");
    auto& editor=*g_gui.GetCurrentEditor();
    const auto revision=editor.map.getChangeTracker().getGeneration();
    auto* d=new TerrainGeneratorDialog(g_gui.root);
    auto& style=control<wxChoice>(*d,"terrain-style");style.SetSelection(1);changed(style);
    auto& builtIn=control<wxChoice>(*d,"terrain-profile");builtIn.SetSelection(0);changed(builtIn);
    auto& n=control<wxSpinCtrl>(*d,"terrain-candidate-count");
    check(n.GetMin()==1 && n.GetMax()==16 && n.GetValue()==4,"N bounds/default");
    n.SetValue(int(count));changed(n,wxEVT_TEXT);
    if (*policy) {
        auto& p=control<wxChoice>(*d,"terrain-profile");
        const int index=p.FindString(std::string("i52-")+policy+"-fixture");check(index!=wxNOT_FOUND,"Profile fixture not offered");
        p.SetSelection(index);changed(p);
    }
    control<wxSpinCtrl>(*d,"terrain-plateau-percent").SetValue(0);
    control<wxCheckBox>(*d,"terrain-repair").SetValue(false);
    // A single-floor fixture: shape ranking remains active, actual client ground is walkable.
    for(const char* name:{"terrain-grass-brush","terrain-dirt-brush","terrain-rock-brush","terrain-shore-brush","terrain-water-brush"})
    {
        auto& brush=control<wxComboBox>(*d,name);
        if(brush.FindString("grass")==wxNOT_FOUND)brush.Append("grass");
        brush.SetStringSelection("grass");
    }
    control<wxTextCtrl>(*d,"terrain-seed").ChangeValue("1729");
    control<wxTextCtrl>(*d,"terrain-copy-index").SetValue(copyIndex);
    d->Show();click(*d,"terrain-preview");wait(*d);
    auto& list=control<wxListCtrl>(*d,"terrain-candidates");
    check(list.GetItemCount()==count,"Incomplete gallery: "+control<wxStaticText>(*d,"terrain-status").GetLabel().ToStdString());
    unchanged(editor,revision);
    const auto initial=list.GetNextItem(-1,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED);
    check(initial>=0,"No default selection");
    if (!*policy) check(initial==0,"Unranked fallback must be candidate zero");
    const auto target=count > 1 ? uint32_t((initial+1)%count) : 0u;
    list.SetItemState(initial,0,wxLIST_STATE_SELECTED);
    list.SetItemState(long(target),wxLIST_STATE_SELECTED|wxLIST_STATE_FOCUSED,wxLIST_STATE_SELECTED|wxLIST_STATE_FOCUSED);
    wait(*d);unchanged(editor,revision);
    check(control<wxButton>(*d,"terrain-place").IsEnabled(),"Selected candidate must be admissible");
    if (*copyIndex) check(list.GetItemText(target,2).Contains("below copy thresholds"), "Clean indexed result must be assessed");
    else check(list.GetItemText(target,2).Contains("not-evaluated"), "No index must say not evaluated");
    check(list.GetItemText(target).Contains("Selected"),"Text selection label missing");
    check(control<wxTextCtrl>(*d,"terrain-seed").GetValue()=="1729","User seed silently replaced");
    const uint64_t seed=std::stoull(list.GetItemText(target,1).ToStdString());
    check(seed==candidateSeed(1729,target),"Actual candidate seed");
    GenerationConfig config{{100,100,7,100,100},seed};config.algorithm=GenerationAlgorithm::Surface;
    if (*policy) {
        std::ifstream in(std::string(policy)+".profile");std::string text((std::istreambuf_iterator<char>(in)),{});
        const auto p=parseProfile(text);check(bool(p)&&p->tuning.has_value(),"Private profile");config.surface=*p->tuning;
    }
    config.surface.shoreSmoothing=2;config.surface.repairReach=0;config.surface.plateauMinTop=0;config.surface.plateauMaxPercent=0;
    config.surface.pathLength=0;config.surface.buildingDensity=0;config.surface.campCount=0;config.surface.fordLength=0;
    GroundProfile profile{{"grass","grass","grass","grass","grass","grass"},6};
    if (*policy) {
        StyleProfile provenance; provenance.sources={{std::string(policy)=="m06"?"M06":"M19",1000}};
        std::vector<CandidateScore> scores;
        for (uint32_t i=0;i<count;++i) {
            auto trial=config;trial.seed=candidateSeed(1729,i);
            auto candidate=prepareTerrain(editor.map,trial,profile,{100000,120000,500000,2ULL<<30,true});
            check(candidate && candidate->candidateMetrics && candidate->levels,"Independent ranked adapter");
            const bool hardFail=candidateHardFail(true,candidate->levels->report,candidate->levels->repair);
            const auto score=scoreCandidateMetrics(*candidate->candidateMetrics,candidateBandsFor(provenance),hardFail,i);
            check(bool(score),"Independent A score");scores.push_back(*score);
        }
        check(recommendedCandidate(scores,true)==uint32_t(initial),"GUI default differs from independent adapter A ordering");
    }
    auto expected=prepareTerrain(editor.map,config,profile,{100000,120000,500000,2ULL<<30});
    check(bool(expected),"Independent selected adapter failed");
    if (count==1 && !*policy && !*copyIndex) {
        check(!control<wxButton>(*d,"terrain-adjust").IsEnabled(),"I54 built-in Adjust disabled");
        click(*d,"terrain-approve");
        const auto history=StyleHistoryStore::open(styleProfileDirectory()/"history.json");
        check(history && !history->data().feedback.empty() && history->data().feedback.back().base.empty(),"I54 built-in feedback");
        unchanged(editor,revision);report<<"I54_BUILTIN_FEEDBACK_NO_BASE_ADJUST_DISABLED_PASS\n";
    }
    if (count==4 && std::string(policy)=="m06") {
        const auto file=styleProfileDirectory()/"history.json";
        auto state=StyleHistoryStore::open(file);check(bool(state),"I54 private history");
        const auto before=state->data().feedback.size();
        auto& note=control<wxTextCtrl>(*d,"terrain-feedback-note");
        note.SetValue(wxString::FromUTF8((std::string(1023,'a')+"\xc4\x85").c_str()));
        click(*d,"terrain-approve");
        check(control<wxStaticText>(*d,"terrain-status").GetLabel().Contains("1024 UTF-8"),"I54 note overlimit visible");
        check(StyleHistoryStore::open(file)->data().feedback.size()==before,"I54 overlimit persisted");
        note.SetValue(wxString::FromUTF8("Ocena \xc5\xbc\xc3\xb3\xc5\x82\xc4\x87"));
        click(*d,"terrain-approve");
        const auto approved = control<wxStaticText>(*d,"terrain-status").GetLabel().ToStdString();
        click(*d,"terrain-reject");
        state=StyleHistoryStore::open(file);
        check(state && state->data().feedback.size()==before+2,
            "I54 independent explicit feedback: Approve: " + approved + ", Reject: " +
                control<wxStaticText>(*d,"terrain-status").GetLabel().ToStdString());
        const auto& saved=state->data().feedback[before];
        check(saved.candidate==target && saved.actualSeed==seed && saved.userSeed==1729 &&
            saved.note=="Ocena \xc5\xbc\xc3\xb3\xc5\x82\xc4\x87","I54 saved Selected identity/UTF-8");
        unchanged(editor,revision);
        click(*d,"terrain-adjust");
        state=StyleHistoryStore::open(file);check(state && state->data().versions.size()==2,"I54 root and local version");
        const auto first=state->data().active;
        const auto root=state->data().versions[0].id;
        check(list.GetItemCount()==0 && !control<wxButton>(*d,"terrain-place").IsEnabled(),"I54 Adjust invalidates series");
        click(*d,"terrain-approve");
        check(StyleHistoryStore::open(file)->data().feedback.size()==before+3,"I54 stale feedback refused");
        auto& versions=control<wxChoice>(*d,"terrain-history");versions.SetSelection(0);click(*d,"terrain-rollback");
        check(StyleHistoryStore::open(file)->data().active==root,"I54 explicit rollback persisted");
        const auto regenerate=[&] {
            control<wxCheckBox>(*d,"terrain-repair").SetValue(false);
            for(const char* name:{"terrain-grass-brush","terrain-dirt-brush","terrain-rock-brush","terrain-shore-brush","terrain-water-brush"}) {
                auto& brush=control<wxComboBox>(*d,name); if(brush.FindString("grass")==wxNOT_FOUND)brush.Append("grass");brush.SetStringSelection("grass");
            }
            click(*d,"terrain-preview");wait(*d);
            const auto selected=list.GetNextItem(-1,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED);
            if(selected>=0)list.SetItemState(selected,0,wxLIST_STATE_SELECTED);
            list.SetItemState(long(target),wxLIST_STATE_SELECTED|wxLIST_STATE_FOCUSED,wxLIST_STATE_SELECTED|wxLIST_STATE_FOCUSED);wait(*d);
            check(control<wxButton>(*d,"terrain-place").IsEnabled(),"I54 regenerated version admissible");
        };
        regenerate();click(*d,"terrain-adjust");
        state=StyleHistoryStore::open(file);
        check(state && state->data().versions.size()==3 && state->data().versions.back().parent==root && state->data().active!=first,"I54 branch after rollback");
        auto* restored=new TerrainGeneratorDialog(g_gui.root);
        check(control<wxChoice>(*restored,"terrain-profile").GetStringSelection().Contains("Local"),"I54 reopened dialog restores version");
        restored->Destroy();
        regenerate();unchanged(editor,revision);
        // Force stale settings without an event: handler must compare the actual request.
        auto& seedInput=control<wxTextCtrl>(*d,"terrain-seed");seedInput.ChangeValue("1730");
        const auto n=state->data().feedback.size();click(*d,"terrain-reject");
        check(StyleHistoryStore::open(file)->data().feedback.size()==n,"I54 stale request handler refusal");
        seedInput.ChangeValue("1729");
        report<<"I54_SELECTED_FEEDBACK_UTF8_ADJUST_ROLLBACK_BRANCH_REOPEN_STALE_NO_MUTATION_PASS\n";
    }
    d->Hide();
    wxTheApp->CallAfter([d]{click(*d,"terrain-place");});
    check(d->ShowModal()==wxID_OK,"Place failed");
    check(editor.actionQueue->getSize()==1 && editor.actionQueue->getCurrentIndex()==1,"Place must be one Action");
    verify(editor,*expected);
    check(editor.actionQueue->undo() && editor.map.getTileCount()==0,"One Undo did not restore source");
    check(editor.actionQueue->redo(),"Redo refused");verify(editor,*expected);
    report<<"CANDIDATE_DIALOG_PLACE_UNDO_REDO_PASS N="<<count<<" policy="<<policy<<" selected="<<target<<" seed="<<seed<<'\n';
    d->Destroy();editor.map.clearChanges();g_gui.CloseCurrentEditor();
}
void safety(std::ostream& report) {
    check(g_gui.NewMap(), "Safety map");
    auto& editor = *g_gui.GetCurrentEditor();
    const auto revision = editor.map.getChangeTracker().getGeneration();
    for (int mode = 0; mode < 3; ++mode) {
        auto* d = new TerrainGeneratorDialog(g_gui.root);
        auto& style = control<wxChoice>(*d, "terrain-style"); style.SetSelection(1); changed(style);
        control<wxSpinCtrl>(*d, "terrain-width").SetValue(300);
        control<wxSpinCtrl>(*d, "terrain-height").SetValue(300);
        d->Show(); click(*d, "terrain-preview");
        // Let the staged adapter start its owned worker.
        for (int i = 0; i < 6; ++i) { wxYieldIfNeeded(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        if (mode == 0) click(*d, "terrain-cancel");
        else if (mode == 1) {
            auto& seed = control<wxTextCtrl>(*d, "terrain-seed"); seed.SetValue("1730");
        } else { d->Close(); d = nullptr; }
        for (int i = 0; i < 20; ++i) { wxYieldIfNeeded(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        unchanged(editor, revision);
        if (d) {
            check(!control<wxButton>(*d, "terrain-place").IsEnabled() &&
                control<wxListCtrl>(*d, "terrain-candidates").GetItemCount() == 0, "Cancelled/stale result resurfaced");
            d->Destroy();
        }
    }
    auto* d = new TerrainGeneratorDialog(g_gui.root);
    d->Show(); click(*d, "terrain-preview"); wait(*d);
    editor.map.getChangeTracker().markTileDirty(100, 100, 7);
    for (int i = 0; i < 80; ++i) { wxYieldIfNeeded(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    check(!control<wxButton>(*d, "terrain-place").IsEnabled() &&
        control<wxListCtrl>(*d, "terrain-candidates").GetItemCount() == 0, "Map revision did not invalidate gallery");
    d->Destroy(); editor.map.clearChanges(); g_gui.CloseCurrentEditor();
    report << "CANDIDATE_CANCEL_INPUT_CLOSE_STALE_PASS\n";
}

void hardFailure(std::ostream& report) {
    check(g_gui.NewMap(),"HardFail map");auto& editor=*g_gui.GetCurrentEditor();
    auto* d=new TerrainGeneratorDialog(g_gui.root);
    auto& style=control<wxChoice>(*d,"terrain-style");style.SetSelection(1);changed(style);
    auto& builtIn=control<wxChoice>(*d,"terrain-profile");builtIn.SetSelection(0);changed(builtIn);
    control<wxSpinCtrl>(*d,"terrain-candidate-count").SetValue(1);
    control<wxSpinCtrl>(*d,"terrain-cave-entrances").SetValue(1);
    control<wxCheckBox>(*d,"terrain-repair").SetValue(false);
    d->Show();bool found=false;
    for(uint64_t seed=1;seed<=16 && !found;++seed) {
        control<wxTextCtrl>(*d,"terrain-seed").SetValue(std::to_string(seed));
        click(*d,"terrain-preview");wait(*d);
        auto& list=control<wxListCtrl>(*d,"terrain-candidates");
        check(list.GetItemCount()==1,"HardFail fixture did not produce a complete preview");
        if(!list.GetItemText(0,2).Contains("hardFail"))continue;
        found=true;
        check(!control<wxButton>(*d,"terrain-place").IsEnabled(),"hardFail enabled Place");
        click(*d,"terrain-place");
        const auto history=StyleHistoryStore::open(styleProfileDirectory()/"history.json");check(bool(history),"I54 hardFail history");
        const auto size=history->data().feedback.size();
        click(*d,"terrain-approve");click(*d,"terrain-adjust");
        check(StyleHistoryStore::open(styleProfileDirectory()/"history.json")->data().feedback.size()==size,"I54 hardFail approve/adjust refused");
        click(*d,"terrain-reject");
        const auto rejected=StyleHistoryStore::open(styleProfileDirectory()/"history.json");
        check(rejected && rejected->data().feedback.size()==size+1 && rejected->data().feedback.back().hardFail,"I54 hardFail Reject saved");
        check(editor.map.getTileCount()==0 && editor.actionQueue->getSize()==0,"Forced hardFail Place mutated map");
    }
    check(found,"No real-client hardFail fixture in fixed search range");
    d->Destroy();editor.map.clearChanges();g_gui.CloseCurrentEditor();
    report<<"CANDIDATE_REAL_CLIENT_HARDFAIL_BLOCK_PASS\n";
}

void copyGuard(std::ostream& report) {
    check(g_gui.NewMap(), "I53 fixture map");
    auto& editor = *g_gui.GetCurrentEditor();
    const auto revision = editor.map.getChangeTracker().getGeneration();
    const auto ids = loadedClientAssetIds(); check(bool(ids), "I53 assets");
    GenerationConfig config{{100,100,7,100,100},1729}; config.algorithm = GenerationAlgorithm::Surface;
    config.surface.shoreSmoothing = 2; config.surface.repairReach = 0;
    config.surface.plateauMinTop = 0; config.surface.plateauMaxPercent = 0;
    const auto surface = generateSurface({config.region, config.seed, config.surface}, {65001,65001,16}, {10000,100000,10000000});
    check(bool(surface), "I53 fixture surface");
    const auto fp = surfaceClassFingerprint(*surface); check(fp && fp->hashes.size() >= 16, "I53 sufficient fixture");
    CopyIndex index; index.client=ids->client; index.otbSha256=ids->otbSha256; index.datSha256=ids->datSha256; index.references.resize(2);
    index.references[0] = {"M06", std::string(64,'a'), std::string(64,'b'), std::string(64,'c'), *fp};
    Fingerprint independent; independent.windows=independent.candidateWindows=32;
    for (uint64_t i=1; i<=32; ++i) independent.hashes.push_back(i);
    index.references[1] = {"M19", std::string(64,'d'), std::string(64,'e'), std::string(64,'c'), independent};
    GroundProfile ground{{"grass","grass","grass","grass","grass","grass"},6};
    auto baseline = prepareTerrain(editor.map,config,ground,{100000,120000,500000,2ULL<<30});
    check(bool(baseline), "I53 memory baseline");
    auto fullBudget = PreparationBudget{100000,120000,500000,2ULL<<30};
    fullBudget.copyIndex = &index;
    auto charged = prepareTerrain(editor.map,config,ground,fullBudget);
    check(charged && charged->copyAssessment.hardFail() &&
        charged->memory.totalBytes >= baseline->memory.totalBytes + copyWorkingBytes(10000), "I53 scratch was not charged");
    charged = std::unexpected(PreparationInputError::Cancelled);
    auto limited = PreparationBudget{100000,120000,500000,baseline->memory.totalBytes};
    limited.copyIndex = &index;
    const auto refused = prepareTerrain(editor.map,config,ground,limited);
    check(!refused, "I53 under-budget preparation unexpectedly succeeded");
    check(refused.error()==PreparationError(PreparationInputError::MemoryLimit) ||
        refused.error()==PreparationError(SnapshotError::Limit) || refused.error()==PreparationError(ResolveError::PlanLimit) ||
        refused.error()==PreparationError(PreviewError::Limit), "I53 failed for a non-budget reason");
    baseline = std::unexpected(PreparationInputError::Cancelled);
    report << "I53_SHARED_BUDGET_REFUSAL_PASS stage=" << refused.error().index() << '\n';
    { std::ofstream file("copy-fixture.i53", std::ios::binary); file << serializeCopyIndex(index); }
    auto* d = new TerrainGeneratorDialog(g_gui.root);
    auto& style=control<wxChoice>(*d,"terrain-style"); style.SetSelection(1); changed(style);
    auto& builtIn=control<wxChoice>(*d,"terrain-profile");builtIn.SetSelection(0);changed(builtIn);
    control<wxSpinCtrl>(*d,"terrain-candidate-count").SetValue(1);
    control<wxSpinCtrl>(*d,"terrain-plateau-percent").SetValue(0);
    control<wxCheckBox>(*d,"terrain-repair").SetValue(false);
    for (const char* name : {"terrain-grass-brush","terrain-dirt-brush","terrain-rock-brush","terrain-shore-brush","terrain-water-brush"}) {
        auto& brush=control<wxComboBox>(*d,name);
        if (brush.FindString("grass")==wxNOT_FOUND) brush.Append("grass");
        brush.SetStringSelection("grass");
    }
    auto& path=control<wxTextCtrl>(*d,"terrain-copy-index");
    path.SetValue("copy-fixture.i53");
    d->Show(); click(*d,"terrain-preview"); wait(*d);
    auto& list=control<wxListCtrl>(*d,"terrain-candidates");
    check(list.GetItemCount()==1 && list.GetItemText(0,2).Contains("copy thresholds met"),
        "I53 copy not detected: "+control<wxStaticText>(*d,"terrain-status").GetLabel().ToStdString());
    check(!control<wxButton>(*d,"terrain-place").IsEnabled(), "I53 enabled Place");
    click(*d,"terrain-place"); unchanged(editor,revision);
    report << "I53_COPY_FORCED_PLACE_BLOCK_PASS\n";
    { std::ofstream file("broken-fixture.i53"); file << "corrupt\n"; }
    auto wrongAssets = index; wrongAssets.client = "other-client";
    { std::ofstream file("wrong-assets-fixture.i53",std::ios::binary); file << serializeCopyIndex(wrongAssets); }
    for (const char* bad : {"missing-index.i53", "broken-fixture.i53", "wrong-assets-fixture.i53"}) {
        path.SetValue(bad); click(*d,"terrain-preview"); wait(*d);
        check(list.GetItemCount()==0 && !control<wxButton>(*d,"terrain-place").IsEnabled() &&
            control<wxStaticText>(*d,"terrain-status").GetLabel().Contains("Entire series refused"), "Bad explicit index did not refuse series");
        click(*d,"terrain-place"); unchanged(editor,revision);
    }
    report << "I53_BAD_INDEX_REFUSAL_PASS\n";
    path.SetValue("copy-fixture.i53");
    click(*d,"terrain-preview"); click(*d,"terrain-cancel");
    for (int i=0;i<20;++i) { wxYieldIfNeeded(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    check(list.GetItemCount()==0 && !control<wxButton>(*d,"terrain-place").IsEnabled(), "I53 cancelled result resurfaced");
    unchanged(editor,revision);
    report << "I53_CANCEL_PASS\n";
    control<wxSpinCtrl>(*d,"terrain-width").SetValue(8);
    control<wxSpinCtrl>(*d,"terrain-height").SetValue(8);
    click(*d,"terrain-preview"); wait(*d);
    check(list.GetItemCount()==1 && list.GetItemText(0,2).Contains("too few windows"), "Small sample did not say not evaluated");
    unchanged(editor,revision);
    report << "I53_SMALL_SAMPLE_NOT_EVALUATED_PASS\n";
    d->Destroy(); editor.map.clearChanges(); g_gui.CloseCurrentEditor();
    index.references[0].fingerprint = independent;
    for(auto& h:index.references[0].fingerprint.hashes) h += 100;
    { std::ofstream file("clean-fixture.i53", std::ios::binary); file << serializeCopyIndex(index); }
    oneRun(1,"",report,"clean-fixture.i53");
    report << "I53_CLEAR_PLACE_UNDO_REDO_PASS\n";
}

void run() {
    std::ofstream report("resolver-probe.log");report<<std::unitbuf;
    try {
        check(std::filesystem::is_regular_file(".g1-resolver-probe"),"Private marker required");
        wxString error;std::vector<std::string>warnings;
        check(g_version.LoadVersion("1098-BlackTek",error,warnings),"Real client load failed");
        const auto directory=styleProfileDirectory();
        const auto rel=std::filesystem::weakly_canonical(directory).lexically_relative(std::filesystem::current_path().parent_path());
        check(!rel.empty() && *rel.begin()!="..","Profile output must be private");
        std::filesystem::create_directories(directory);
        const auto ids=loadedClientAssetIds();check(bool(ids),"Actual client asset identities");
        for(const char* name:{"m06","m19"}) {
            std::ifstream in(std::string(name)+".profile");
            auto fixture=parseProfile(std::string(std::istreambuf_iterator<char>(in),{}));
            check(fixture && fixture->tuning && candidateBandsFor(*fixture).size()==26,"Private tuning/provenance fixture");
            // These are explicitly test fixtures, not a claim that the offline profile matches this client.
            fixture->client=ids->client;fixture->otbSha256=ids->otbSha256;fixture->datSha256=ids->datSha256;
            const auto encoded=serializeProfile(*fixture);check(bool(encoded),"Fixture serialization");
            const auto file=directory/(std::string("i52-")+name+"-fixture.profile");
            check(!std::filesystem::exists(file),"Do not overwrite existing profile");
            std::ofstream out(file);out<<*encoded;check(bool(out),"Private fixture write");
        }
        for(auto n:{1u,4u,16u})oneRun(n,"",report);
        oneRun(4,"m06",report);oneRun(4,"m19",report);
        safety(report);
        hardFailure(report);
        copyGuard(report);
        report<<"CANDIDATE_DIALOG_PASS\n";report.close();
        wxTheApp->CallAfter([]{g_gui.root->Close();});
    }catch(const std::exception& e){report<<"FAILED\t"<<e.what()<<'\n';report.close();std::_Exit(9);}
}
class CandidateModule final:public wxModule {
public:
    bool OnInit()override{wxTheApp->Bind(wxEVT_IDLE,&CandidateModule::idle,this);return true;}
    void OnExit()override{}
    void idle(wxIdleEvent& e){e.Skip();if(dismissStartupWarningsDialog())return;
        if(!g_gui.root||!wxApp::IsMainLoopRunning()||!g_gui.root->IsEnabled())return;
        wxTheApp->Unbind(wxEVT_IDLE,&CandidateModule::idle,this);wxTheApp->CallAfter([]{run();});}
    wxDECLARE_DYNAMIC_CLASS(CandidateModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(CandidateModule,wxModule);
}
