// SPDX-License-Identifier: GPL-3.0-or-later
// D1 private application-entry probe: closing map tabs and the program on client 10.98.
// Commands go through the production frame handlers (queued wxEVT_MENU / notebook PAGE_CLOSE);
// they are not mouse clicks. Case is read from run/d1-case.txt: exit, cancel or tab-close.
#include "app/main.h"
#include "app/managers/version_manager.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/editor_tabs.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/main_frame.h"
#include "ui/main_menubar.h"
#include "../../../tests/procedural_focus_trace.h"

#include <wx/aui/auibook.h>
#include <wx/evtloop.h>
#include <wx/listbox.h>
#include <wx/module.h>
#include <wx/timer.h>
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;

void require(bool condition, const char* message) {
	if (!condition) throw std::runtime_error(message);
}

// Returns the visible "Save changes" message box owned by this GUI thread, if any.
HWND findSavePrompt() {
	HWND found = nullptr;
	EnumThreadWindows(GetCurrentThreadId(), [](HWND hwnd, LPARAM out) -> BOOL {
		wchar_t cls[16]{}, title[64]{};
		GetClassNameW(hwnd, cls, 16);
		GetWindowTextW(hwnd, title, 64);
		if (IsWindowVisible(hwnd) && std::wstring(cls) == L"#32770" && std::wstring(title) == L"Save changes") {
			*reinterpret_cast<HWND*>(out) = hwnd;
			return FALSE;
		}
		return TRUE;
	}, reinterpret_cast<LPARAM>(&found));
	return found;
}

class CloseProbe final : public wxEvtHandler {
public:
	explicit CloseProbe(std::string mode) : timer(this), report("d1-probe.log", std::ios::app), mode(std::move(mode)) {
		report << std::unitbuf;
		require(g_version.IsVersionLoaded() && g_version.GetCurrentVersion().getName() == "1098-BlackTek",
			"Startup did not load private client 10.98");
		const int tabs = this->mode == "tab-close" ? 3 : 2;
		// Startup already opened one empty map; it becomes the first tab.
		const int startupTabs = g_gui.tabbook->GetTabCount();
		require(startupTabs == 1, "Expected exactly the startup map tab");
		editors.push_back(g_gui.GetCurrentEditor());
		while (int(editors.size()) < tabs) {
			require(g_gui.NewMap(), "NewMap failed");
			editors.push_back(g_gui.GetCurrentEditor());
			require(editors.back() != nullptr, "New editor missing");
		}
		if (this->mode == "cancel") {
			// First tab dirty, last (current) tab clean: Exit closes the clean tab, then prompts.
			Map& map = editors.front()->map;
			map.createTile(100, 100, 7);
			map.doChange();
		}
		require(g_gui.tabbook->GetTabCount() == tabs && g_gui.GetCurrentEditor() == editors.back(), "Unexpected initial tabs");
		report << "D1_READY\tcase=" << this->mode << "\ttabs=" << tabs << "\tstartup_tabs=" << startupTabs << "\tclient=" << g_version.GetCurrentVersion().getName() << '\n';
		Bind(wxEVT_TIMER, &CloseProbe::tick, this);
		require(timer.Start(100), "Cannot start D1 observer");
	}
	~CloseProbe() override { timer.Stop(); }

private:
	[[noreturn]] void fail(const char* message) {
		report << "FAILED\t" << message << '\n';
		std::_Exit(8);
	}
	void menu(int item) {
		g_gui.root->GetEventHandler()->QueueEvent(new wxCommandEvent(wxEVT_MENU, MAIN_FRAME_MENU + item));
	}
	// The same event wxAuiNotebook sends for the tab close button of the current page.
	void notebookClose() {
		wxAuiNotebook* notebook = nullptr;
		for (wxWindow* child : g_gui.tabbook->GetChildren()) {
			if ((notebook = dynamic_cast<wxAuiNotebook*>(child))) break;
		}
		require(notebook != nullptr, "Notebook not found");
		auto* event = new wxAuiNotebookEvent(wxEVT_AUINOTEBOOK_PAGE_CLOSE, notebook->GetId());
		event->SetSelection(notebook->GetSelection());
		event->SetEventObject(notebook);
		notebook->GetEventHandler()->QueueEvent(event);
	}
	bool idle() const { return g_gui.root->IsEnabled(); }

	void tick(wxTimerEvent&) {
		try {
			if (mode == "exit") tickExit();
			else if (mode == "cancel") tickCancel();
			else tickTabClose();
		} catch (const std::exception& error) {
			fail(error.what());
		}
	}

	void tickExit() {
		if (step == 0 && idle()) {
			step = 1;
			timer.Stop();
			report << "D1_EXIT_POSTED\ttabs=" << g_gui.tabbook->GetTabCount() << '\n';
			menu(MenuBar::EXIT);
		}
	}

