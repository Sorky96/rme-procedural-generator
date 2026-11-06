// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <windows.h>
#include <dbghelp.h>
#include <wx/dialog.h>
#include <wx/log.h>

#include <fstream>

#pragma comment(lib, "dbghelp.lib")

// Private diagnostic target. Keep wx's original log output and symbolize only
// the failing SetFocus call on the GUI thread.
class ProceduralFocusTrace final : public wxLogInterposer {
public:
	ProceduralFocusTrace() : report_("focus-trace.log") {
		SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
		symbolsReady_ = SymInitialize(GetCurrentProcess(), nullptr, TRUE);
	}

	~ProceduralFocusTrace() override {
		if (symbolsReady_) SymCleanup(GetCurrentProcess());
	}

protected:
	void DoLogRecord(wxLogLevel level, const wxString& message, const wxLogRecordInfo& info) override {
		if (message.Contains("'SetFocus' failed") && wxIsMainThread()) {
			using CaptureStack = WORD (WINAPI*)(DWORD, DWORD, PVOID*, PDWORD);
			const auto capture = reinterpret_cast<CaptureStack>(
				GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "RtlCaptureStackBackTrace"));
			void* frames[48]{};
			const WORD count = capture ? capture(0, 48, frames, nullptr) : 0;
			report_ << message.ToStdString() << '\n';
			for (USHORT i = 0; i < count; ++i) {
				const auto address = reinterpret_cast<DWORD64>(frames[i]);
				alignas(SYMBOL_INFO) unsigned char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
				auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
				symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
				symbol->MaxNameLen = MAX_SYM_NAME;
				DWORD64 offset = 0;
				IMAGEHLP_MODULE64 module{};
				module.SizeOfStruct = sizeof(module);
				report_ << i << '\t';
				if (symbolsReady_ && SymGetModuleInfo64(GetCurrentProcess(), address, &module)) {
					report_ << module.ModuleName << "+0x" << std::hex << address - module.BaseOfImage << std::dec << '\t';
				}
				if (symbolsReady_ && SymFromAddr(GetCurrentProcess(), address, &offset, symbol)) {
					report_ << symbol->Name << "+0x" << std::hex << offset << std::dec;
					IMAGEHLP_LINE64 line{};
					line.SizeOfStruct = sizeof(line);
					DWORD lineOffset = 0;
					if (SymGetLineFromAddr64(GetCurrentProcess(), address, &lineOffset, &line)) {
						report_ << '\t' << line.FileName << ':' << line.LineNumber;
					}
				}
				report_ << '\n';
			}
			writeWindowState();
				report_.flush();
		}
		wxLogChain::DoLogRecord(level, message, info);
	}

private:
	// Win32 SetFocus fails with ERROR_INVALID_PARAMETER (87) when the target or one of its
	// ancestors is disabled. The failing target is not exposed, so record which windows are.
	void writeWindowState() {
		const auto describe = [this](const char* label, HWND hwnd) {
			report_ << "STATE\t" << label << "\thwnd=" << hwnd;
			if (hwnd) {
				wchar_t cls[64]{};
				GetClassNameW(hwnd, cls, 64);
				report_ << "\tclass=" << wxString(cls).ToStdString() << "\tenabled=" << (IsWindowEnabled(hwnd) != 0)
					<< "\tvisible=" << (IsWindowVisible(hwnd) != 0);
			}
			report_ << '\n';
		};
		describe("foreground", GetForegroundWindow());
		describe("active", GetActiveWindow());
		HWND focus = GetFocus();
		describe("focus", focus);
		for (HWND parent = focus ? GetParent(focus) : nullptr; parent; parent = GetParent(parent)) {
			describe("focus-ancestor", parent);
		}
		for (wxWindow* window : wxTopLevelWindows) {
			const auto* dialog = dynamic_cast<const wxDialog*>(window);
			report_ << "STATE\ttop-level\t" << wxString(window->GetClassInfo()->GetClassName()).ToStdString()
				<< "\ttitle=" << window->GetLabel().ToStdString()
				<< "\twx_enabled=" << window->IsEnabled() << "\twx_shown=" << window->IsShown()
				<< "\tmodal=" << (dialog && dialog->IsModal()) << '\n';
		}
	}

	std::ofstream report_;
	bool symbolsReady_ = false;
};
