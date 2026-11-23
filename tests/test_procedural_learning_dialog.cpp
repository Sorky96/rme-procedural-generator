// SPDX-License-Identifier: GPL-3.0-or-later
// Real-client, private application-entry probe; synthetic maps, production controls/handlers.
#include "app/main.h"
#include "procedural_probe_warnings.h"
#include "app/managers/version_manager.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "map/tile.h"
#include "game/item.h"
#include "ui/dialogs/terrain_generator_dialog.h"
#include "ui/dialogs/style_learning_dialog.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/module.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <fstream>
#include <stdexcept>

namespace {
using namespace rme::procedural;
void check(bool b, const std::string& s) { if (!b) throw std::runtime_error(s); }
template<class T> T& field(wxWindow& d, const char* name) {
    auto* f = dynamic_cast<T*>(wxWindow::FindWindowByName(name, &d));
    check(f != nullptr, std::string("Missing field: ") + name); return *f;
}
void click(wxWindow& d, const char* name) {
    auto& b = field<wxButton>(d, name); wxCommandEvent e(wxEVT_BUTTON, b.GetId());
    e.SetEventObject(&b); b.GetEventHandler()->ProcessEvent(e);
}
void changed(wxWindow& f, wxEventType type = wxEVT_CHOICE) {
    wxCommandEvent e(type, f.GetId()); e.SetEventObject(&f); f.GetEventHandler()->ProcessEvent(e);
}
template<class Predicate> void until(Predicate p) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    while (!p()) { check(std::chrono::steady_clock::now() < end, "Probe timed out"); wxYieldIfNeeded(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
}
void settle() {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(80);
    until([&] { return std::chrono::steady_clock::now() >= end; });
}
void preview(TerrainGeneratorDialog& d) {
    click(d, "terrain-preview"); until([&] { return field<wxButton>(d, "terrain-preview").IsEnabled(); });
    check(field<wxButton>(d, "terrain-place").IsEnabled(), "Preview refused: " + field<wxStaticText>(d, "terrain-status").GetLabel().ToStdString());
}
void place(TerrainGeneratorDialog& d) {
    // A refused Place only sets the status; close the dialog then instead of waiting forever.
    d.Hide(); wxTheApp->CallAfter([&d] {
        d.SetReturnCode(wxID_NONE); click(d, "terrain-place");
        // Success has already called EndModal(wxID_OK); the loop exits after this handler.
        if (d.GetReturnCode() != wxID_OK) d.EndModal(wxID_CANCEL);
    });
    check(d.ShowModal() == wxID_OK, "Place refused: " + field<wxStaticText>(d, "terrain-status").GetLabel().ToStdString());
}
uint64_t contents(Map& map) {
    uint64_t h = 1469598103934665603ULL;
    auto add = [&](uint64_t v) { h = (h ^ v) * 1099511628211ULL; };
    for (auto& loc : map.tiles()) if (const auto* t = loc.get(); t && !t->empty()) {
        add(t->getPosition().x); add(t->getPosition().y); add(t->getPosition().z);
        add(t->ground ? t->ground->getID() : 0); add(t->items.size());
        for (const auto& item : t->items) add(item->getID());
    }
    return h;
}
// The status label is wrapped to the dialog width; join its lines before searching it.
std::string statusText(wxWindow& d) {
    auto text = field<wxStaticText>(d, "terrain-status").GetLabel().ToStdString();
    std::ranges::replace(text, '\n', ' ');
    return text;
}
// Natural surface, built-in settings, 128 x 128 at (100, 100, 7), built-in profile, one candidate, no plateaus.
TerrainGeneratorDialog* naturalGenerator(const char* seed) {
    auto* d = new TerrainGeneratorDialog(g_gui.root);
    auto& style = field<wxChoice>(*d, "terrain-style"); style.SetSelection(1); changed(style);
    // The I56 part leaves an adjusted all-grass profile chosen; its uniform terrain has nothing to protect.
    auto& builtIn = field<wxChoice>(*d, "terrain-profile"); builtIn.SetSelection(0); changed(builtIn);
    field<wxSpinCtrl>(*d, "terrain-candidate-count").SetValue(1);
    field<wxSpinCtrl>(*d, "terrain-plateau-percent").SetValue(0);
    field<wxTextCtrl>(*d, "terrain-seed").SetValue(seed);
    field<wxSpinCtrl>(*d, "terrain-width").SetValue(128); field<wxSpinCtrl>(*d, "terrain-height").SetValue(128);
    return d;
}
// I57: Learn adds a varied area to a new copy index; the generator lists it; generating the same
// terrain again is blocked at Place, while other terrain places with one Undo and an exact Redo.
void i57CopyIndex(std::ofstream& report, const std::filesystem::path& dir) {
    check(g_gui.NewMap(), "I57 source map"); auto& source = *g_gui.GetCurrentEditor();
    auto* fixture = naturalGenerator("1729"); fixture->Show(); preview(*fixture);
    report << "I57_STAGE source preview\n";
    place(*fixture); fixture->Destroy();
    report << "I57_STAGE source placed\n";
    const auto sourceHash = contents(source.map), undoSize = source.actionQueue->getSize();
    auto* generator = new TerrainGeneratorDialog(g_gui.root); generator->Show();
    std::exception_ptr learningError;
    wxTheApp->CallAfter([&] {
        auto* d = dynamic_cast<StyleLearningDialog*>(wxWindow::FindWindowByName("style-learning-dialog"));
        try {
            check(d != nullptr, "I57 learning entry");
            field<wxCheckBox>(*d, "learn-permission").SetValue(true);
            field<wxSpinCtrl>(*d, "learn-x").SetValue(100); field<wxSpinCtrl>(*d, "learn-y").SetValue(100);
            field<wxSpinCtrl>(*d, "learn-z").SetValue(7);
            field<wxSpinCtrl>(*d, "learn-width").SetValue(128); field<wxSpinCtrl>(*d, "learn-height").SetValue(128);
            click(*d, "learn-read"); until([&] { return field<wxButton>(*d, "learn-analyze").IsEnabled(); });
            report << "I57_STAGE read\n";
            click(*d, "learn-analyze");
            until([&] { return field<wxButton>(*d, "learn-save").IsEnabled() || field<wxButton>(*d, "learn-analyze").IsEnabled(); });
            report << "I57_STAGE analyzed\n";
            check(field<wxButton>(*d, "learn-save").IsEnabled(), "I57 fit failed: " + field<wxStaticText>(*d, "learn-status").GetLabel().ToStdString());
            field<wxCheckBox>(*d, "learn-protect").SetValue(true);
            field<wxTextCtrl>(*d, "learn-index-name").SetValue("../escape");
            field<wxTextCtrl>(*d, "learn-name").SetValue("i57-source"); click(*d, "learn-save");
            check(!std::filesystem::exists(dir / "i57-source.profile") && !std::filesystem::exists(dir.parent_path() / "escape.i53"),
                "Refused index name saved something");
            report << "I57_STAGE refused name checked\n";
            field<wxTextCtrl>(*d, "learn-index-name").SetValue("i57-fixture"); click(*d, "learn-save");
            // A refused save keeps the modal dialog open; report it instead of waiting forever.
            check(std::filesystem::exists(dir / "i57-fixture.i53"),
                "I57 save refused: " + field<wxStaticText>(*d, "learn-status").GetLabel().ToStdString() +
                "\nmapping:\n" + field<wxTextCtrl>(*d, "learn-mapping").GetValue().ToStdString());
        } catch (...) { learningError = std::current_exception(); if (d) d->EndModal(wxID_CANCEL); }
    });
    click(*generator, "terrain-learn");
    if (learningError) std::rethrow_exception(learningError);
    check(contents(source.map) == sourceHash && source.actionQueue->getSize() == undoSize, "I57 learning modified map or Undo");
    const auto ids = loadedClientAssetIds();
    const auto index = readCopyIndex(dir / "i57-fixture.i53", ids->client, ids->otbSha256, ids->datSha256);
    check(index && index->references.size() == 1 && index->references[0].alias == "i57-source", "I57 index written with one area");
    check(field<wxChoice>(*generator, "terrain-copy-index-choice").FindString("i57-fixture (1 area)") != wxNOT_FOUND,
        "I57 index not listed after learning");
    generator->Destroy(); source.map.clearChanges(); g_gui.CloseCurrentEditor();
    report << "I57_LEARN_ADDS_INDEX_NO_MUTATION_PASS\n";

    check(g_gui.NewMap(), "I57 target map"); auto& target = *g_gui.GetCurrentEditor();
    auto* copy = naturalGenerator("1729");
    auto& choice = field<wxChoice>(*copy, "terrain-copy-index-choice");
    check(choice.SetStringSelection("i57-fixture (1 area)"), "I57 index choice"); changed(choice);
    check(field<wxTextCtrl>(*copy, "terrain-copy-index").GetValue() == wxString((dir / "i57-fixture.i53").wstring()), "Choice fills the index path");
    copy->Show();
    click(*copy, "terrain-preview"); until([&] { return field<wxButton>(*copy, "terrain-preview").IsEnabled(); });
    const auto status = statusText(*copy);
    check(!field<wxButton>(*copy, "terrain-place").IsEnabled() && status.find("Place blocked") != std::string::npos,
        "Copy of the learned area not blocked: " + status);
    check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0, "Blocked copy modified target");
    copy->Destroy();
    report << "I57_COPY_BLOCKED_PASS\n";

    // Other seeds: the first one that is placeable at all (some fail reachability; D30) must be
    // evaluated below the copy thresholds and then place normally.
    TerrainGeneratorDialog* other = nullptr;
    for (const char* seed : {"4242", "4243", "4244", "4245", "4246", "4247", "4248", "4249"}) {
        other = naturalGenerator(seed);
        auto& otherChoice = field<wxChoice>(*other, "terrain-copy-index-choice");
        check(otherChoice.SetStringSelection("i57-fixture (1 area)"), "I57 index choice (other seed)"); changed(otherChoice);
        other->Show();
        click(*other, "terrain-preview"); until([&] { return field<wxButton>(*other, "terrain-preview").IsEnabled(); });
        const auto otherStatus = statusText(*other);
        check(otherStatus.find("evaluated (below copy thresholds)") != std::string::npos, "Other terrain not evaluated as clear: " + otherStatus);
        if (field<wxButton>(*other, "terrain-place").IsEnabled()) { report << "I57_STAGE other seed " << seed << '\n'; break; }
        other->Destroy(); other = nullptr;
    }
    check(other != nullptr, "No placeable other seed among 8");
    place(*other); check(target.actionQueue->getSize() == 1 && target.actionQueue->getCurrentIndex() == 1, "I57 Place one Undo");
    const auto placed = contents(target.map); check(target.map.getTileCount() > 0, "I57 empty Place");
    check(target.actionQueue->undo() && target.map.getTileCount() == 0, "I57 Undo");
    check(target.actionQueue->redo() && contents(target.map) == placed, "I57 Redo exact payload");
    other->Destroy(); target.map.clearChanges(); g_gui.CloseCurrentEditor();
    report << "I57_OTHER_TERRAIN_PLACE_UNDO_REDO_PASS\n";
}
void run() {
    std::ofstream report("resolver-probe.log"); report << std::unitbuf;
    try {
        check(std::filesystem::is_regular_file(".g1-resolver-probe"), "Private marker required");
        wxString error; std::vector<std::string> warnings;
        check(g_version.LoadVersion("1098-BlackTek", error, warnings), "Load actual client");
        const auto dir = styleProfileDirectory();
        const auto rel = std::filesystem::weakly_canonical(dir).lexically_relative(std::filesystem::current_path().parent_path());
        check(!rel.empty() && *rel.begin() != "..", "Private profile directory required");
        check(g_gui.NewMap(), "Synthetic source map");
        auto& source = *g_gui.GetCurrentEditor();
        auto* fixture = new TerrainGeneratorDialog(g_gui.root);
        field<wxSpinCtrl>(*fixture, "terrain-candidate-count").SetValue(1);
        field<wxSpinCtrl>(*fixture, "terrain-dirt-weight").SetValue(0);
        field<wxSpinCtrl>(*fixture, "terrain-water-weight").SetValue(0);
        auto& brush = field<wxComboBox>(*fixture, "terrain-grass-brush"); brush.SetValue("grass");
        fixture->Show(); preview(*fixture); place(*fixture); fixture->Destroy();
        const auto sourceHash = contents(source.map), sourceRevision = source.map.getChangeTracker().getGeneration();
        const auto undoSize = source.actionQueue->getSize();
        auto unchanged = [&] {
            check(contents(source.map) == sourceHash && source.map.getChangeTracker().getGeneration() == sourceRevision
                && source.actionQueue->getSize() == undoSize, "Learning modified map or Undo");
        };
        auto* generator = new TerrainGeneratorDialog(g_gui.root); generator->Show();
        std::exception_ptr learningError;
        wxTheApp->CallAfter([&] {
            auto* d = dynamic_cast<StyleLearningDialog*>(wxWindow::FindWindowByName("style-learning-dialog"));
            try {
            check(d != nullptr, "Generator learning entry");
            click(*d, "learn-read");
            check(field<wxStaticText>(*d, "learn-status").GetLabel().Contains("right"), "Permission refusal");
            field<wxCheckBox>(*d, "learn-permission").SetValue(true);
            field<wxSpinCtrl>(*d, "learn-width").SetValue(64); field<wxSpinCtrl>(*d, "learn-height").SetValue(64);
            check(!field<wxButton>(*d, "learn-cancel").IsEnabled(), "Stop enabled while idle");
            click(*d, "learn-read");
            check(field<wxButton>(*d, "learn-cancel").IsEnabled(), "Stop unavailable while reading");
            click(*d, "learn-cancel"); settle();
            check(!field<wxButton>(*d, "learn-analyze").IsEnabled(), "Cancelled read resurfaced"); unchanged();
            check(!field<wxButton>(*d, "learn-cancel").IsEnabled(), "Stop stayed enabled after cancellation");
            click(*d, "learn-read"); until([&] { return field<wxButton>(*d, "learn-analyze").IsEnabled(); });
            check(field<wxTextCtrl>(*d, "learn-mapping").GetValue().Contains("grass"), "Actual brush name mapping");
            check(!field<wxButton>(*d, "learn-cancel").IsEnabled(), "Stop enabled after reading");
            click(*d, "learn-analyze"); click(*d, "learn-cancel");
            until([&] { return field<wxButton>(*d, "learn-read").IsEnabled(); });
            check(!field<wxButton>(*d, "learn-save").IsEnabled(), "Cancelled fit resurfaced"); unchanged();
            click(*d, "learn-analyze");
            until([&] { return field<wxButton>(*d, "learn-save").IsEnabled() || field<wxButton>(*d, "learn-analyze").IsEnabled(); });
            check(field<wxButton>(*d, "learn-save").IsEnabled(), "Fit failed: " + field<wxStaticText>(*d,"learn-status").GetLabel().ToStdString());
            unchanged();
            field<wxTextCtrl>(*d, "learn-name").SetValue("../escape"); click(*d,"learn-save");
            check(!std::filesystem::exists(dir.parent_path()/"escape.profile"), "Name escaped local directory");
            field<wxTextCtrl>(*d, "learn-name").SetValue("i56-user-fixture"); click(*d,"learn-save");
            } catch (...) { learningError = std::current_exception(); if (d) d->EndModal(wxID_CANCEL); }
        });
        click(*generator, "terrain-learn");
        if (learningError) std::rethrow_exception(learningError);
        unchanged();
        check(field<wxChoice>(*generator,"terrain-profile").FindString("i56-user-fixture") != wxNOT_FOUND, "Saved profile not offered immediately");
        generator->Destroy();
        const auto ids = loadedClientAssetIds(); const auto offered = scanStyleProfiles(dir, *ids);
        check(offered.offer.offered.size() == 1, "Normal disk offer");
        const auto profile = offered.offer.offered.front().profile;
        check(candidateBandsFor(profile).empty() && profile.sources[0].alias == "user-local-surface", "No invented M06/M19 provenance");
        report << "LEARNING_PERMISSION_CANCEL_FIT_SAVE_OFFER_NO_MUTATION_PASS\n";
        // A map revision change must invalidate the result even with no dialog input event.
        auto* stale = new StyleLearningDialog(g_gui.root); stale->Show();
        field<wxCheckBox>(*stale,"learn-permission").SetValue(true);
        click(*stale,"learn-read"); until([&] { return field<wxButton>(*stale,"learn-analyze").IsEnabled(); });
        click(*stale,"learn-analyze");
        until([&] { return field<wxButton>(*stale,"learn-save").IsEnabled() || field<wxButton>(*stale,"learn-analyze").IsEnabled(); });
        check(field<wxButton>(*stale,"learn-save").IsEnabled(), "Stale fixture needs a completed fit"); unchanged();
        source.map.getChangeTracker().markTileDirty(100,100,7); settle();
        check(!field<wxButton>(*stale,"learn-save").IsEnabled() && !field<wxButton>(*stale,"learn-analyze").IsEnabled(), "Source revision did not invalidate completed fit/sample");
        click(*stale,"learn-save"); check(!field<wxButton>(*stale,"learn-save").IsEnabled(), "Stale map saved");
        stale->Destroy(); source.map.clearChanges(); g_gui.CloseCurrentEditor();
        report << "LEARNING_SOURCE_INVALIDATION_PASS\n";
        check(g_gui.NewMap(), "Target map"); auto& target = *g_gui.GetCurrentEditor();
        auto* d = new TerrainGeneratorDialog(g_gui.root);
        auto& style = field<wxChoice>(*d,"terrain-style"); style.SetSelection(1); changed(style);
        auto& selected = field<wxChoice>(*d,"terrain-profile"); check(selected.SetStringSelection("i56-user-fixture"), "Learned profile choice"); changed(selected);
        field<wxSpinCtrl>(*d,"terrain-candidate-count").SetValue(1);
        field<wxSpinCtrl>(*d,"terrain-plateau-percent").SetValue(0);
        d->Show(); preview(*d);
        check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0, "Preview modified target");
        click(*d,"terrain-approve");
        auto history = StyleHistoryStore::open(dir/"history.json");
        check(history && history->data().feedback.size() == 1, "Learned profile I54 feedback");
        click(*d,"terrain-adjust"); history = StyleHistoryStore::open(dir/"history.json");
        check(history && history->data().versions.size() == 2
            && history->data().versions.back().profile.sources == profile.sources, "Learned profile I54 Adjust retains user provenance");
        check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0, "I54 modified target");
        preview(*d);
        place(*d); check(target.actionQueue->getSize() == 1 && target.actionQueue->getCurrentIndex() == 1, "Place one Undo");
        const auto placed = contents(target.map); check(target.map.getTileCount() > 0, "Empty Place");
        check(target.actionQueue->undo() && target.map.getTileCount() == 0, "Undo source");
        check(target.actionQueue->redo() && contents(target.map) == placed, "Redo exact payload");
        d->Destroy(); target.map.clearChanges(); g_gui.CloseCurrentEditor();
        report << "LEARNING_PROFILE_PREVIEW_I54_PLACE_UNDO_REDO_PASS\n";
        i57CopyIndex(report, dir);
        report << "LEARNING_DIALOG_PASS\n";
        report.close(); wxTheApp->CallAfter([] { g_gui.root->Close(); });
    } catch (const std::exception& e) { report << "FAILED\t" << e.what() << '\n'; report.close(); std::_Exit(9); }
}
class LearningModule final : public wxModule {
public:
    bool OnInit() override { wxTheApp->Bind(wxEVT_IDLE, &LearningModule::idle, this); return true; }
    void OnExit() override {}
    void idle(wxIdleEvent& e) {
        e.Skip(); if (dismissStartupWarningsDialog()) return;
        if (!g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
        wxTheApp->Unbind(wxEVT_IDLE, &LearningModule::idle, this); wxTheApp->CallAfter([] { run(); });
    }
    wxDECLARE_DYNAMIC_CLASS(LearningModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(LearningModule, wxModule);
}
