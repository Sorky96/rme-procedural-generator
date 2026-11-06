// SPDX-License-Identifier: GPL-3.0-or-later
// Private real-application observer. All UI commands are performed through the native UI.
#include "app/main.h"
#include "app/managers/version_manager.h"
#include "brushes/managers/brush_manager.h"
#include "brushes/ground/ground_brush.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "map/map.h"
#include "procedural/adapter/terrain_preparation.h"
#include "procedural/adapter/snapshot_equal.h"
#include "ui/dialogs/terrain_generator_dialog.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "ui/theme.h"
#include <wx/module.h>
#include <wx/timer.h>
#include <wx/textctrl.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {
using namespace rme::procedural;
void check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
constexpr GenerationConfig config {{100,100,7,100,100},1729,3,2,1};
const GroundProfile profile {{{"grass","dirt","clear water"}}};

class DialogObservation final : public wxEvtHandler {
public:
	DialogObservation() : timer(this) {
		check(std::filesystem::is_regular_file(".g1-dialog-probe"),"Private dialog marker required");
		report.open("dialog-probe.log");report<<std::unitbuf;
		wxString error;std::vector<std::string> warnings;
		check(g_version.LoadVersion("1098-BlackTek",error,warnings),"Private client load failed");
		check(g_gui.NewMap(),"Private dialog source creation failed");
		editor=g_gui.GetCurrentEditor();check(editor!=nullptr,"Private source not current");
		auto* grass=g_brushes.getBrush("grass")->as<GroundBrush>();
		check(grass && !grass->getBorderItems().empty(),"Private grass fixture unavailable");
		auto* tile=editor->map.createTile(900,900,7);
		tile->setGround(Item::Create(grass->getBorderItems().front().id));
		tile->ground->setUniqueID(32100);
		sentinel=tile->deepCopy();
		editor->actionQueue->clear();editor->map.clearChanges();
		generation=editor->map.getGeneration();revision=editor->map.getChangeTracker().getGeneration();
		expected=prepareTerrain(editor->map,config,profile,{250000,254016,250000,512ULL*1024*1024});
		check(bool(expected),"Independent default request preparation failed");
		report<<"DIALOG_FIXTURE_READY\t"<<expected->preview.getFingerprint()<<'\n';
		Bind(wxEVT_TIMER,&DialogObservation::tick,this);timer.Start(100);
	}
private:
	void unchanged() {
		check(g_gui.GetCurrentEditor()==editor && editor->map.getGeneration()==generation,"Dialog switched source");
		check(editor->map.getTileCount()==1 && editor->actionQueue->getSize()==0 && !editor->map.hasChanged() &&
			editor->map.getChangeTracker().getGeneration()==revision && snapshotMatchesMap(editor->map,expected->snapshot),
			"Preview, Validate, invalid input or Cancel changed source/history");
		check(snapshotTilesEqual(editor->map.getTile(900,900,7),sentinel.get()),"Protected sentinel changed");
	}
	void placed() {
		check(editor->actionQueue->getSize()==1 && editor->actionQueue->getCurrentIndex()==1 &&
			editor->map.getTileCount()==1+expected->preview.getChanges().size(),"GUI placement not one history entry");
		for (const auto& pos:expected->preview.getChanges()) {
			check(snapshotTilesEqual(editor->map.getTile(pos),expected->preview.getBuffer().getTile(pos)),
				"GUI placement differs from independently prepared concrete result");
		}
		check(snapshotTilesEqual(editor->map.getTile(900,900,7),sentinel.get()),"Placement touched protected sentinel");
	}
	void tick(wxTimerEvent&) {
		try {
			if (phase==4) return;
			check(std::chrono::steady_clock::now()-started<std::chrono::minutes(12),"Native UI test timed out");
			TerrainGeneratorDialog* dialog=nullptr;
			for (auto* window:wxTopLevelWindows) if (auto* candidate=dynamic_cast<TerrainGeneratorDialog*>(window);candidate && candidate->IsShown()) dialog=candidate;
			if (phase==1 && opened && !dialog && editor->actionQueue->getCurrentIndex()==1) {
				placed();phase=2;report<<"DIALOG_GUI_COMMIT_PASS\n";
			}
			if (phase<2) {
				unchanged();
				if (dialog) {
					opened=true;
					auto* place=wxWindow::FindWindowByName("terrain-place",dialog);
					auto* status=wxWindow::FindWindowByName("terrain-status",dialog);
					auto* seed=dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("terrain-seed",dialog));
					check(place && status && seed,"Production dialog controls missing");
					if (phase==0) {
						if (!badSeed && seed->GetValue()=="not-a-seed" && !place->IsEnabled() && status->GetForegroundColour()==Theme::Get(Theme::Role::Error)) {
							badSeed=true;report<<"DIALOG_INVALID_SEED_PASS\n";
						}
						if (!preview && place->IsEnabled()) { preview=true;report<<"DIALOG_PREVIEW_PASS\n"; }
						if (!validated && status->GetLabel().StartsWith("Preview validated.")) { validated=true;report<<"DIALOG_VALIDATE_PASS\n"; }
						if (preview && seed->GetValue()=="1728" && !place->IsEnabled() && !invalidated) { invalidated=true;report<<"DIALOG_INPUT_INVALIDATION_PASS\n"; }
						if (invalidated && seed->GetValue()=="1729" && place->IsEnabled()) regenerated=true;
					}
					return;
				}
				if (opened && phase==0) {
					check(badSeed && preview && validated && invalidated && regenerated,"First dialog UI sequence incomplete");
					phase=1;opened=false;report<<"DIALOG_CANCEL_PASS\n";
				}
				return;
			}
			if (phase==2 && editor->actionQueue->getCurrentIndex()==0) {
				check(editor->actionQueue->canRedo() && editor->map.getTileCount()==1,"GUI Undo lost source or Redo");
				const auto& c=expected->snapshot.context;
				for (uint32_t y=0;y<c.height;++y) for(uint32_t x=0;x<c.width;++x)
					check(snapshotTilesEqual(editor->map.getTile(c.x+int(x),c.y+int(y),c.z),expected->snapshot.cells[size_t(y)*c.width+x].original.get()),"GUI Undo altered source context");
				phase=3;report<<"DIALOG_GUI_UNDO_PASS\n";return;
			}
			if (phase==3 && editor->actionQueue->getCurrentIndex()==1) {
				placed();phase=4;timer.Stop();
				expected=std::unexpected(PreparationInputError::Cancelled);sentinel.reset();editor->actionQueue->clear();editor->map.clearChanges();editor=nullptr;
				check(g_gui.CloseAllEditors(),"Private UI test tabs failed to close");
				report<<"DIALOG_GUI_REDO_PASS\nDIALOG_PASS\n";report.close();
				wxTheApp->CallAfter([] {g_gui.root->Close();});
			}
		} catch(const std::exception& error) { report<<"FAILED\t"<<error.what()<<'\n';report.close();std::_Exit(8); }
	}
private:
	wxTimer timer;
	std::ofstream report;
	Editor* editor=nullptr;
	std::unique_ptr<Tile> sentinel;
	std::expected<PreparedTerrain,PreparationError> expected=std::unexpected(PreparationInputError::Cancelled);
	uint64_t generation=0,revision=0;
	int phase=0;
	bool opened=false,badSeed=false,preview=false,validated=false,invalidated=false,regenerated=false;
	std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
};

class DialogProbeModule final : public wxModule {
public:
	bool OnInit() override { wxTheApp->Bind(wxEVT_IDLE,&DialogProbeModule::idle,this);return true; }
	void OnExit() override { observation.reset(); }
	void idle(wxIdleEvent& event) {
		event.Skip();
		if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
		if (!observation) {
			try { observation=std::make_unique<DialogObservation>(); }
			catch(const std::exception& error) {std::ofstream("dialog-probe.log")<<"FAILED\t"<<error.what()<<'\n';std::_Exit(8);}
		}
	}
private:
	std::unique_ptr<DialogObservation> observation;
	wxDECLARE_DYNAMIC_CLASS(DialogProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(DialogProbeModule,wxModule);
}