	void tickCancel() {
		if (step == 0 && idle()) {
			step = 1;
			report << "D1_EXIT_POSTED\ttabs=" << g_gui.tabbook->GetTabCount() << '\n';
			menu(MenuBar::EXIT);
		} else if (step == 1) {
			// Runs from the message box's own loop: the prompt is modal.
			if (HWND prompt = findSavePrompt()) {
				require(g_gui.tabbook->GetTabCount() == 1 && g_gui.GetCurrentEditor() == editors.front(),
					"Prompt shown before the clean tab was closed");
				step = 2;
				report << "D1_SAVE_PROMPT\ttabs=1\n";
				// WM_CLOSE on a Yes/No/Cancel prompt returns IDCANCEL.
				PostMessageW(prompt, WM_CLOSE, 0, 0);
			}
		} else if (step == 2 && idle() && !findSavePrompt()) {
			Editor* editor = g_gui.GetCurrentEditor();
			require(g_gui.tabbook->GetTabCount() == 1 && editor == editors.front(), "Cancelled exit lost the dirty tab");
			require(editor->map.hasChanged() && editor->map.getTileCount() == 1 && g_gui.ShouldSave(),
				"Cancelled exit changed the dirty map");
			report << "D1_CANCEL_PASS\ttabs=1\tdirty=1\n";
			// Discard the private change so the second Exit needs no prompt.
			editor->map.clearChanges();
			editor->actionQueue->clear();
			step = 3;
			timer.Stop();
			report << "D1_EXIT_POSTED\ttabs=1\n";
			menu(MenuBar::EXIT);
		}
	}

	void tickTabClose() {
		if (!idle()) return;
		const int tabs = g_gui.tabbook->GetTabCount();
		if (step == 0) {
			step = 1;
			report << "D1_FILE_CLOSE_POSTED\ttabs=" << tabs << '\n';
			menu(MenuBar::CLOSE);
		} else if (step == 1 && tabs == 2) {
			require(g_gui.GetCurrentEditor() == editors[1], "File > Close selected an unexpected tab");
			report << "D1_FILE_CLOSE_PASS\ttabs=2\n";
			step = 2;
			report << "D1_TAB_BUTTON_CLOSE_POSTED\ttabs=2\n";
			notebookClose();
		} else if (step == 2 && tabs == 1) {
			require(g_gui.GetCurrentEditor() == editors[0], "Tab close selected an unexpected tab");
			report << "D1_TAB_BUTTON_CLOSE_PASS\ttabs=1\n";
			step = 3;
			timer.Stop();
			report << "D1_EXIT_POSTED\ttabs=1\n";
			menu(MenuBar::EXIT);
		}
	}

	wxTimer timer;
	std::ofstream report;
	const std::string mode;
	std::vector<Editor*> editors;
	int step = 0;
};

class CloseProbeModule final : public wxModule {
public:
	bool OnInit() override {
		std::ifstream input("d1-case.txt");
		std::getline(input, mode);
		if (mode == "exit" || mode == "cancel" || mode == "tab-close") {
			wxTheApp->Bind(wxEVT_IDLE, &CloseProbeModule::idle, this);
		}
		return true;
	}
	void OnExit() override {
		if (started) std::ofstream("d1-probe.log", std::ios::app) << "D1_MODULE_EXIT\n";
		probe.reset();
		(void)trace.release(); // wx deletes the active log target at shutdown
	}

private:
	void idle(wxIdleEvent& event) {
		event.Skip();
		if (!trace) trace = std::make_unique<ProceduralFocusTrace>();
		if (!started) acknowledgeStartupWarnings();
		// Start only from the main loop, after the startup map exists, not inside a startup modal loop.
		const auto* loop = wxEventLoopBase::GetActive();
		if (started || !g_gui.root || !loop || !loop->IsMain() || !g_gui.root->IsEnabled()) return;
		started = true;
		try {
			probe = std::make_unique<CloseProbe>(mode);
		} catch (const std::exception& error) {
			std::ofstream("d1-probe.log", std::ios::app) << "FAILED\t" << error.what() << '\n';
			std::_Exit(8);
		}
	}
	// Loading client 10.98 at startup lists asset warnings in a modal dialog that an operator
	// dismisses with OK. Idle events also run inside its modal loop.
	void acknowledgeStartupWarnings() {
		for (wxWindow* window : wxTopLevelWindows) {
			auto* dialog = dynamic_cast<wxDialog*>(window);
			if (!dialog || !dialog->IsModal() || dialog->GetTitle() != "Warnings") continue;
			std::ofstream log("d1-probe.log", std::ios::app);
			for (wxWindow* child : dialog->GetChildren()) {
				if (auto* list = dynamic_cast<wxListBox*>(child)) {
					for (unsigned int i = 0; i < list->GetCount(); ++i) {
						log << "D1_STARTUP_WARNING\t" << list->GetString(i).ToStdString() << '\n';
					}
				}
			}
			dialog->EndModal(wxID_OK);
			return;
		}
	}
	std::string mode;
	bool started = false;
	std::unique_ptr<CloseProbe> probe;
	std::unique_ptr<ProceduralFocusTrace> trace;
	wxDECLARE_DYNAMIC_CLASS(CloseProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(CloseProbeModule, wxModule);
}
