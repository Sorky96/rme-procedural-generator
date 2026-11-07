// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only helper for wxModule probes; never linked into normal RME.
#pragma once

#include <wx/dialog.h>
#include <wx/window.h>
#include <fstream>

// The application shows a modal "Warnings" dialog at startup when the client load reports
// warnings. It disables the main window, so an unattended probe would wait for an operator.
// Closes that one dialog and records the fact next to the probe log. Returns true if it closed one.
inline bool dismissStartupWarningsDialog() {
	for (auto* window : wxTopLevelWindows) {
		auto* dialog = dynamic_cast<wxDialog*>(window);
		if (dialog && dialog->IsModal() && dialog->GetTitle() == "Warnings") {
			std::ofstream("warnings-dialog-dismissed.txt") << "Startup Warnings dialog closed by the probe" << std::endl;
			dialog->EndModal(wxID_OK);
			return true;
		}
	}
	return false;
}
