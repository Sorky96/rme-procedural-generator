// SPDX-License-Identifier: GPL-3.0-or-later
// Private application-entry probe. The operator invokes Preview through native input.
#include "app/main.h"
#include "app/managers/version_manager.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "procedural/adapter/generation_snapshot.h"
#include "procedural/adapter/snapshot_equal.h"
#include "ui/dialogs/terrain_generator_dialog.h"
#include "ui/gui.h"
#include "ui/main_frame.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <wx/choice.h>
#include <wx/eventfilter.h>
#include <wx/module.h>
#include <wx/spinctrl.h>

namespace {
thread_local size_t faultSize = 0;
thread_local bool faultHit = false;
}

void* operator new(size_t size) {
	if (faultSize && size == faultSize) {
		faultSize = 0;
		faultHit = true;
		throw std::bad_alloc();
	}
	if (auto* allocation = std::malloc(size ? size : 1)) return allocation;
	throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, size_t) noexcept { ::operator delete(value); }
void operator delete[](void* value) noexcept { ::operator delete(value); }
void operator delete[](void* value, size_t) noexcept { ::operator delete(value); }

namespace {
const Position fixturePositions[] {{900, 900, 7}, {900, 900, 8}};

void require(bool condition, const char* message) {
	if (!condition) throw std::runtime_error(message);
}

class RefusalProbe final : public wxEvtHandler, public wxEventFilter {
public:
	RefusalProbe() : timer(this), report("dialog-probe.log") {
		report << std::unitbuf;
		loadClient();
		require(g_gui.NewMap(), "Synthetic map creation failed");
		editor = g_gui.GetCurrentEditor();
		require(editor != nullptr, "Synthetic editor missing");
		editor->selection.start(Selection::INTERNAL);
		for (const auto& position : fixturePositions) {
			auto* tile = editor->map.createTile(position.x, position.y, position.z);
			editor->selection.addInternal(tile);
		}
		editor->selection.finish(Selection::INTERNAL);
		for (size_t i = 0; i < std::size(fixturePositions); ++i) {
			original[i] = editor->map.getTile(fixturePositions[i])->deepCopy();
		}
		editor->map.clearChanges();
		editor->actionQueue->clear();
		revision = editor->map.getChangeTracker().getGeneration();
		wxEvtHandler::AddFilter(this);
		Bind(wxEVT_TIMER, &RefusalProbe::tick, this);
		require(timer.Start(20), "Cannot start probe timer");
		report << "REFUSAL_MULTIFLOOR_READY\n";
	}

	~RefusalProbe() override { wxEvtHandler::RemoveFilter(this); }

	int FilterEvent(wxEvent& event) override {
		if (done || event.GetEventType() != wxEVT_BUTTON) return Event_Skip;
		auto* control = dynamic_cast<wxWindow*>(event.GetEventObject());
		if (!control || control->GetName() != "terrain-preview") return Event_Skip;
		try {
			auto* dialog = control->GetParent();
			require(dynamic_cast<TerrainGeneratorDialog*>(dialog) != nullptr, "Missing generator dialog");
			auto* scope = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("terrain-scope", dialog));
			require(scope && scope->GetSelection() == (phase == 0 ? 1 : 0), "Wrong scope for refusal case");
			checkSource();
			if (phase == 3) {
				for (const auto* field : {"terrain-x", "terrain-y", "terrain-width", "terrain-height"}) {
					auto* spin = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(field, dialog));
					require(spin && spin->GetValue() == 100, "Allocation probe expects default rectangle");
				}
				// Only the private probe binary replaces allocation. Match the default rectangle's
				// clipped halo vector, on the GUI thread, once.
				// MSVC's vector allocator adds a header for large aligned blocks.
				faultSize = 104 * 104 * sizeof(rme::procedural::SnapshotCell);
#ifdef _MSC_VER
				faultSize += std::_Non_user_size;
#endif
			}
			pending = true;
		} catch (const std::exception& error) {
			fail(error.what());
		}
		return Event_Skip;
	}

