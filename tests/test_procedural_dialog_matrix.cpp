// SPDX-License-Identifier: GPL-3.0-or-later
// Private native-UI observer. Commands are performed by the operator, never dispatched here.
#include "app/main.h"
#include "app/settings.h"
#include "app/managers/version_manager.h"
#include "brushes/managers/brush_manager.h"
#include "brushes/ground/ground_brush.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "map/map.h"
#include "procedural/adapter/terrain_preparation.h"
#include "procedural/adapter/generation_selection.h"
#include "procedural/adapter/snapshot_equal.h"
#include "ui/dialogs/terrain_generator_dialog.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "ui/theme.h"
#include <wx/module.h>
#include <wx/timer.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>

namespace {
using namespace rme::procedural;
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
const GroundProfile profile {{{"grass","dirt","clear water"}}};
const std::array<Position,5> members {{{200,200,7},{203,200,7},{200,203,7},{203,203,7},{201,201,7}}};
constexpr size_t protectedMember=3;
constexpr uint16_t protectedUid=32101;

class MatrixObservation final : public wxEvtHandler {
public:
    MatrixObservation() : timer(this) {
        check(std::filesystem::is_regular_file(".g1-dialog-probe"),"Private matrix marker required");
        report.open("dialog-probe.log"); report<<std::unitbuf;
        wxString error;std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek",error,warnings),"Private client load failed");
        setup(std::filesystem::is_regular_file(".g1-dialog-selection-only"));
        Bind(wxEVT_TIMER,&MatrixObservation::tick,this);timer.Start(150);
    }
private:
    void setup(bool selected) {
        check(g_gui.NewMap(),"Private matrix editor creation failed");
        editor=g_gui.GetCurrentEditor();check(editor!=nullptr,"No fixture editor");
        auto* brush=g_brushes.getBrush("grass");
        check(brush!=nullptr,"Grass fixture unavailable");
        auto* grass=brush->as<GroundBrush>();
        check(grass && !grass->getBorderItems().empty(),"Grass fixture has no items");
        auto* tile=editor->map.createTile(900,900,7);
        tile->setGround(Item::Create(grass->getBorderItems().front().id));tile->ground->setUniqueID(32100);
        sentinel=tile->deepCopy();
        config={{100,100,7,500,500},1729,3,2,1};
        if (selected) {
            config={{200,200,7,4,4},1729,1,0,0};
            for(size_t i=0;i<members.size();++i) {
                originals[i]=editor->map.createTile(members[i].x,members[i].y,7);
                editor->selection.addInternal(originals[i]);
                check(!originals[i]->isSelected(),"Fixture must include unflagged membership");
            }
            auto* guarded=originals[protectedMember];
            guarded->setGround(Item::Create(grass->getBorderItems().front().id));guarded->ground->setUniqueID(protectedUid);
            protectedGround=guarded->ground.get();
            protectedCopy=guarded->deepCopy();
            hole=editor->map.createTile(202,202,7);
            auto captured=captureGenerationSelection(*editor,{16,1024});
            check(bool(captured),"Fixture selection capture failed");selection=std::move(*captured);
        }
        auto prepared=prepareTerrain(editor->map,config,profile,{250000,254016,250000,512ULL*1024*1024},
            selection?std::span<const uint8_t>(selection->mask):std::span<const uint8_t>());
        check(bool(prepared),"Independent matrix preparation failed");expected=std::move(*prepared);
        check(expected->preview.getChanges().size()==(selected?4:250000),"Unexpected fixture diff");
        if (selected) {
            protectedIntact();
            report<<"MATRIX_SELECTION_PROTECTED_UID_EXCLUDED\t"<<protectedUid<<'\n';
        }
        editor->actionQueue->clear();editor->map.clearChanges();
        generation=editor->map.getGeneration();revision=editor->map.getChangeTracker().getGeneration();
        sourceCount=editor->map.getTileCount();
        phase=0;opened=false;preview=false;validated=false;
        report<<(selected?"MATRIX_SELECTION_READY\t":"MATRIX_500_READY\t")<<expected->preview.getFingerprint()<<'\n';
    }
    void payloadOriginal() {
        check(editor->map.getTileCount()==sourceCount,"Original tile count changed");
        const auto& c=expected->snapshot.context;
        for(uint32_t y=0;y<c.height;++y)for(uint32_t x=0;x<c.width;++x)
            check(snapshotTilesEqual(editor->map.getTile(c.x+int(x),c.y+int(y),c.z),
                expected->snapshot.cells[size_t(y)*c.width+x].original.get()),"Source context changed");
        check(snapshotTilesEqual(editor->map.getTile(900,900,7),sentinel.get()),"Sentinel changed");
    }
    void protectedIntact() {
        if (!selection) return;
        const auto& position=members[protectedMember];
        check(std::ranges::find(expected->preview.getChanges(),position)==expected->preview.getChanges().end(),
            "Protected UID position has a diff entry");
        const auto* tile=editor->map.getTile(position);
        check(tile==originals[protectedMember],"Protected UID tile identity changed");
        check(tile->ground && tile->ground.get()==protectedGround,"Protected UID ground identity changed");
        check(tile->ground->getUniqueID()==protectedUid,"Protected UID changed");
        check(snapshotTilesEqual(tile,protectedCopy.get()),"Protected UID tile contents changed");
    }
    void membership() {
        if (!selection) return;
        check(bool(validateGenerationSelection(*editor,*selection)),"Exact membership lost");
        for(const auto& pos:members)check(!editor->map.getTile(pos)->isSelected(),"Membership invented selection flags");
        check(editor->map.getTile(202,202,7)==hole,"Mask hole identity changed");
    }
    void unchanged() {
        check(g_gui.GetCurrentEditor()==editor && editor->map.getGeneration()==generation,"Source identity changed");
        check(editor->actionQueue->getSize()==0 && !editor->map.hasChanged() &&
            editor->map.getChangeTracker().getGeneration()==revision,"Read-only command changed history/revision");
        payloadOriginal();
        if (!staleInjected || staleRefused) membership();
    }
    void placed() {
        check(editor->actionQueue->getSize()==1 && editor->actionQueue->getCurrentIndex()==1,"Not one history entry");
        check(editor->map.getTileCount()==(selection?sourceCount:sourceCount+250000),"Placed tile count mismatch");
        const auto& c=expected->snapshot.context;
        for(uint32_t y=0;y<c.height;++y)for(uint32_t x=0;x<c.width;++x) {
            const Position pos(c.x+int(x),c.y+int(y),c.z);
            const auto* target=expected->preview.getBuffer().getTile(pos);
            if(!target)target=expected->snapshot.cells[size_t(y)*c.width+x].original.get();
            check(snapshotTilesEqual(editor->map.getTile(pos),target),"Placed/Redo context differs from independent preview");
        }
        check(snapshotTilesEqual(editor->map.getTile(900,900,7),sentinel.get()),"Placement changed sentinel");membership();
        protectedIntact();
    }
    void discard() {
        expected.reset();selection.reset();sentinel.reset();protectedCopy.reset();protectedGround=nullptr;editor->selection.clear();
        editor->actionQueue->clear();editor->map.clearChanges();editor=nullptr;
        check(g_gui.CloseAllEditors(),"Private matrix tabs failed to close");
    }
    void tick(wxTimerEvent&) {
        try {
            if(done)return;
            check(std::chrono::steady_clock::now()-started<std::chrono::minutes(25),"Native matrix timed out");
            TerrainGeneratorDialog* dialog=nullptr;
            for(auto* window:wxTopLevelWindows)if(auto* candidate=dynamic_cast<TerrainGeneratorDialog*>(window);candidate && candidate->IsShown())dialog=candidate;
            if(phase==0 && opened && !dialog && editor->actionQueue->getCurrentIndex()==1) {
                check(preview && validated,"Place without required Preview/Validate observation");
                if(!selection)check(emptyRefused && brushRefused && weightsRefused && boundsRefused && historyRefused,"500 refusal sequence incomplete");
                else check(staleRefused,"Selected stale Place refusal was not tested");
                placed();phase=1;
                if(selection)report<<"MATRIX_SELECTION_PROTECTED_UID_PLACE_INTACT\t"<<protectedUid<<'\n';
                report<<(selection?"MATRIX_SELECTION_COMMIT_PASS\n":"MATRIX_500_COMMIT_PASS\n");
            }
            if(phase==0) {
                unchanged();
                if(!dialog)return;
                opened=true;
                auto* status=wxWindow::FindWindowByName("terrain-status",dialog);
                auto* place=wxWindow::FindWindowByName("terrain-place",dialog);
                check(status && place,"Dialog matrix controls missing");
                const auto text=status->GetLabel();
                if(!selection && !historyRefused && text.StartsWith("Undo history refused this change.")) {
                    report<<"HISTORY_MESSAGE\t"<<text.ToStdString()<<'\n';
                    auto guidance=text;guidance.Replace("\n"," ");
                    check(!place->IsEnabled() && guidance.Contains("40 MiB / 400 actions") &&
                        guidance.Contains("File > Preferences > General") && guidance.Contains("smaller area"),
                        "History refusal has no accurate limits/recovery guidance");
                    check(g_settings.getInteger(Config::UNDO_MEM_SIZE)==40,"Unexpected private Undo limit");
                    historyRefused=true;preview=false;validated=false;
                    report<<"MATRIX_HISTORY_GUIDANCE_PASS\n";
                }
                if(status->GetForegroundColour()==Theme::Get(Theme::Role::Error) && !place->IsEnabled()) {
                    if(!emptyRefused && text.StartsWith("Nothing is selected.")) {emptyRefused=true;report<<"MATRIX_EMPTY_SELECTION_REFUSED\n";}
                    if(!brushRefused && text.StartsWith("A brush name does not match")) {brushRefused=true;report<<"MATRIX_INVALID_BRUSH_REFUSED\n";}
                    if(!weightsRefused && text.StartsWith("At least one terrain weight")) {weightsRefused=true;report<<"MATRIX_ZERO_WEIGHTS_REFUSED\n";}
                    if(!boundsRefused && text.StartsWith("The area lies outside the map.")) {boundsRefused=true;report<<"MATRIX_BOUNDS_REFUSED\n";}
                    if(selection && staleInjected && !staleRefused && text.StartsWith("Map, selection or client data changed.")) {
                        editor->selection.removeInternal(hole);editor->selection.addInternal(originals.back());
                        staleRefused=true;preview=false;validated=false;membership();
                        report<<"MATRIX_STALE_PLACE_REFUSED\n";
                    }
                }
                // Place can only be enabled by a complete production Preview; exact result is checked on Place.
                if(!preview && place->IsEnabled() && text.StartsWith("Preview ready:") &&
                    text.Find(selection?"4 tiles":"250000 tiles")!=wxNOT_FOUND) {
                    if(selection) {
                        auto* region=wxWindow::FindWindowByName("terrain-region",dialog);
                        check(region && region->GetLabel()=="Selection: 4 x 4 at (200, 200, 7)\n5 selected tiles.",
                            "Selection description does not match captured mask");
                        for(const auto* name:{"terrain-x","terrain-y","terrain-z","terrain-width","terrain-height"}) {
                            auto* field=wxWindow::FindWindowByName(name,dialog);
                            check(field && !field->IsShown() && !field->IsEnabled(),"Irrelevant rectangle field shown");
                        }
                        report<<"MATRIX_SELECTION_PRESENTATION_PASS\n";
                    } else if(historyRefused) {
                        check(g_settings.getInteger(Config::UNDO_MEM_SIZE)==512,"Private Undo retry limit not applied");
                        report<<"MATRIX_HISTORY_RETRY_PASS\n";
                    }
                    preview=true;report<<(selection?"MATRIX_SELECTION_PREVIEW_PASS\n":"MATRIX_500_PREVIEW_PASS\n");
                }
                if(text.StartsWith("Preview validated.")) {
                    validated=true;
                    if(selection && !staleInjected) {
                        editor->selection.removeInternal(originals.back());editor->selection.addInternal(hole);
                        staleInjected=true;report<<"MATRIX_STALE_MEMBERSHIP_INJECTED\n";
                    }
                }
                return;
            }
            if(phase==1 && editor->actionQueue->getCurrentIndex()==0) {
                check(editor->actionQueue->canRedo(),"GUI Undo lost Redo");payloadOriginal();membership();
                if(selection)for(size_t i=0;i<members.size();++i)check(editor->map.getTile(members[i])==originals[i],"Undo lost original tile identity");
                protectedIntact();
                if(selection)report<<"MATRIX_SELECTION_PROTECTED_UID_UNDO_INTACT\t"<<protectedUid<<'\n';
                phase=2;report<<(selection?"MATRIX_SELECTION_UNDO_PASS\n":"MATRIX_500_UNDO_PASS\n");return;
            }
            if(phase==2 && editor->actionQueue->getCurrentIndex()==1) {
                placed();
                const bool selected=selection.has_value();
                if(selected)report<<"MATRIX_SELECTION_PROTECTED_UID_REDO_INTACT\t"<<protectedUid<<'\n';
                report<<(selected?"MATRIX_SELECTION_REDO_PASS\n":"MATRIX_500_REDO_PASS\n");
                discard();
                if(!selected) {setup(true);return;}
                done=true;timer.Stop();report<<"MATRIX_PASS\n";report.close();
                wxTheApp->CallAfter([]{g_gui.root->Close();});
            }
        }catch(const std::exception& error){report<<"FAILED\t"<<error.what()<<'\n';report.close();std::_Exit(8);}
    }
    wxTimer timer;
    std::ofstream report;
    Editor* editor=nullptr;
    std::optional<PreparedTerrain> expected;
    std::optional<GenerationSelection> selection;
    std::unique_ptr<Tile> sentinel;
    std::unique_ptr<Tile> protectedCopy;
    const Item* protectedGround=nullptr;
    std::array<Tile*,5> originals {};
    Tile* hole=nullptr;
    GenerationConfig config;
    uint64_t generation=0,revision=0,sourceCount=0;
    int phase=0;
    bool done=false,opened=false,preview=false,validated=false;
    bool emptyRefused=false,brushRefused=false,weightsRefused=false,boundsRefused=false;
    bool staleInjected=false,staleRefused=false;
    bool historyRefused=false;
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
};
class MatrixProbeModule final : public wxModule {
public:
    bool OnInit() override {wxTheApp->Bind(wxEVT_IDLE,&MatrixProbeModule::idle,this);return true;}
    void OnExit() override {observation.reset();}
    void idle(wxIdleEvent& event) {
        event.Skip();
        if(!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled() || observation)return;
        try{observation=std::make_unique<MatrixObservation>();}
        catch(const std::exception& error){std::ofstream("dialog-probe.log")<<"FAILED\t"<<error.what()<<'\n';std::_Exit(8);}
    }
private:
    std::unique_ptr<MatrixObservation> observation;
    wxDECLARE_DYNAMIC_CLASS(MatrixProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(MatrixProbeModule,wxModule);
}
