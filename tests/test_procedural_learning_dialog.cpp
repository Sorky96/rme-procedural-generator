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
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/terrain_placement.h"
#include "brushes/managers/brush_manager.h"
#include "game/creature.h"
#include "game/house.h"
#include "game/spawn.h"
#include "game/waypoints.h"
#include "ui/main_frame.h"
#include "procedural/core/terrain_preset.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/listbox.h>
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
// I62: two rectangles of a varied map (and a third added then removed) learn one profile with two sources and bands;
// the generator recommends a candidate for it; learning leaves map and Undo unchanged; Place, Undo, Redo.
void i62Samples(std::ofstream& report, const std::filesystem::path& dir) {
    check(g_gui.NewMap(), "I62 source map"); auto& source = *g_gui.GetCurrentEditor();
    auto* fixture = naturalGenerator("1729"); fixture->Show(); preview(*fixture); place(*fixture); fixture->Destroy();
    const auto sourceHash = contents(source.map), undoSize = source.actionQueue->getSize();
    auto* generator = new TerrainGeneratorDialog(g_gui.root); generator->Show();
    std::exception_ptr learningError;
    wxTheApp->CallAfter([&] {
        auto* d = dynamic_cast<StyleLearningDialog*>(wxWindow::FindWindowByName("style-learning-dialog"));
        try {
            check(d != nullptr, "I62 learning entry");
            field<wxCheckBox>(*d, "learn-permission").SetValue(true);
            check(!field<wxButton>(*d, "learn-add-sample").IsEnabled(), "I62 Add sample before reading");
            const auto readAt = [&](int x, int y) {
                field<wxSpinCtrl>(*d, "learn-x").SetValue(x); field<wxSpinCtrl>(*d, "learn-y").SetValue(y);
                field<wxSpinCtrl>(*d, "learn-z").SetValue(7);
                field<wxSpinCtrl>(*d, "learn-width").SetValue(64); field<wxSpinCtrl>(*d, "learn-height").SetValue(64);
                click(*d, "learn-read"); until([&] { return field<wxButton>(*d, "learn-analyze").IsEnabled(); });
                click(*d, "learn-add-sample");
            };
            readAt(100, 100); readAt(164, 164); readAt(100, 164);
            auto& list = field<wxListBox>(*d, "learn-samples");
            check(list.GetCount() == 3, "I62 three samples listed: " + field<wxStaticText>(*d, "learn-status").GetLabel().ToStdString());
            list.SetSelection(2);
            wxCommandEvent select(wxEVT_LISTBOX, list.GetId()); select.SetEventObject(&list); select.SetInt(2);
            list.GetEventHandler()->ProcessEvent(select);
            click(*d, "learn-remove-sample");
            check(list.GetCount() == 2, "I62 sample removed");
            click(*d, "learn-analyze");
            until([&] { return field<wxButton>(*d, "learn-save").IsEnabled() || field<wxButton>(*d, "learn-analyze").IsEnabled(); });
            const auto status = field<wxStaticText>(*d, "learn-status").GetLabel().ToStdString();
            check(field<wxButton>(*d, "learn-save").IsEnabled() && status.find("from 2 samples") != std::string::npos, "I62 fit failed: " + status);
            field<wxCheckBox>(*d, "learn-protect").SetValue(true);
            field<wxTextCtrl>(*d, "learn-name").SetValue("i62-two"); click(*d, "learn-save");
            check(!std::filesystem::exists(dir / "i62-two.profile"), "I62 copy index of several samples saved something");
            field<wxCheckBox>(*d, "learn-protect").SetValue(false); click(*d, "learn-save");
            check(std::filesystem::exists(dir / "i62-two.profile"), "I62 save refused: " + field<wxStaticText>(*d, "learn-status").GetLabel().ToStdString());
        } catch (...) { learningError = std::current_exception(); if (d) d->EndModal(wxID_CANCEL); }
    });
    click(*generator, "terrain-learn");
    if (learningError) std::rethrow_exception(learningError);
    check(contents(source.map) == sourceHash && source.actionQueue->getSize() == undoSize, "I62 learning modified map or Undo");
    generator->Destroy(); source.map.clearChanges(); g_gui.CloseCurrentEditor();
    const auto profile = parseProfile(fileText(dir / "i62-two.profile"));
    check(profile && profile->sources.size() == 2 && profile->sources[0].alias == "sample-1" &&
        profile->sources[0].weightPermille + profile->sources[1].weightPermille == 1000 && !profile->bands.empty() &&
        std::ranges::none_of(profile->bands, [](const CandidateBand& b) { return !learnableCandidateMetric(b.metric); }),
        "I62 profile has two weighted sources and learned bands");
    report << "I62_SAMPLES_PROFILE_NO_MUTATION_PASS\n";

    check(g_gui.NewMap(), "I62 target map"); auto& target = *g_gui.GetCurrentEditor();
    auto* d = naturalGenerator("4243");
    auto& choice = field<wxChoice>(*d, "terrain-profile"); check(choice.SetStringSelection("i62-two"), "I62 profile offered"); changed(choice);
    field<wxSpinCtrl>(*d, "terrain-candidate-count").SetValue(4);
    d->Show();
    click(*d, "terrain-preview"); until([&] { return field<wxButton>(*d, "terrain-preview").IsEnabled(); });
    const auto status = statusText(*d);
    auto& list = field<wxListCtrl>(*d, "terrain-candidates");
    bool recommended = false;
    for (long i = 0; i < list.GetItemCount(); ++i) recommended |= list.GetItemText(i, 0).Contains("Recommended");
    check(list.GetItemCount() == 4 && (recommended || status.find("No admissible") != std::string::npos), "I62 no recommendation: " + status);
    check(status.find("learned from the profile") != std::string::npos || hasRow(rows(*d, "terrain-statistics"), "Ranking"),
        "I62 learned policy not named: " + status);
    check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0, "I62 preview modified target");
    report << "I62_GENERATOR_RECOMMENDS_PASS\n";
    if (field<wxButton>(*d, "terrain-place").IsEnabled()) {
        place(*d); check(target.actionQueue->getSize() == 1, "I62 Place one Undo");
        const auto placed = contents(target.map);
        check(target.actionQueue->undo() && target.map.getTileCount() == 0, "I62 Undo");
        check(target.actionQueue->redo() && contents(target.map) == placed, "I62 Redo exact payload");
        report << "I62_PLACE_UNDO_REDO_PASS\n";
    } else report << "I62_PLACE_SKIPPED selected candidate blocked\n";
    d->Destroy(); target.map.clearChanges(); g_gui.CloseCurrentEditor();
}
// I63: a house, a spawn, a waypoint, a zone tile and an item with an action id inside the area. Margin 0 reports them and
// keeps them; margin 3 also keeps every cell within 3 of them empty; one Undo restores, Redo reproduces; a preset keeps it.
std::string tileImage(Map& map, const Position& at) {
    const Tile* tile = map.getTile(at);
    if (!tile) return "-";
    std::string text = std::format("g{} h{} s{} z{}", tile->ground ? tile->ground->getID() : 0, tile->house_id, tile->spawn ? 1 : 0, tile->getZoneIds().size());
    if (tile->ground) text += std::format(" a{}", tile->ground->getActionID());
    for (const auto& item : tile->items) text += std::format(" i{}", item->getID());
    return text;
}
void i63Protection(std::ofstream& report, const std::filesystem::path& dir) {
    check(g_gui.NewMap(), "I63 map"); auto& target = *g_gui.GetCurrentEditor();
    auto& map = target.map;
    check(map.convert({MAP_OTBM_5, map.getVersion().client}), "I63 OTBM 5 map");
    const auto grass = g_brushes.getBrush("grass")->as<GroundBrush>()->getPlacementItemId();
    std::vector<Position> fixture;
    for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) fixture.emplace_back(140 + dx, 140 + dy, 7);
    const Position spawnAt(190, 180, 7), waypointAt(160, 200, 7), zoneAt(200, 130, 7), actionAt(130, 205, 7);
    fixture.insert(fixture.end(), {spawnAt, waypointAt, zoneAt, actionAt});
    for (const auto& at : fixture) TerrainPlacement::placeBrushItem(*map.getOrCreateTile(at), grass);
    auto house = std::make_unique<House>(map);
    house->setID(1); house->name = "I63 House"; house->setExit(Position(140, 140, 7));
    auto* houseRef = house.get();
    check(map.houses.addHouse(std::move(house)), "I63 house");
    for (size_t i = 0; i < 4; ++i) map.getTile(fixture[i])->setHouse(houseRef);
    const auto zone = map.addZone("I63 Zone"); check(zone != 0, "I63 zone");
    map.getTile(zoneAt)->addZone(zone);
    map.getTile(actionAt)->ground->setActionID(31063);
    auto* spawnTile = map.getTile(spawnAt);
    spawnTile->spawn = std::make_unique<Spawn>(2); spawnTile->spawn->addKind(Spawn::MONSTERS);
    spawnTile->creature = std::make_unique<Creature>("Rat");
    check(map.addSpawn(spawnTile), "I63 spawn");
    map.waypoints.addWaypoint(std::make_unique<Waypoint>("I63 Waypoint", waypointAt));
    map.rebuildZoneIndex();
    std::vector<std::string> before;
    for (const auto& at : fixture) before.push_back(tileImage(map, at));
    const auto fixtureKept = [&](const char* where) {
        for (size_t i = 0; i < fixture.size(); ++i) check(tileImage(map, fixture[i]) == before[i], std::string("I63 fixture tile changed ") + where);
    };

    auto* d = naturalGenerator("4243");
    d->Show();
    // After Place and Undo the first Preview may only report the changed map; one more starts the series.
    const auto runPreview = [&] {
        for (int attempt = 0; attempt < 2; ++attempt) {
            click(*d, "terrain-preview"); until([&] { return field<wxButton>(*d, "terrain-preview").IsEnabled(); });
            if (statusText(*d).find("data changed") == std::string::npos) return;
        }
    };
    runPreview();
    const auto stats = rows(*d, "terrain-statistics");
    const auto value = [&](const std::vector<std::pair<std::string, std::string>>& list, std::string_view name) {
        const auto found = std::ranges::find(list, name, &std::pair<std::string, std::string>::first);
        return found == list.end() ? std::string() : found->second;
    };
    // The spawn's radius 2 protects its 25 cells: 4 house + 3 single + 25 spawn-area cells.
    check(value(stats, "Existing tiles kept") == "32" && value(stats, "House tiles kept") == "4" && value(stats, "House exits kept") == "1" &&
        value(stats, "Spawns kept") == "1" && value(stats, "Cells of spawn areas kept") == "25" && value(stats, "Waypoints kept") == "1" && value(stats, "Tiles with zones kept") == "1" &&
        value(stats, "Items with unique or action id kept") == "1" && value(stats, "Kept clear by the margin").empty(),
        "I63 preserved counts: " + value(stats, "Existing tiles kept") + "/" + value(stats, "House tiles kept") + "/" + value(stats, "Spawns kept"));
    check(hasRow(rows(*d, "terrain-warnings"), "Warning", "touches") || hasRow(rows(*d, "terrain-warnings"), "No warnings") ||
        !rows(*d, "terrain-warnings").empty(), "I63 warnings list");
    check(map.getTileCount() == fixture.size() && target.actionQueue->getSize() == 0, "I63 preview modified map");
    report << "I63_REPORT_MARGIN0_PASS\n";
    if (field<wxButton>(*d, "terrain-place").IsEnabled()) {
        place(*d); fixtureKept("by Place with margin 0");
        check(target.actionQueue->undo() && map.getTileCount() == fixture.size(), "I63 Undo margin 0");
        fixtureKept("by Undo");
    }

    auto& margin = field<wxSpinCtrl>(*d, "terrain-protection-margin");
    margin.SetValue(3); changed(margin, wxEVT_SPINCTRL);
    runPreview();
    const auto marginStats = rows(*d, "terrain-statistics");
    check(value(marginStats, "Kept clear by the margin").find("(3 tiles") != std::string::npos, "I63 margin row: " + [&] {
        std::string all = std::format("spin {} status {} rows:", margin.GetValue(), statusText(*d));
        for (const auto& [k, v] : marginStats) all += " " + k + "=" + v + ";";
        return all; }());
    check(field<wxButton>(*d, "terrain-place").IsEnabled(), "I63 margin 3 candidate blocked: " + statusText(*d));
    const auto undoBefore = target.actionQueue->getCurrentIndex(); // an earlier Undo leaves a redo entry that Place replaces
    place(*d); fixtureKept("by Place with margin 3");
    for (const auto& at : fixture)
        for (int dy = -3; dy <= 3; ++dy) for (int dx = -3; dx <= 3; ++dx) {
            const Position cell(at.x + dx, at.y + dy, 7);
            if (std::ranges::find(fixture, cell) != fixture.end()) continue;
            check(tileImage(map, cell) == "-", std::format("I63 margin cell {},{} written", cell.x, cell.y));
        }
    const auto placed = contents(map);
    check(target.actionQueue->getCurrentIndex() == undoBefore + 1 && target.actionQueue->getSize() == undoBefore + 1, "I63 margin Place one Undo");
    check(target.actionQueue->undo() && map.getTileCount() == fixture.size(), "I63 margin Undo");
    fixtureKept("by margin Undo");
    check(target.actionQueue->redo() && contents(map) == placed, "I63 margin Redo exact payload");
    report << "I63_MARGIN_KEEPS_CLEAR_UNDO_REDO_PASS\n";

    const auto presets = dir / "presets";
    std::filesystem::remove(presets / "i63-margin.preset");
    field<wxTextCtrl>(*d, "terrain-preset-name").SetValue("i63-margin");
    click(*d, "terrain-preset-save"); settle();
    check(fileText(presets / "i63-margin.preset").find("terrain-protection-margin\t3\n") != std::string::npos, "I63 preset lacks the margin");
    margin.SetValue(0); changed(margin, wxEVT_SPINCTRL);
    auto& choice = field<wxChoice>(*d, "terrain-preset");
    check(choice.SetStringSelection("i63-margin"), "I63 preset listed"); changed(choice); settle();
    check(margin.GetValue() == 3, "I63 preset did not restore the margin");
    d->Destroy(); map.clearChanges(); g_gui.CloseCurrentEditor();
    report << "I63_PRESET_MARGIN_PASS\n";
}
// I64: the tabs of section 6, the preset keys of the I63 layout, a biome that fills the terrain fields, a preset load that
// resets the biome, and Preview/Place with a biome.
void i64Tabs(std::ofstream& report, const std::filesystem::path& dir) {
    check(g_gui.NewMap(), "I64 map"); auto& target = *g_gui.GetCurrentEditor();
    auto* d = naturalGenerator("4244");
    d->Show();
    auto& tabs = field<wxNotebook>(*d, "terrain-sections");
    std::vector<std::string> titles;
    for (size_t i = 0; i < tabs.GetPageCount(); ++i) titles.push_back(tabs.GetPageText(i).ToStdString());
    check(titles == std::vector<std::string> {"Area", "Terrain", "Biomes", "Structures", "Decoration", "Reference Style", "Advanced", "Validation"},
        "I64 tab titles");
    report << "I64_TABS_PASS\n";

    const auto presets = dir / "presets";
    std::filesystem::remove(presets / "i64-keys.preset");
    field<wxTextCtrl>(*d, "terrain-preset-name").SetValue("i64-keys");
    click(*d, "terrain-preset-save"); settle();
    const auto saved = parsePreset(fileText(presets / "i64-keys.preset"));
    check(saved.has_value(), "I64 preset saved");
    std::vector<std::string> keys;
    for (const auto& [key, value] : saved->values) keys.push_back(key);
    const std::vector<std::string> i63Keys {
        "terrain-bridge-brush",
        "terrain-building-density",
        "terrain-building-dungeons",
        "terrain-building-stairs",
        "terrain-building-storeys",
        "terrain-bush-brush",
        "terrain-camp-count",
        "terrain-campfire-brush",
        "terrain-candidate-count",
        "terrain-cave-door-brush",
        "terrain-cave-door-corridors",
        "terrain-cave-entrance-brush",
        "terrain-cave-entrance-style",
        "terrain-cave-entrances",
        "terrain-cave-exit-brush",
        "terrain-cave-far-exits",
        "terrain-cave-ground-brush",
        "terrain-cave-wall-brush",
        "terrain-dirt-brush",
        "terrain-dirt-weight",
        "terrain-door-type",
        "terrain-doors",
        "terrain-dungeon-descent",
        "terrain-dungeon-floor",
        "terrain-dungeon-percent",
        "terrain-dungeon-stairs",
        "terrain-dungeon-wall",
        "terrain-floor-brush",
        "terrain-ford-length",
        "terrain-grass-brush",
        "terrain-grass-weight",
        "terrain-height",
        "terrain-mountain-passes",
        "terrain-path-brush",
        "terrain-path-length",
        "terrain-plateau-cliff",
        "terrain-plateau-ground",
        "terrain-plateau-levels",
        "terrain-plateau-percent",
        "terrain-plateau-ramp",
        "terrain-profile",
        "terrain-protection-margin",
        "terrain-repair",
        "terrain-retry-seeds",
        "terrain-rock-brush",
        "terrain-rock-passes",
        "terrain-ruin-percent",
        "terrain-seed",
        "terrain-shore-brush",
        "terrain-smooth-beaches",
        "terrain-stone-brush",
        "terrain-style",
        "terrain-tree-brush",
        "terrain-upper-floor-percent",
        "terrain-vegetation",
        "terrain-wall-brush",
        "terrain-walls",
        "terrain-water-brush",
        "terrain-water-weight",
        "terrain-width"
    };
    auto expectedKeys = i63Keys; // I68 adds the secondary biome and its share
    expectedKeys.insert(expectedKeys.end(), {"terrain-secondary-biome", "terrain-secondary-share"});
    std::ranges::sort(expectedKeys);
    check(keys == expectedKeys, std::format("I64 preset keys differ from I63 + I68 ({} vs {})", keys.size(), expectedKeys.size()));
    report << "I64_PRESET_KEYS_PASS\n";

    auto& biome = field<wxChoice>(*d, "terrain-biome");
    check(biome.SetStringSelection("Snow"), "I64 Snow listed"); changed(biome); settle();
    const auto status = statusText(*d);
    check(status.find("Biome Snow applied") != std::string::npos, "I64 biome status: " + status);
    check(field<wxSpinCtrl>(*d, "terrain-grass-weight").GetValue() == 3 && field<wxSpinCtrl>(*d, "terrain-dirt-weight").GetValue() == 2 &&
        field<wxSpinCtrl>(*d, "terrain-water-weight").GetValue() == 1 && field<wxCheckBox>(*d, "terrain-vegetation").GetValue(), "I64 biome weights");
    const auto& snow = std::ranges::find(biomePresets(), std::string_view("Snow"), &BiomePreset::name)->values.values;
    for (const auto& [key, value] : snow) {
        if (!key.ends_with("-brush")) continue;
        const auto shown = field<wxComboBox>(*d, key.c_str()).GetValue().ToStdString();
        check(shown == value || status.find(value) != std::string::npos, std::format("I64 {} is {} and not reported", key, shown));
    }
    check(field<wxComboBox>(*d, "terrain-grass-brush").GetValue() == "snow", "I64 snow ground set");
    report << "I64_BIOME_APPLIES_PASS " << status << "\n";

    // Every biome can be placed in this client and Preview leaves the map alone; the first one is placed with one Undo.
    bool placed = false; size_t refused = 0;
    for (const auto& entry : biomePresets()) {
        check(biome.SetStringSelection(wxString::FromUTF8(entry.name.data(), entry.name.size())), "I64 biome listed"); changed(biome); settle();
        const auto applied = statusText(*d);
        const auto queued = target.actionQueue->getSize();
        click(*d, "terrain-preview"); until([&] { return field<wxButton>(*d, "terrain-preview").IsEnabled(); });
        check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == queued, "I64 preview modified map");
        const bool placeable = field<wxButton>(*d, "terrain-place").IsEnabled();
        report << "I64_BIOME " << entry.name << (placeable ? " placeable" : " refused: " + statusText(*d)) << " | " << applied << "\n";
        refused += placeable ? 0 : 1;
        if (placeable && !placed) {
            place(*d); check(target.actionQueue->getSize() == queued + 1, "I64 Place one Undo");
            check(target.actionQueue->undo() && target.map.getTileCount() == 0, "I64 Undo");
            placed = true;
        }
    }
    check(placed && refused == 0, std::format("I64 {} biome(s) refused in this client", refused));
    report << "I64_BIOME_PLACE_PASS\n";

    auto& choice = field<wxChoice>(*d, "terrain-preset");
    check(choice.SetStringSelection("i64-keys"), "I64 preset listed"); changed(choice); settle();
    check(biome.GetSelection() == 0 && field<wxComboBox>(*d, "terrain-grass-brush").GetValue() == "grass", "I64 preset load resets the biome");
    report << "I64_PRESET_RESETS_BIOME_PASS\n";
    d->Destroy(); target.map.clearChanges(); g_gui.CloseCurrentEditor();
}
// I65: Preview with nothing changed keeps the finished series; Place and Undo still work; a map change (Undo) or a
// changed seed generates again.
std::vector<std::string> gallery(wxWindow& d) {
    auto& list = field<wxListCtrl>(d, "terrain-candidates");
    std::vector<std::string> out;
    for (long i = 0; i < list.GetItemCount(); ++i) out.push_back((list.GetItemText(i, 0) + "|" + list.GetItemText(i, 1)).ToStdString());
    return out;
}
void i65Reuse(std::ofstream& report) {
    check(g_gui.NewMap(), "I65 map"); auto& target = *g_gui.GetCurrentEditor();
    auto* d = naturalGenerator("4245");
    field<wxSpinCtrl>(*d, "terrain-candidate-count").SetValue(2);
    d->Show();
    preview(*d);
    const auto first = gallery(*d);
    check(first.size() == 2 && statusText(*d).find("is kept") == std::string::npos, "I65 first series");
    const auto started = std::chrono::steady_clock::now();
    click(*d, "terrain-preview");
    const auto took = std::chrono::steady_clock::now() - started;
    check(statusText(*d).starts_with("Settings, map and client unchanged") && gallery(*d) == first &&
        field<wxButton>(*d, "terrain-preview").IsEnabled() && field<wxButton>(*d, "terrain-place").IsEnabled(), "I65 series kept: " + statusText(*d));
    check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0, "I65 reuse modified the map");
    report << std::format("I65_REUSE_PASS {} ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(took).count());

    place(*d); check(target.actionQueue->getSize() == 1, "I65 Place one Undo");
    check(target.actionQueue->undo() && target.map.getTileCount() == 0, "I65 Undo");
    d->Show(); preview(*d);
    check(statusText(*d).find("is kept") == std::string::npos && gallery(*d) == first, "I65 after Undo not generated: " + statusText(*d));
    report << "I65_MAP_CHANGE_GENERATES_PASS\n";

    field<wxTextCtrl>(*d, "terrain-seed").SetValue("4246");
    preview(*d);
    check(statusText(*d).find("is kept") == std::string::npos && gallery(*d) != first, "I65 seed change not generated: " + statusText(*d));
    report << "I65_SEED_CHANGE_GENERATES_PASS\n";
    d->Destroy(); target.map.clearChanges(); g_gui.CloseCurrentEditor();
}
// I66: zoomed in, the pane shows sprites (its pixels differ from minimap colours); at fit level it does not.
wxImage capture(wxPanel& panel) {
    panel.Refresh(); panel.Update();
    const wxSize size = panel.GetClientSize();
    wxBitmap bitmap(size);
    wxMemoryDC memory(bitmap);
    wxClientDC screen(&panel);
    memory.Blit(0, 0, size.GetWidth(), size.GetHeight(), &screen, 0, 0);
    memory.SelectObject(wxNullBitmap);
    return bitmap.ConvertToImage();
}
bool samePixels(const wxImage& a, const wxImage& b) {
    return a.GetSize() == b.GetSize() && std::equal(a.GetData(), a.GetData() + size_t(a.GetWidth()) * a.GetHeight() * 3, b.GetData());
}
void i66Sprites(std::ofstream& report) {
    check(g_gui.NewMap(), "I66 map"); auto& target = *g_gui.GetCurrentEditor();
    auto* d = naturalGenerator("4247");
    d->Show(); preview(*d);
    auto& panel = field<wxPanel>(*d, "terrain-preview-image");
    auto& sprites = field<wxCheckBox>(*d, "terrain-preview-sprites");
    check(sprites.GetValue(), "I66 sprites on by default");
    const auto fitOn = capture(panel);
    sprites.SetValue(false); changed(sprites, wxEVT_CHECKBOX);
    check(samePixels(fitOn, capture(panel)), "I66 fit level differs with sprites");
    for (int i = 0; i < 6; ++i) key(panel, '+');
    const auto zoomedOff = capture(panel);
    sprites.SetValue(true); changed(sprites, wxEVT_CHECKBOX);
    const auto started = std::chrono::steady_clock::now();
    const auto zoomedOn = capture(panel);
    const auto took = std::chrono::steady_clock::now() - started;
    check(!samePixels(zoomedOff, zoomedOn), "I66 zoomed pane shows no sprites");
    check(field<wxButton>(*d, "terrain-place").IsEnabled() && target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0,
        "I66 painting changed the preview or the map: " + statusText(*d));
    report << std::format("I66_SPRITES_PASS paint {} ms, view {}\n", std::chrono::duration_cast<std::chrono::milliseconds>(took).count(),
        field<wxStaticText>(*d, "terrain-preview-view").GetLabel().utf8_string());
    place(*d); check(target.actionQueue->getSize() == 1, "I66 Place one Undo");
    report << "I66_PLACE_PASS\n";
    d->Destroy(); target.map.clearChanges(); g_gui.CloseCurrentEditor();
}
// I67: each share field has a slider that edits it, follows it and shares its enabled state; presets keep 60 keys.
void i67Sliders(std::ofstream& report, const std::filesystem::path& dir) {
    check(g_gui.NewMap(), "I67 map"); auto& target = *g_gui.GetCurrentEditor();
    auto* d = naturalGenerator("4248");
    d->Show(); preview(*d);
    bool released = false;
    for (const char* name : {"terrain-building-density", "terrain-ruin-percent", "terrain-upper-floor-percent", "terrain-dungeon-percent", "terrain-plateau-percent"}) {
        auto& spin = field<wxSpinCtrl>(*d, name);
        auto& slider = field<wxSlider>(*d, (std::string(name) + "-slider").c_str());
        const int before = spin.GetValue(), moved = before == 37 ? 38 : 37;
        slider.SetValue(moved);
        wxCommandEvent event(wxEVT_SLIDER, slider.GetId()); event.SetEventObject(&slider); event.SetInt(moved);
        slider.ProcessWindowEvent(event); settle();
        check(spin.GetValue() == moved, std::format("I67 {} slider did not set the field", name));
        if (!released) {
            check(!field<wxButton>(*d, "terrain-place").IsEnabled() && field<wxListCtrl>(*d, "terrain-candidates").GetItemCount() == 0,
                std::format("I67 {} slider kept the preview", name));
            released = true;
        }
        spin.SetValue(before); d->UpdateWindowUI(wxUPDATE_UI_RECURSE);
        check(slider.GetValue() == before, std::format("I67 {} slider does not follow the field", name));
        check(slider.IsEnabled() == spin.IsEnabled(), std::format("I67 {} slider enabled state differs", name));
    }
    report << "I67_SLIDERS_PASS\n";
    const auto presets = dir / "presets";
    std::filesystem::remove(presets / "i67-keys.preset");
    field<wxTextCtrl>(*d, "terrain-preset-name").SetValue("i67-keys");
    click(*d, "terrain-preset-save"); settle();
    const auto saved = parsePreset(fileText(presets / "i67-keys.preset"));
    check(saved && saved->values.size() == 62 && std::ranges::none_of(saved->values, [](const auto& v) { return v.first.ends_with("-slider"); }),
        "I67 preset keys changed");
    report << "I67_PRESET_KEYS_PASS\n";
    check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0, "I67 map changed");
    d->Destroy(); target.map.clearChanges(); g_gui.CloseCurrentEditor();
}
// I68: every ordered pair of biomes as primary and secondary at 40 %; the secondary grass covers about its share of the
// grass; share 0 places the same terrain as None; presets hold the two settings and old presets load with None.
size_t groundCount(Map& map, std::string_view brush) {
    size_t count = 0;
    for (auto& loc : map.tiles()) if (const auto* t = loc.get(); t && t->ground) {
        const auto* ground = t->ground->getGroundBrush();
        count += ground && ground->getName() == brush;
    }
    return count;
}
void i68Secondary(std::ofstream& report, const std::filesystem::path& dir) {
    check(g_gui.NewMap(), "I68 map"); auto& target = *g_gui.GetCurrentEditor();
    auto* d = naturalGenerator("4249");
    d->Show();
    auto& primary = field<wxChoice>(*d, "terrain-biome");
    auto& secondary = field<wxChoice>(*d, "terrain-secondary-biome");
    auto& share = field<wxSpinCtrl>(*d, "terrain-secondary-share");
    check(secondary.GetStringSelection() == "None" && share.GetValue() == 30, "I68 defaults");
    const auto& biomes = biomePresets();
    size_t placed = 0, refused = 0;
    for (const auto& first : biomes) for (const auto& second : biomes) {
        if (first.name == second.name) continue;
        check(primary.SetStringSelection(wxstr(first.name)), "I68 primary listed"); changed(primary); settle();
        check(secondary.SetStringSelection(wxstr(second.name)), "I68 secondary listed"); changed(secondary);
        share.SetValue(40);
        click(*d, "terrain-preview"); until([&] { return field<wxButton>(*d, "terrain-preview").IsEnabled(); });
        const bool ok = field<wxButton>(*d, "terrain-place").IsEnabled();
        (ok ? placed : refused) += 1;
        report << std::format("I68_PAIR {} + {}: {}\n", first.name, second.name, ok ? "placeable" : "refused | " + statusText(*d));
        check(target.map.getTileCount() == 0 && target.actionQueue->getSize() == 0, "I68 preview modified the map");
        if (first.name == "Temperate") check(ok, std::format("I68 Temperate + {} refused: {}", second.name, statusText(*d)));
    }
    report << std::format("I68_PAIRS_PASS placeable {} refused {}\n", placed, refused);

    // Temperate with Snow regions: snow on about 40 % of the grass.
    check(primary.SetStringSelection("Temperate"), "I68 Temperate"); changed(primary); settle();
    check(secondary.SetStringSelection("Snow"), "I68 Snow"); changed(secondary); share.SetValue(40);
    preview(*d); place(*d);
    check(target.actionQueue->getSize() == 1, "I68 Place one Undo");
    const size_t snow = groundCount(target.map, "snow"), grass = groundCount(target.map, "grass");
    const double ratio = double(snow) / double(std::max<size_t>(1, snow + grass));
    check(ratio >= 0.30 && ratio <= 0.50, std::format("I68 snow share {:.3f} ({} snow, {} grass)", ratio, snow, grass));
    report << std::format("I68_SHARE_PASS snow {:.3f} of the grass ({} / {})\n", ratio, snow, snow + grass);
    check(target.actionQueue->undo() && target.map.getTileCount() == 0, "I68 Undo");

    d->Show(); share.SetValue(0); preview(*d); place(*d);
    const auto zero = contents(target.map);
    check(target.actionQueue->undo(), "I68 Undo share 0");
    d->Show(); check(secondary.SetStringSelection("None"), "I68 None"); changed(secondary); share.SetValue(30); preview(*d); place(*d);
    check(contents(target.map) == zero, "I68 share 0 differs from None");
    check(target.actionQueue->undo(), "I68 Undo None");
    report << "I68_SHARE_ZERO_IS_NONE_PASS\n";

    d->Show();
    check(secondary.SetStringSelection("Desert"), "I68 Desert"); changed(secondary); share.SetValue(25);
    const auto presets = dir / "presets";
    std::filesystem::remove(presets / "i68-mix.preset");
    field<wxTextCtrl>(*d, "terrain-preset-name").SetValue("i68-mix");
    click(*d, "terrain-preset-save"); settle();
    auto saved = parsePreset(fileText(presets / "i68-mix.preset"));
    check(saved && saved->values.at("terrain-secondary-biome") == "Desert" && saved->values.at("terrain-secondary-share") == "25", "I68 preset saved");
    saved->values.erase("terrain-secondary-biome"); saved->values.erase("terrain-secondary-share");
    std::filesystem::remove(presets / "i68-old.preset");
    check(saveTerrainPreset(presets, "i68-old", *saved, false).has_value(), "I68 old preset written");
    // A save through the dialog reloads its preset list, which then lists the file written above.
    std::filesystem::remove(presets / "i68-refresh.preset");
    field<wxTextCtrl>(*d, "terrain-preset-name").SetValue("i68-refresh");
    click(*d, "terrain-preset-save"); settle();
    auto& choice = field<wxChoice>(*d, "terrain-preset");
    check(choice.SetStringSelection("i68-old"), "I68 old preset listed"); changed(choice); settle();
    check(secondary.GetStringSelection() == "None" && share.GetValue() == 30, "I68 old preset did not load with None: " + statusText(*d));
    report << "I68_PRESETS_PASS\n";
    d->Destroy(); target.map.clearChanges(); g_gui.CloseCurrentEditor();
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
        check(!profile.bands.empty() && candidateBandsFor(profile).data() == profile.bands.data() && profile.sources[0].alias == "user-local-surface",
            "Learned bands, no invented M06/M19 provenance");
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
        i62Samples(report, dir);
        i63Protection(report, dir);
        i64Tabs(report, dir);
        i65Reuse(report);
        i66Sprites(report);
        i67Sliders(report, dir);
        i68Secondary(report, dir);
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