private:
	static void loadClient() {
		wxString error;
		std::vector<std::string> warnings;
		require(g_version.LoadVersion("1098-BlackTek", error, warnings), "Private client load failed");
	}

	[[noreturn]] void fail(const char* message) {
		report << "FAILED\t" << message << '\n';
		std::_Exit(8);
	}

	void checkSource() const {
		require(g_gui.GetCurrentEditor() == editor, "Source changed: current editor");
		require(editor->map.getTileCount() == 2, "Source changed: tile count");
		const uint64_t generation = editor->map.getChangeTracker().getGeneration();
		if (generation != revision) {
			throw std::runtime_error("Source changed: map generation " + std::to_string(revision) + " -> " +
				std::to_string(generation) + " in phase " + std::to_string(phase) + (pending ? " (Preview pending)" : ""));
		}
		require(!editor->map.hasChanged(), "Source changed: map dirty flag");
		require(editor->selection.size() == 2, "Source changed: selection size");
		require(editor->actionQueue->getSize() == 0, "History changed: action queue not empty");
		for (size_t i = 0; i < std::size(fixturePositions); ++i) {
			require(rme::procedural::snapshotTilesEqual(editor->map.getTile(fixturePositions[i]), original[i].get()),
				"Synthetic tile changed");
		}
	}

	void tick(wxTimerEvent&) {
		try {
			checkSource();
			TerrainGeneratorDialog* dialog = nullptr;
			for (auto* window : wxTopLevelWindows) {
				if (auto* candidate = dynamic_cast<TerrainGeneratorDialog*>(window); candidate && candidate->IsShown()) {
					dialog = candidate;
					break;
				}
			}
			if (!dialog && phase == 6) {
				done = true;
				timer.Stop();
				require(g_gui.CloseAllEditors(), "Private editors did not close");
				report << "REFUSAL_PASS\n";
				wxTheApp->CallAfter([] { g_gui.root->Close(); });
				return;
			}
			if (!pending || !dialog) return;
			auto* status = wxWindow::FindWindowByName("terrain-status", dialog);
			require(status != nullptr, "Missing status control");
			const auto label = status->GetLabel();
			constexpr const char* expected[] {
				"The selection spans several floors.", "Finish the current selection",
				"Terrain generation is not available during a live session.",
				"Not enough memory for this preview.", "No client is loaded.", "Preview ready: 10000 tiles"
			};
			if (!label.StartsWith(expected[phase])) return;
			for (const auto* name : {"terrain-place", "terrain-validate"}) {
				auto* button = wxWindow::FindWindowByName(name, dialog);
				require(button && button->IsEnabled() == (phase == 5), "Wrong result action state");
			}
			pending = false;
			report << "REFUSAL_CASE_PASS\t" << phase << '\t' << label.ToStdString() << '\n';
			switch (phase++) {
				case 0: editor->selection.start(Selection::INTERNAL); break;
				case 1: editor->selection.finish(Selection::INTERNAL); editor->live_manager.StartServer(); break;
				case 2: editor->live_manager.CloseServer(); break;
				case 3:
					require(faultHit && !faultSize, "Snapshot allocation fault was not reached");
					g_version.UnloadVersion();
					break;
				case 4: loadClient(); break;
				case 5: break;
			}
		} catch (const std::exception& error) {
			fail(error.what());
		}
	}

	wxTimer timer;
	std::ofstream report;
	Editor* editor = nullptr;
	std::unique_ptr<Tile> original[2];
	uint64_t revision = 0;
	size_t phase = 0;
	bool pending = false;
	bool done = false;
};

class RefusalProbeModule final : public wxModule {
public:
	bool OnInit() override {
		if (std::filesystem::is_regular_file(".g1-dialog-refusals")) {
			wxTheApp->Bind(wxEVT_IDLE, &RefusalProbeModule::idle, this);
		}
		return true;
	}
	void OnExit() override { probe.reset(); }

private:
	void idle(wxIdleEvent& event) {
		event.Skip();
		if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled() || probe) return;
		try {
			probe = std::make_unique<RefusalProbe>();
		} catch (const std::exception& error) {
			std::ofstream("dialog-probe.log", std::ios::app) << "FAILED\t" << error.what() << '\n';
			std::_Exit(8);
		}
	}
	std::unique_ptr<RefusalProbe> probe;
	wxDECLARE_DYNAMIC_CLASS(RefusalProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(RefusalProbeModule, wxModule);
}
