// SPDX-License-Identifier: GPL-3.0-or-later
// Private application-entry probe, self-driven: unlike the other dialog probes it sends the dialog's
// commands itself (queued wxEVT_MENU / wxEVT_BUTTON), so it exercises the production handlers but NOT
// native operator input. Purpose: process memory and GDI/USER/handle counts after repeated
// open / in-flight Cancel / Preview / Validate / Cancel cycles. The sampler is the tick itself.
// Marker ".g1-dialog-leak": "<cycles> <side>" or "<cycles> <side> heap"; the heap mode adds Win32 heap and
// VirtualQuery samples per cycle and one sample after HeapCompact at the end.
#include "app/main.h"
#include "app/managers/version_manager.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "map/map.h"
#include "ui/dialogs/terrain_generator_dialog.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/main_frame.h"
#include "ui/main_menubar.h"

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/module.h>
#include <wx/spinctrl.h>
#include <wx/timer.h>
#include <windows.h>
#include <psapi.h>
#include <malloc.h>
#pragma comment(lib, "psapi.lib")
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

void require(bool condition, const char* message) {
	if (!condition) throw std::runtime_error(message);
}

struct Sample {
	uint64_t workingSet = 0;
	uint64_t privateBytes = 0;
	uint32_t gdiObjects = 0;
	uint32_t userObjects = 0;
	uint32_t handles = 0;
};

Sample sampleProcess() {
	PROCESS_MEMORY_COUNTERS_EX counters {};
	require(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)),
		"Process memory counters unavailable");
	DWORD handles = 0;
	require(GetProcessHandleCount(GetCurrentProcess(), &handles), "Handle count unavailable");
	return { counters.WorkingSetSize, counters.PrivateUsage,
		GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS), handles };
}

// Optional heap diagnostics (D4): separates live heap allocations from committed-but-free allocator
// reserve and from private memory outside the Win32 heaps (driver, VirtualAlloc, thread stacks).
// Only the CRT heap is walked block by block: walking the other process heaps (driver heaps) crashed
// the probe. The other heaps contribute HeapSummary totals only. SEH guards turn a fault into a count.
constexpr int BucketCount = 40; // busy block sizes by floor(log2(bytes))

struct HeapWalkTotals {
	uint64_t busy = 0, busyBlocks = 0, free = 0, regionCommitted = 0, uncommitted = 0;
	uint64_t bucketBlocks[BucketCount] {}, bucketBytes[BucketCount] {};
};

struct HeapSample {
	uint64_t vqPrivateCommit = 0, vqPrivateReserve = 0;
	uint64_t summaryAllocated = 0, summaryCommitted = 0, summaryReserved = 0;
	uint64_t crtAllocated = 0, crtCommitted = 0;
	HeapWalkTotals walk;
	uint32_t heapCount = 0, faults = 0;
	double walkMs = 0;
};

