// SPDX-License-Identifier: GPL-3.0-or-later
// Private application-entry observer. Preview, editing and closing use native input.
#include "app/main.h"
#include "app/managers/version_manager.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "procedural/adapter/snapshot_equal.h"
#include "ui/dialogs/terrain_generator_dialog.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "procedural_focus_trace.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <wx/eventfilter.h>
#include <wx/module.h>
#include <wx/spinctrl.h>
#include <wx/timer.h>

namespace {
using Clock = std::chrono::steady_clock;
const Position sentinelPosition(900, 900, 7);

void require(bool condition, const char* message) {
	if (!condition) throw std::runtime_error(message);
}

class InvalidationProbe final : public wxEvtHandler, public wxEventFilter {
public:
	InvalidationProbe() : timer(this), report("dialog-probe.log") {
		report << std::unitbuf;
		wxString error;
		std::vector<std::string> warnings;
		require(g_version.LoadVersion("1098-BlackTek", error, warnings), "Private client load failed");
		require(g_gui.NewMap(), "Private synthetic editor creation failed");
		editor = g_gui.GetCurrentEditor();
		require(editor != nullptr, "Missing synthetic editor");
		sentinel = editor->map.createTile(sentinelPosition.x, sentinelPosition.y, sentinelPosition.z)->deepCopy();
		editor->map.clearChanges();
		editor->actionQueue->clear();
		revision = editor->map.getChangeTracker().getGeneration();
		Bind(wxEVT_TIMER, &InvalidationProbe::tick, this);
		require(timer.Start(20), "Cannot start observer timer");
		wxEvtHandler::AddFilter(this);
		report << "INVALIDATION_FIXTURE_READY\n";
	}

	~InvalidationProbe() override { wxEvtHandler::RemoveFilter(this); }

	int FilterEvent(wxEvent& event) override {
		try {
			auto* control = dynamic_cast<wxWindow*>(event.GetEventObject());
			if (!control) return Event_Skip;
			if (phase == Phase::Edit && event.GetEventType() == wxEVT_TEXT && control->GetName() == "terrain-seed" &&
				status(control->GetParent()).StartsWith("Preparing preview")) {
				editInFlight = true;
				report << "INVALIDATION_EDIT_IN_FLIGHT\n";
			}
			if (phase == Phase::Close && event.GetEventType() == wxEVT_CLOSE_WINDOW &&
				dynamic_cast<TerrainGeneratorDialog*>(control) && status(control).StartsWith("Preparing preview")) {
				closeInFlight = true;
				report << "INVALIDATION_CLOSE_IN_FLIGHT\n";
			}
			if (event.GetEventType() == wxEVT_BUTTON && control->GetName() == "terrain-preview") {
				for (const auto* name : {"terrain-width", "terrain-height"}) {
					auto* spin = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(name, control->GetParent()));
					const int side = phase == Phase::Reopen ? 100 : 500;
					require(spin && spin->GetValue() == side, "Unexpected Preview dimensions");
				}
			}
		} catch (const std::exception& error) {
			fail(error.what());
		}
		return Event_Skip;
	}

private:
	enum class Phase { Edit, WaitAfterEdit, Retry, CloseRetry, Close, WaitAfterClose, Reopen, Finish };

	static wxString status(wxWindow* dialog) {
		auto* control = wxWindow::FindWindowByName("terrain-status", dialog);
		return control ? control->GetLabel() : wxString{};
	}

	[[noreturn]] void fail(const char* message) {
		report << "FAILED\t" << message << '\n';
		std::_Exit(8);
	}

	void checkActions(wxWindow* dialog, bool enabled) {
		for (const auto* name : {"terrain-validate", "terrain-place"}) {
			auto* action = wxWindow::FindWindowByName(name, dialog);
			require(action && action->IsEnabled() == enabled, "Unexpected result action state");
		}
	}

