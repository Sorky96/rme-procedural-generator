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
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/modalhook.h>
#include <charconv>
#include <cstdio>
#include <wx/panel.h>
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
// I58: the rows of a two-column report list.
std::vector<std::pair<std::string, std::string>> rows(wxWindow& d, const char* name) {
    auto& list = field<wxListCtrl>(d, name);
    std::vector<std::pair<std::string, std::string>> out;
    for (long i = 0; i < list.GetItemCount(); ++i) out.emplace_back(list.GetItemText(i, 0).ToStdString(), list.GetItemText(i, 1).ToStdString());
    return out;
}
bool hasRow(const std::vector<std::pair<std::string, std::string>>& list, std::string_view first, std::string_view second = {}) {
    return std::ranges::any_of(list, [&](const auto& row) { return row.first == first && row.second.find(second) != std::string::npos; });
}
// Both lists filled; a Blocking warning exactly when Place is refused.
void reportConsistent(wxWindow& d, const std::string& where) {
    const auto statistics = rows(d, "terrain-statistics"), warnings = rows(d, "terrain-warnings");
    check(hasRow(statistics, "Area") && hasRow(statistics, "Places reached") && !warnings.empty(), where + ": report lists empty");
    check(hasRow(warnings, "Blocking") == !field<wxButton>(d, "terrain-place").IsEnabled(),
        where + ": Blocking warnings disagree with Place; status " + statusText(d));
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
    reportConsistent(*copy, "I58 copy");
    check(hasRow(rows(*copy, "terrain-warnings"), "Blocking", "Copies i57-source"), "I58 copy warning does not name the reference");
    check(hasRow(rows(*copy, "terrain-statistics"), "Similarity to i57-source"), "I58 similarity row missing");
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
    reportConsistent(*other, "I58 other terrain");
    check(hasRow(rows(*other, "terrain-statistics"), "Grass", " cells)") && hasRow(rows(*other, "terrain-statistics"), "Similarity to i57-source"),
        "I58 terrain shares or similarity missing");
    report << "I58_COPY_REPORT_PASS\n";
    place(*other); check(target.actionQueue->getSize() == 1 && target.actionQueue->getCurrentIndex() == 1, "I57 Place one Undo");
    const auto placed = contents(target.map); check(target.map.getTileCount() > 0, "I57 empty Place");
    check(target.actionQueue->undo() && target.map.getTileCount() == 0, "I57 Undo");
    check(target.actionQueue->redo() && contents(target.map) == placed, "I57 Redo exact payload");
    other->Destroy(); target.map.clearChanges(); g_gui.CloseCurrentEditor();
    report << "I57_OTHER_TERRAIN_PLACE_UNDO_REDO_PASS\n";
}
// I58: two candidates; selecting the other one refreshes both lists; the lists never touch the map or Undo.
void i58Report(std::ofstream& report) {
    check(g_gui.NewMap(), "I58 target map"); auto& target = *g_gui.GetCurrentEditor();
    auto* d = naturalGenerator("4243");
    field<wxSpinCtrl>(*d, "terrain-candidate-count").SetValue(2);
    d->Show();
    click(*d, "terrain-preview"); until([&] { return field<wxButton>(*d, "terrain-preview").IsEnabled(); });
    auto& list = field<wxListCtrl>(*d, "terrain-candidates");
    check(list.GetItemCount() == 2, "I58 gallery of two: " + statusText(*d));
    reportConsistent(*d, "I58 first");
    const auto first = rows(*d, "terrain-statistics");
    const long other = list.GetItemText(0, 0).Contains("Selected") ? 1 : 0;
    list.SetItemState(other, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    const std::string expected = "Selected #" + std::to_string(other) + ";";
    until([&] { return statusText(*d).starts_with(expected) && field<wxButton>(*d, "terrain-preview").IsEnabled(); });
    reportConsistent(*d, "I58 second");
    check(rows(*d, "terrain-statistics") != first, "I58 statistics not refreshed for the other candidate");
    check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0, "I58 report modified map or Undo");
    d->Destroy(); g_gui.CloseCurrentEditor();
    report << "I58_SELECTION_REFRESH_NO_MUTATION_PASS\n";
}
// I59: the value of a generator input as the preset stores it.
std::string valueOf(wxWindow& d, const char* name) {
    auto* w = wxWindow::FindWindowByName(name, &d);
    check(w != nullptr, std::string("Missing field: ") + name);
    if (auto* spin = dynamic_cast<wxSpinCtrl*>(w)) return std::to_string(spin->GetValue());
    if (auto* box = dynamic_cast<wxCheckBox*>(w)) return box->GetValue() ? "1" : "0";
    if (auto* combo = dynamic_cast<wxComboBox*>(w)) return combo->GetValue().utf8_string(); // a wxChoice on wxMSW
    if (auto* choice = dynamic_cast<wxChoice*>(w)) return choice->GetStringSelection().utf8_string();
    if (auto* text = dynamic_cast<wxTextCtrl*>(w)) return text->GetValue().utf8_string();
    throw std::runtime_error(std::string("Unsupported field: ") + name);
}
// Settings on four tabs plus the area position, which a preset never holds.
constexpr const char* i59Fields[] {"terrain-seed", "terrain-width", "terrain-path-length", "terrain-grass-brush", "terrain-vegetation", "terrain-style"};
std::vector<std::string> i59Values(wxWindow& d) {
    std::vector<std::string> values;
    for (const char* name : i59Fields) values.push_back(valueOf(d, name));
    values.push_back(valueOf(d, "terrain-x"));
    return values;
}
void i59Set(wxWindow& d, const char* seed, int width, int path, const char* grass, bool vegetation, int style, int x) {
    field<wxTextCtrl>(d, "terrain-seed").SetValue(seed);
    field<wxSpinCtrl>(d, "terrain-width").SetValue(width);
    field<wxSpinCtrl>(d, "terrain-path-length").SetValue(path);
    auto& brush = field<wxComboBox>(d, "terrain-grass-brush"); brush.Clear(); brush.Append(grass); brush.SetSelection(0); changed(brush, wxEVT_COMBOBOX);
    field<wxCheckBox>(d, "terrain-vegetation").SetValue(vegetation);
    auto& styles = field<wxChoice>(d, "terrain-style"); styles.SetSelection(style); changed(styles);
    field<wxSpinCtrl>(d, "terrain-x").SetValue(x);
}
// Answers "terrain-confirm" questions while it lives, before they are shown (wxModalDialogHook); other dialogs open normally.
class Answerer final : public wxModalDialogHook {
public:
    Answerer() { Register(); }
    ~Answerer() override { Unregister(); }
    void reply(int id) { id_ = id; asked = false; }
    bool asked = false;
protected:
    int Enter(wxDialog* dialog) override {
        if (dialog->GetName() != "terrain-confirm") return wxID_NONE;
        asked = true;
        return id_;
    }
    void Exit(wxDialog*) override {}
private:
    int id_ = wxID_NO;
};
std::string fileText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
// I59: save, change, load; refused presets leave every setting; overwrite and delete ask; Random seed starts nothing.
void i59Presets(std::ofstream& report, const std::filesystem::path& dir) {
    const auto presets = dir / "presets";
    std::filesystem::create_directories(presets);
    std::filesystem::remove(presets / "i59-fixture.preset");
    { std::ofstream(presets / "i59-missing-brush.preset", std::ios::binary) << "terrain-preset/1\nterrain-grass-brush\tno such brush i59\nterrain-seed\t5\n"; }
    { std::ofstream(presets / "i59-range.preset", std::ios::binary) << "terrain-preset/1\nterrain-seed\t6\nterrain-width\t999999\n"; }
    { std::ofstream(presets / "i59-unknown.preset", std::ios::binary) << "terrain-preset/1\nterrain-no-such-control\t1\n"; }
    check(g_gui.NewMap(), "I59 map"); auto& target = *g_gui.GetCurrentEditor();
    auto* d = new TerrainGeneratorDialog(g_gui.root); d->Show();
    auto& choice = field<wxChoice>(*d, "terrain-preset");
    check(choice.FindString("i59-range") != wxNOT_FOUND && choice.FindString("i59-missing-brush") != wxNOT_FOUND, "I59 presets not listed");

    i59Set(*d, "777", 77, 33, "sand", true, 1, 400);
    const auto saved = i59Values(*d);
    field<wxTextCtrl>(*d, "terrain-preset-name").SetValue("i59-fixture");
    Answerer answerer;
    answerer.reply(wxID_NO); click(*d, "terrain-preset-save"); settle();
    check(!answerer.asked && std::filesystem::exists(presets / "i59-fixture.preset"), "I59 new preset not saved or asked: " + statusText(*d));
    check(choice.GetStringSelection() == "i59-fixture", "I59 saved preset not selected");
    const auto file = fileText(presets / "i59-fixture.preset");
    check(file.find("terrain-x\t") == std::string::npos && file.find("terrain-copy-index") == std::string::npos &&
        file.find("terrain-seed\t777\n") != std::string::npos, "I59 preset content: " + file);

    i59Set(*d, "123", 120, 5, "grass", false, 0, 555);
    choice.SetSelection(0); check(choice.SetStringSelection("i59-fixture"), "I59 choose preset"); changed(choice);
    auto loaded = i59Values(*d);
    check(loaded.back() == "555", "I59 preset changed the area position");
    loaded.back() = saved.back();
    check(loaded == saved, "I59 loaded settings differ from the saved ones: " + statusText(*d));
    report << "I59_SAVE_LOAD_PASS\n";

    const auto before = i59Values(*d);
    for (const char* bad : {"i59-missing-brush", "i59-range", "i59-unknown"}) {
        check(choice.SetStringSelection(bad), "I59 choose refused preset"); changed(choice);
        check(i59Values(*d) == before, std::string("I59 refused preset changed settings: ") + bad);
        check(statusText(*d).find("not loaded") != std::string::npos && choice.GetSelection() == 0,
            std::string("I59 refusal not reported: ") + bad + " " + statusText(*d));
    }
    report << "I59_REFUSED_PRESET_KEEPS_SETTINGS_PASS\n";

    field<wxTextCtrl>(*d, "terrain-seed").SetValue("888");
    answerer.reply(wxID_NO); click(*d, "terrain-preset-save"); settle();
    check(answerer.asked && fileText(presets / "i59-fixture.preset") == file, "I59 overwrite without asking or after No");
    answerer.reply(wxID_YES); click(*d, "terrain-preset-save"); settle();
    check(answerer.asked && fileText(presets / "i59-fixture.preset").find("terrain-seed\t888\n") != std::string::npos, "I59 overwrite after Yes");
    check(choice.SetStringSelection("i59-fixture"), "I59 choose for delete"); changed(choice);
    answerer.reply(wxID_NO); click(*d, "terrain-preset-delete"); settle();
    check(answerer.asked && std::filesystem::exists(presets / "i59-fixture.preset"), "I59 delete without asking or after No");
    answerer.reply(wxID_YES); click(*d, "terrain-preset-delete"); settle();
    check(answerer.asked && !std::filesystem::exists(presets / "i59-fixture.preset") && choice.FindString("i59-fixture") == wxNOT_FOUND, "I59 delete after Yes");
    report << "I59_OVERWRITE_DELETE_ASK_PASS\n";

    const auto seed = valueOf(*d, "terrain-seed");
    click(*d, "terrain-seed-random"); settle();
    const auto random = valueOf(*d, "terrain-seed");
    uint64_t parsed = 0;
    const auto [end, error] = std::from_chars(random.data(), random.data() + random.size(), parsed);
    check(random != seed && error == std::errc() && end == random.data() + random.size(), "I59 random seed: " + random);
    check(field<wxListCtrl>(*d, "terrain-candidates").GetItemCount() == 0 && !field<wxButton>(*d, "terrain-place").IsEnabled(),
        "I59 random seed started generation");
    check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0, "I59 presets modified map or Undo");
    d->Destroy(); g_gui.CloseCurrentEditor();
    for (const char* bad : {"i59-missing-brush", "i59-range", "i59-unknown"}) std::filesystem::remove(presets / (std::string(bad) + ".preset"));
    report << "I59_RANDOM_SEED_NO_MUTATION_PASS\n";
}
// I60: the preview's view label, as {zoom, x0, x1, y0, y1}.
struct View { unsigned zoom = 0, x0 = 0, x1 = 0, y0 = 0, y1 = 0; };
View viewOf(wxWindow& d) {
    const auto text = field<wxStaticText>(d, "terrain-preview-view").GetLabel().utf8_string();
    View v;
    check(std::sscanf(text.c_str(), "Zoom %ux; cells %u-%u, %u-%u", &v.zoom, &v.x0, &v.x1, &v.y0, &v.y1) == 5, "I60 view label: " + text);
    return v;
}
void wheel(wxWindow& panel, int x, int y, int rotation) {
    wxMouseEvent e(wxEVT_MOUSEWHEEL);
    e.SetEventObject(&panel); e.SetPosition(wxPoint(x, y));
    e.m_wheelRotation = rotation; e.m_wheelDelta = 120; e.m_linesPerAction = 3;
    panel.GetEventHandler()->ProcessEvent(e);
}
void key(wxWindow& panel, int code) {
    wxKeyEvent e(wxEVT_CHAR);
    e.SetEventObject(&panel); e.m_keyCode = code;
    panel.GetEventHandler()->ProcessEvent(e);
}
// I60: wheel, keys and fit change the view; comparing keeps the Selected candidate, its statistics and Place;
// another candidate of the same size keeps the view; map and Undo stay unchanged.
void i60Navigation(std::ofstream& report) {
    check(g_gui.NewMap(), "I60 map"); auto& target = *g_gui.GetCurrentEditor();
    auto* d = naturalGenerator("4243");
    field<wxSpinCtrl>(*d, "terrain-candidate-count").SetValue(2);
    d->Show();
    click(*d, "terrain-preview"); until([&] { return field<wxButton>(*d, "terrain-preview").IsEnabled(); });
    auto& list = field<wxListCtrl>(*d, "terrain-candidates");
    check(list.GetItemCount() == 2, "I60 gallery of two: " + statusText(*d));
    auto& panel = field<wxPanel>(*d, "terrain-preview-image");
    auto view = viewOf(*d);
    check(view.zoom == 1 && view.x0 == 0 && view.x1 == 127 && view.y0 == 0 && view.y1 == 127, "I60 initial view is not the whole area");

    const wxSize size = panel.GetClientSize();
    wheel(panel, size.GetWidth() / 4, size.GetHeight() / 4, 120);
    view = viewOf(*d);
    const auto text = field<wxStaticText>(*d, "terrain-preview-view").GetLabel().utf8_string();
    unsigned hx = 0, hy = 0;
    const auto under = text.find("Under the cursor: ");
    check(under != std::string::npos && std::sscanf(text.c_str() + under, "Under the cursor: %u,%u", &hx, &hy) == 2, "I60 cursor cell: " + text);
    check(view.zoom == 2 && (view.x1 - view.x0) * (view.y1 - view.y0) < 127u * 127u && view.x0 <= hx - 100 && hx - 100 <= view.x1 && view.y0 <= hy - 100 && hy - 100 <= view.y1,
        "I60 wheel zoom does not show the cell under the cursor: " + text);
    key(panel, '+');
    const auto zoomed = viewOf(*d);
    check(zoomed.zoom == 4 && (zoomed.x1 - zoomed.x0) * (zoomed.y1 - zoomed.y0) < (view.x1 - view.x0) * (view.y1 - view.y0), "I60 + did not zoom in");
    key(panel, WXK_LEFT); key(panel, WXK_LEFT);
    const auto left = viewOf(*d);
    key(panel, WXK_RIGHT); key(panel, WXK_RIGHT); key(panel, WXK_RIGHT);
    const auto right = viewOf(*d);
    check(right.x0 > left.x0 && right.zoom == 4 && right.y0 == left.y0, "I60 arrows did not pan");
    key(panel, '0');
    view = viewOf(*d);
    check(view.zoom == 1 && view.x0 == 0 && view.x1 == 127 && view.y1 == 127, "I60 0 did not fit");
    key(panel, '+');
    report << "I60_ZOOM_PAN_FIT_PASS\n";

    const auto statistics = rows(*d, "terrain-statistics");
    const bool placeable = field<wxButton>(*d, "terrain-place").IsEnabled();
    const auto selectedText = statusText(*d).substr(0, 12);
    auto& compare = field<wxChoice>(*d, "terrain-compare");
    check(compare.IsEnabled() && compare.GetCount() == 2, "I60 compare offers the other candidate");
    compare.SetSelection(1); changed(compare);
    const auto label = field<wxStaticText>(*d, "terrain-preview-view").GetLabel().utf8_string();
    const long other = list.GetItemText(0, 0).Contains("Selected") ? 1 : 0;
    check(label.find(std::format("with #{} (right)", other)) != std::string::npos, "I60 comparison not reported: " + label);
    check(viewOf(*d).zoom == 2, "I60 comparison changed the zoom");
    check(rows(*d, "terrain-statistics") == statistics && field<wxButton>(*d, "terrain-place").IsEnabled() == placeable &&
        statusText(*d).substr(0, 12) == selectedText, "I60 comparison changed the selection, statistics or Place");
    report << "I60_COMPARE_KEEPS_SELECTION_PASS\n";

    list.SetItemState(other, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    const std::string expected = "Selected #" + std::to_string(other) + ";";
    until([&] { return statusText(*d).starts_with(expected) && field<wxButton>(*d, "terrain-preview").IsEnabled(); });
    check(viewOf(*d).zoom == 2, "I60 another candidate of the same size reset the view");
    check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0, "I60 navigation modified map or Undo");
    d->Destroy(); g_gui.CloseCurrentEditor();
    report << "I60_VIEW_KEPT_NO_MUTATION_PASS\n";
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
        i58Report(report);
        i59Presets(report, dir);
        i60Navigation(report);
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