bool guardedSummary(HANDLE heap, HEAP_SUMMARY& summary) {
	__try {
		return HeapSummary(heap, 0, &summary) != FALSE;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void walkBlocks(HANDLE heap, HeapWalkTotals& totals) {
	PROCESS_HEAP_ENTRY entry {};
	while (HeapWalk(heap, &entry)) {
		if (entry.wFlags & PROCESS_HEAP_REGION) {
			totals.regionCommitted += entry.Region.dwCommittedSize;
		} else if (entry.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE) {
			totals.uncommitted += entry.cbData;
		} else if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
			totals.busy += entry.cbData;
			++totals.busyBlocks;
			const int bucket = entry.cbData == 0 ? 0 : std::min(BucketCount - 1, std::bit_width(entry.cbData) - 1);
			++totals.bucketBlocks[bucket];
			totals.bucketBytes[bucket] += entry.cbData;
		} else {
			totals.free += entry.cbData;
		}
	}
}

bool guardedWalk(HANDLE heap, HeapWalkTotals& totals) {
	if (!HeapLock(heap)) return false;
	bool ok = true;
	__try {
		walkBlocks(heap, totals);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		ok = false;
	}
	HeapUnlock(heap);
	return ok;
}

HeapSample sampleHeaps() {
	HeapSample result;
	const auto begin = Clock::now();
	MEMORY_BASIC_INFORMATION info {};
	for (auto* address = static_cast<const char*>(nullptr); VirtualQuery(address, &info, sizeof(info)) == sizeof(info);
		 address = static_cast<const char*>(info.BaseAddress) + info.RegionSize) {
		if (info.Type == MEM_PRIVATE && info.State == MEM_COMMIT) result.vqPrivateCommit += info.RegionSize;
		if (info.Type == MEM_PRIVATE && info.State == MEM_RESERVE) result.vqPrivateReserve += info.RegionSize;
	}
	const auto crtHeap = reinterpret_cast<HANDLE>(_get_heap_handle());
	std::vector<HANDLE> heaps(GetProcessHeaps(0, nullptr) + 16);
	heaps.resize(GetProcessHeaps(static_cast<DWORD>(heaps.size()), heaps.data()));
	result.heapCount = static_cast<uint32_t>(heaps.size());
	for (HANDLE heap : heaps) {
		HEAP_SUMMARY summary { sizeof(HEAP_SUMMARY) };
		if (!guardedSummary(heap, summary)) {
			++result.faults;
			continue;
		}
		result.summaryAllocated += summary.cbAllocated;
		result.summaryCommitted += summary.cbCommitted;
		result.summaryReserved += summary.cbReserved;
		if (heap == crtHeap) {
			result.crtAllocated = summary.cbAllocated;
			result.crtCommitted = summary.cbCommitted;
		}
	}
	if (!guardedWalk(crtHeap, result.walk)) ++result.faults;
	result.walkMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
	return result;
}

enum class Phase { Idle, Opening, InFlight, Cancelling, Preview, Validate, Closing, Settle };

class LeakProbe final : public wxEvtHandler {
public:
	LeakProbe() : timer(this), report("dialog-probe.log"), csv("leak-trend.csv") {
		report << std::unitbuf;
		csv << "cycle,working_set,private_bytes,gdi_objects,user_objects,handles\n";
		std::ifstream marker(".g1-dialog-leak");
		require(static_cast<bool>(marker >> cycles >> side) && cycles >= 3 && (side == 100 || side == 500),
			"Marker must contain cycle count (>=3) and side (100 or 500)");
		std::string mode;
		heap = static_cast<bool>(marker >> mode) && mode == "heap";
		if (heap) {
			heapCsv.open("heap-trend.csv");
			bucketCsv.open("heap-buckets.csv");
			heapCsv << "cycle,stage,private_bytes,working_set,vq_private_commit,vq_private_reserve,heap_count,"
					   "summary_allocated,summary_committed,summary_reserved,crt_summary_allocated,crt_summary_committed,"
					   "walk_busy,walk_busy_blocks,walk_free,walk_region_committed,walk_uncommitted,faults,walk_ms\n";
			bucketCsv << "cycle,stage,log2_size,blocks,bytes\n";
		}
		limit = std::max<Clock::duration>(std::chrono::minutes(20), std::chrono::seconds(10) * cycles);
		wxString error;
		std::vector<std::string> warnings;
		require(g_version.LoadVersion("1098-BlackTek", error, warnings), "Private client load failed");
		require(g_gui.NewMap(), "Synthetic map creation failed");
		editor = g_gui.GetCurrentEditor();
		require(editor != nullptr, "Synthetic editor missing");
		editor->map.clearChanges();
		editor->actionQueue->clear();
		revision = editor->map.getChangeTracker().getGeneration();
		Bind(wxEVT_TIMER, &LeakProbe::tick, this);
		started = phaseStarted = Clock::now();
		require(timer.Start(20), "Cannot start probe timer");
		report << std::format("LEAK_READY\tcycles={}\tside={}\n", cycles, side);
	}

private:
	[[noreturn]] void fail(const char* message) {
		report << "FAILED\t" << message << '\n';
		std::_Exit(8);
	}

	void enter(Phase next) {
		phase = next;
		phaseStarted = Clock::now();
	}

	void record() {
		const Sample sample = sampleProcess();
		csv << std::format("{},{},{},{},{},{}\n", cycle, sample.workingSet, sample.privateBytes, sample.gdiObjects,
			sample.userObjects, sample.handles);
		csv.flush();
		if (heap) recordHeap("cycle");
	}

	void recordHeap(std::string_view stage) {
		const HeapSample heapSample = sampleHeaps();
		const Sample sample = sampleProcess();
		const HeapWalkTotals& walk = heapSample.walk;
		heapCsv << std::format("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{:.1f}\n", cycle, stage,
			sample.privateBytes, sample.workingSet, heapSample.vqPrivateCommit, heapSample.vqPrivateReserve,
			heapSample.heapCount, heapSample.summaryAllocated, heapSample.summaryCommitted, heapSample.summaryReserved,
			heapSample.crtAllocated, heapSample.crtCommitted, walk.busy, walk.busyBlocks, walk.free, walk.regionCommitted,
			walk.uncommitted, heapSample.faults, heapSample.walkMs);
		heapCsv.flush();
		for (int bucket = 0; bucket < BucketCount; ++bucket) {
			if (walk.bucketBlocks[bucket] == 0) continue;
			bucketCsv << std::format("{},{},{},{},{}\n", cycle, stage, bucket, walk.bucketBlocks[bucket], walk.bucketBytes[bucket]);
		}
		bucketCsv.flush();
	}

	// After the last cycle only: return free committed CRT heap memory to the OS and sample again. Private
	// memory that disappears here was allocator reserve, not a live allocation.
	void compactAndRecord() {
		HeapCompact(reinterpret_cast<HANDLE>(_get_heap_handle()), 0);
		recordHeap("compact");
	}

	void checkSource() const {
		require(g_gui.GetCurrentEditor() == editor, "Source changed: current editor");
		require(editor->map.getTileCount() == 0 && !editor->map.hasChanged() && editor->actionQueue->getSize() == 0 &&
			editor->map.getChangeTracker().getGeneration() == revision, "Source or history changed");
	}

	TerrainGeneratorDialog* findDialog() const {
		for (auto* window : wxTopLevelWindows) {
			if (auto* dialog = dynamic_cast<TerrainGeneratorDialog*>(window); dialog && dialog->IsShown()) return dialog;
		}
		return nullptr;
	}

	static void click(wxWindow* dialog, const char* name) {
		auto* button = dynamic_cast<wxButton*>(wxWindow::FindWindowByName(name, dialog));
		require(button && button->IsEnabled(), "Dialog button missing or disabled");
		wxCommandEvent event(wxEVT_BUTTON, button->GetId());
		event.SetEventObject(button);
		button->GetEventHandler()->QueueEvent(event.Clone());
	}

	static wxString status(wxWindow* dialog) {
		auto* label = wxWindow::FindWindowByName("terrain-status", dialog);
		require(label != nullptr, "Missing status control");
		return label->GetLabel();
	}

	void tick(wxTimerEvent&) {
		try {
			require(Clock::now() - started < limit, "Leak probe timed out");
			checkSource();
			auto* dialog = findDialog();
			switch (phase) {
				case Phase::Idle: {
					if (dialog || !g_gui.root || !g_gui.root->IsEnabled()) return;
					if (cycle == 0) {
						record();
						report << "LEAK_BASELINE\n";
					}
					if (cycle == cycles) {
						timer.Stop();
						if (heap) compactAndRecord();
						report << "LEAK_PASS\n";
						require(g_gui.CloseAllEditors(), "Private editors did not close");
						wxTheApp->CallAfter([] { g_gui.root->Close(); });
						return;
					}
					wxCommandEvent event(wxEVT_MENU, MAIN_FRAME_MENU + MenuBar::PROCEDURAL_TERRAIN);
					g_gui.root->GetEventHandler()->QueueEvent(event.Clone());
					enter(Phase::Opening);
					return;
				}
				case Phase::Opening: {
					if (!dialog) return;
					for (const auto* field : { "terrain-width", "terrain-height" }) {
						auto* spin = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName(field, dialog));
						require(spin != nullptr, "Missing size control");
						spin->SetValue(side);
					}
					if (cycle == 0) {
						auto* count = dynamic_cast<wxSpinCtrl*>(wxWindow::FindWindowByName("terrain-candidate-count", dialog));
						report << std::format("LEAK_CANDIDATE_COUNT\t{}\tenabled={}\n", count ? count->GetValue() : -1,
							count && count->IsEnabled());
					}
					click(dialog, "terrain-preview");
					enter(Phase::InFlight);
					return;
				}
				case Phase::InFlight: {
					require(dialog != nullptr, "Dialog closed during in-flight Preview");
					// G1 dialog: "Preparing preview"; G6 candidate series: "Generating candidate".
					if (const auto label = status(dialog); !label.StartsWith("Preparing preview") && !label.StartsWith("Generating candidate")) {
						require(Clock::now() - phaseStarted < std::chrono::seconds(5), "Preview did not start");
						return;
					}
					click(dialog, "terrain-cancel");
					enter(Phase::Cancelling);
					return;
				}
				case Phase::Cancelling: {
					require(dialog != nullptr, "In-flight Cancel closed the dialog");
					if (!status(dialog).StartsWith("Preview cancelled.")) {
						require(Clock::now() - phaseStarted < std::chrono::seconds(5), "In-flight Cancel not observed");
						return;
					}
					click(dialog, "terrain-preview");
					enter(Phase::Preview);
					return;
				}
				case Phase::Preview: {
					require(dialog != nullptr, "Dialog closed during Preview");
					const auto label = status(dialog);
					// G6 reports the finished series as "Selected #<n>"; the tile count follows on Validate.
					if (!label.StartsWith("Preview ready:") && !label.StartsWith("Selected #")) {
						require(Clock::now() - phaseStarted < std::chrono::seconds(30), "Preview did not finish");
						return;
					}
					require(label.StartsWith("Selected #") || label.Contains(side == 100 ? "10000 tiles" : "250000 tiles"),
						"Wrong preview size");
					click(dialog, "terrain-validate");
					enter(Phase::Validate);
					return;
				}
				case Phase::Validate: {
					require(dialog != nullptr, "Dialog closed during Validate");
					const auto label = status(dialog);
					if (!label.StartsWith("Preview validated.")) {
						require(Clock::now() - phaseStarted < std::chrono::seconds(30), "Validate did not finish");
						return;
					}
					require(label.Contains(side == 100 ? "10000 tiles" : "250000 tiles"), "Wrong validated size");
					click(dialog, "terrain-cancel");
					enter(Phase::Closing);
					return;
				}
				case Phase::Closing: {
					if (dialog) {
						require(Clock::now() - phaseStarted < std::chrono::seconds(5), "Dialog did not close");
						return;
					}
					enter(Phase::Settle);
					return;
				}
				case Phase::Settle: {
					if (Clock::now() - phaseStarted < std::chrono::seconds(1)) return;
					++cycle;
					record();
					report << std::format("LEAK_CYCLE_PASS\t{}\n", cycle);
					enter(Phase::Idle);
					return;
				}
			}
		} catch (const std::exception& error) {
			fail(error.what());
		}
	}

	wxTimer timer;
	std::ofstream report, csv, heapCsv, bucketCsv;
	bool heap = false;
	Clock::duration limit {};
	Editor* editor = nullptr;
	uint64_t revision = 0;
	int cycles = 0, side = 0, cycle = 0;
	Phase phase = Phase::Idle;
	Clock::time_point started, phaseStarted;
};