	void tick(wxTimerEvent&) {
		try {
			require(g_gui.GetCurrentEditor() == editor && editor->map.getTileCount() == 1 &&
				editor->map.getChangeTracker().getGeneration() == revision && !editor->map.hasChanged() &&
				editor->actionQueue->getSize() == 0, "Source or history changed");
			require(rme::procedural::snapshotTilesEqual(editor->map.getTile(sentinelPosition), sentinel.get()), "Sentinel changed");
			TerrainGeneratorDialog* dialog = nullptr;
			for (auto* window : wxTopLevelWindows) {
				if (auto* candidate = dynamic_cast<TerrainGeneratorDialog*>(window); candidate && candidate->IsShown()) {
					dialog = candidate;
					break;
				}
			}
			const auto label = dialog ? status(dialog) : wxString{};
			const auto now = Clock::now();
			switch (phase) {
				case Phase::Edit:
					if (editInFlight) {
						require(dialog && label.StartsWith("Settings changed."), "Active edit did not invalidate Preview");
						checkActions(dialog, false);
						settleUntil = now + std::chrono::seconds(2);
						phase = Phase::WaitAfterEdit;
					}
					break;
				case Phase::WaitAfterEdit:
					require(dialog && label.StartsWith("Settings changed."), "Invalidated result resurfaced");
					checkActions(dialog, false);
					if (now >= settleUntil) {
						phase = Phase::Retry;
						report << "INVALIDATION_EDIT_PASS\n";
					}
					break;
				case Phase::Retry:
					if (label.StartsWith("Preview ready:")) {
						require(label.Contains("250000 tiles") && label.Contains("(seed 1728)"), "Retry published a stale result");
						checkActions(dialog, true);
						phase = Phase::CloseRetry;
						report << "INVALIDATION_EDIT_RETRY_PASS\n";
					}
					break;
				case Phase::CloseRetry:
					if (!dialog) {
						phase = Phase::Close;
						report << "INVALIDATION_CLOSE_READY\n";
					}
					break;
				case Phase::Close:
					if (closeInFlight) {
						require(!dialog, "Closing during Preview left the dialog open");
						settleUntil = now + std::chrono::seconds(2);
						phase = Phase::WaitAfterClose;
					}
					break;
				case Phase::WaitAfterClose:
					require(!dialog, "Closed generator resurfaced");
					if (now >= settleUntil) {
						phase = Phase::Reopen;
						report << "INVALIDATION_CLOSE_PASS\n";
					}
					break;
				case Phase::Reopen:
					if (label.StartsWith("Preview ready:")) {
						require(label.Contains("10000 tiles") && label.Contains("(seed 1729)"), "Reopened dialog published a stale result");
						checkActions(dialog, true);
						phase = Phase::Finish;
						report << "INVALIDATION_REOPEN_PASS\n";
					}
					break;
				case Phase::Finish:
					if (!dialog) {
						timer.Stop();
						report << "INVALIDATION_PASS\n";
						// Leave normal application exit to the operator.
					}
					break;
			}
		} catch (const std::exception& error) {
			fail(error.what());
		}
	}

	wxTimer timer;
	std::ofstream report;
	Editor* editor = nullptr;
	std::unique_ptr<Tile> sentinel;
	uint64_t revision = 0;
	Phase phase = Phase::Edit;
	Clock::time_point settleUntil;
	bool editInFlight = false;
	bool closeInFlight = false;
};

class InvalidationProbeModule final : public wxModule {
public:
	bool OnInit() override {
		if (std::filesystem::is_regular_file(".g1-dialog-invalidation")) {
			wxTheApp->Bind(wxEVT_IDLE, &InvalidationProbeModule::idle, this);
		}
		return true;
	}
	void OnExit() override { probe.reset(); trace.reset(); }
private:
	void idle(wxIdleEvent& event) {
		event.Skip();
		if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled() || probe) return;
		try {
			if (std::filesystem::is_regular_file(".g1-focus-trace")) {
				trace = std::make_unique<ProceduralFocusTrace>();
			}
			probe = std::make_unique<InvalidationProbe>();
		} catch (const std::exception& error) {
			std::ofstream("dialog-probe.log", std::ios::app) << "FAILED\t" << error.what() << '\n';
			std::_Exit(8);
		}
	}
	std::unique_ptr<InvalidationProbe> probe;
	std::unique_ptr<ProceduralFocusTrace> trace;
	wxDECLARE_DYNAMIC_CLASS(InvalidationProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(InvalidationProbeModule, wxModule);
}