class LeakProbeModule final : public wxModule {
public:
	bool OnInit() override {
		if (std::filesystem::is_regular_file(".g1-dialog-leak")) {
			wxTheApp->Bind(wxEVT_IDLE, &LeakProbeModule::idle, this);
		}
		return true;
	}
	void OnExit() override { probe.reset(); }

private:
	void idle(wxIdleEvent& event) {
		event.Skip();
		dismissStartupWarnings();
		if (!g_gui.root ||!wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled() || probe) return;
		// The application's own startup map may still be replaced; wait until the editor is stable.
		Editor* current = g_gui.GetCurrentEditor();
		if (current != lastEditor) {
			lastEditor = current;
			editorSince = Clock::now();
			return;
		}
		if (!current || Clock::now() - editorSince < std::chrono::seconds(2)) return;
		try {
			probe = std::make_unique<LeakProbe>();
		} catch (const std::exception& error) {
			std::ofstream("dialog-probe.log") << "FAILED\t" << error.what() << '\n';
			std::_Exit(8);
		}
	}

	// The startup "Warnings" dialog is modal and disables the main window, so nothing else would run.
	// It is dismissed here and recorded; the other probes rely on the operator closing it.
	void dismissStartupWarnings() {
		if (probe || dismissed) return;
		for (auto* window : wxTopLevelWindows) {
			auto* dialog = dynamic_cast<wxDialog*>(window);
			if (dialog && dialog->IsModal() && dialog->GetTitle() == "Warnings") {
				std::ofstream("warnings-dialog-dismissed.txt") << "Startup Warnings dialog closed by the probe" << std::endl;
				dismissed = true;
				dialog->EndModal(wxID_OK);
				return;
			}
		}
	}

	std::unique_ptr<LeakProbe> probe;
	bool dismissed = false;
	Editor* lastEditor = nullptr;
	Clock::time_point editorSince;
	wxDECLARE_DYNAMIC_CLASS(LeakProbeModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(LeakProbeModule, wxModule);
}
