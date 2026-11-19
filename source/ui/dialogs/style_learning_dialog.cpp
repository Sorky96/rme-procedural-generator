// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "ui/dialogs/style_learning_dialog.h"
#include "procedural/adapter/open_map_learning.h"
#include "procedural/adapter/generation_assets.h"
#include "app/managers/version_manager.h"
#include "app/client_version.h"
#include "editor/editor.h"
#include "ui/gui.h"
#include "ui/theme.h"
#include "util/common.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/gauge.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <chrono>
#include <format>

using namespace rme::procedural;
StyleLearningDialog::StyleLearningDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Learn from open map", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    SetName("style-learning-dialog");
    SetFont(Theme::GetFont()); SetBackgroundColour(Theme::Get(Theme::Role::Surface));
    SetForegroundColour(Theme::Get(Theme::Role::Text));
    const int gap = Theme::Grid(2), margin = Theme::Grid(4);
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* help = new wxStaticText(this, wxID_ANY,
        "Learn a surface style from a region of the current map.\n"
        "Read the region, review the ground classes, then analyze and save a profile.");
    root->Add(help, 0, wxEXPAND | wxALL, margin);
    auto* form = new wxFlexGridSizer(2, gap, gap); form->AddGrowableCol(1);
    auto spin = [&](const char* label, const char* name, int min, int max, int value) {
        form->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto* field = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, min, max, value);
        field->SetName(name); form->Add(field, 1, wxEXPAND);
        field->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { Invalidate(); });
        return field;
    };
    x_ = spin("&X:", "learn-x", 0, 65000, 100); y_ = spin("&Y:", "learn-y", 0, 65000, 100);
    z_ = spin("&Floor (0-7):", "learn-z", 0, 7, 7);
    width_ = spin("&Width (max 512):", "learn-width", 1, 512, 100);
    height_ = spin("&Height (max 512):", "learn-height", 1, 512, 100);
    root->Add(form, 0, wxEXPAND | wxLEFT | wxRIGHT, margin);
    permission_ = new wxCheckBox(this, wxID_ANY, "I have the &right to use this map for local style learning.");
    permission_->SetName("learn-permission"); root->Add(permission_, 0, wxALL, margin);
    permission_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { Invalidate(false); });
    root->Add(new wxStaticText(this, wxID_ANY, "Ground &mapping (edit the class column, keep the tabs):"), 0, wxLEFT | wxRIGHT, margin);
    mapping_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(Theme::Grid(100), Theme::Grid(28)), wxTE_MULTILINE | wxHSCROLL);
    mapping_->SetName("learn-mapping"); root->Add(mapping_, 1, wxEXPAND | wxALL, margin);
    mapping_->SetToolTip("G4 classes (case sensitive): Grass, Dirt, Rock, Shore, Water, Other, Unmapped, NoGround. Keep unknown grounds Unmapped. Edit the class column; keep the exact brush name.");
    mapping_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { Invalidate(false); });
    auto* nameRow = new wxBoxSizer(wxHORIZONTAL);
    nameRow->Add(new wxStaticText(this, wxID_ANY, "Profile &name:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    name_ = new wxTextCtrl(this, wxID_ANY); name_->SetName("learn-name");
    nameRow->Add(name_, 1); root->Add(nameRow, 0, wxEXPAND | wxLEFT | wxRIGHT, margin);
    root->Add(new wxStaticText(this, wxID_ANY, "Use 1-64 ASCII letters, digits, hyphens or underscores."), 0, wxALL, margin);
    progress_ = new wxGauge(this, wxID_ANY, 100); root->Add(progress_, 0, wxEXPAND | wxLEFT | wxRIGHT, margin);
    status_ = new wxStaticText(this, wxID_ANY, "Read the rectangle to review ground names. Data stays on this computer.");
    status_->SetName("learn-status"); root->Add(status_, 0, wxEXPAND | wxALL, margin);
    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto button = [&](const char* label, const char* name, auto handler) {
        auto* b = new wxButton(this, wxID_ANY, label); b->SetName(name);
        b->Bind(wxEVT_BUTTON, handler); buttons->Add(b, 0, wxRIGHT, gap); return b;
    };
    read_ = button("&Read rectangle", "learn-read", [this](wxCommandEvent&) { Read(); });
    analyze_ = button("&Analyze", "learn-analyze", [this](wxCommandEvent&) { Analyze(); }); analyze_->Disable();
    save_ = button("&Save profile", "learn-save", [this](wxCommandEvent&) { Save(); }); save_->Disable();
    cancel_ = button("&Stop", "learn-cancel", [this](wxCommandEvent&) {
        if (!reading_ && (!worker_.valid() || stop_.stop_requested())) return;
        const bool wasReading = reading_;
        Invalidate(wasReading);
        Status(wasReading ? "Reading stopped. Read the rectangle to start again."
                          : "Analysis stopped. Choose Analyze to try again.");
    });
    cancel_->Disable();
    auto* close = new wxButton(this, wxID_CANCEL, "Close"); buttons->Add(close);
    root->Add(buttons, 0, wxALL, margin); SetSizerAndFit(root); SetMinSize(GetSize()); CentreOnParent();
    for (auto* child : GetChildren()) {
        child->SetFont(Theme::GetFont()); child->SetForegroundColour(Theme::Get(Theme::Role::Text));
        child->SetBackgroundColour(Theme::Get(Theme::Role::Background));
    }
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { Tick(); }, timer_.GetId());
    timer_.Start(20);
}
StyleLearningDialog::~StyleLearningDialog() { timer_.Stop(); stop_.request_stop(); }
void StyleLearningDialog::Status(const std::string& text) {
    status_->SetLabel(wxstr(text)); status_->Wrap(Theme::Grid(100)); Layout();
}
void StyleLearningDialog::Invalidate(bool discardSample) {
    stop_.request_stop(); reading_ = false; result_.reset(); save_->Disable();
    if (discardSample) {
        captured_ = false;
        source_valid_ = false;
        sample_.reset();
        if (mapping_) mapping_->ChangeValue(wxEmptyString);
    }
    if (cancel_) cancel_->Disable();
    if (progress_) progress_->SetValue(0);
    if (analyze_) analyze_->Enable(captured_ && !worker_.valid());
    if (read_) read_->Enable(!worker_.valid());
    if (status_) Status("Settings changed. Read again if the rectangle changed, then analyze.");
}
bool StyleLearningDialog::CurrentSource() {
    const auto* e = g_gui.GetCurrentEditor();
    if (source_valid_ && std::chrono::steady_clock::now() - asset_check_ >= std::chrono::milliseconds(500)) {
        asset_check_ = std::chrono::steady_clock::now();
        const auto fingerprint = fingerprintGenerationAssets();
        if (!fingerprint || *fingerprint != asset_stamp_) { source_valid_ = false; return false; }
    }
    if (sample_) {
        const auto& b = sample_->floors().front().bounds;
        if (b.x0 != x_->GetValue() || b.y0 != y_->GetValue() || b.z != z_->GetValue()
            || b.width != width_->GetValue() || b.height != height_->GetValue()) return false;
    }
    return source_valid_ && e && g_version.IsVersionLoaded()
        && e->map.getVersion().client == g_version.GetCurrentVersion().getProtocolID() && e->map.getGeneration() == generation_
        && e->map.getChangeTracker().getGeneration() == revision_ && g_version.GetAssetRevision() == assets_
        && e->map.getWidth() == map_width_ && e->map.getHeight() == map_height_;
}
void StyleLearningDialog::Read() {
    if (worker_.valid()) return;
    Invalidate();
    if (!permission_->GetValue()) { Status("Confirm your right to use this map before reading."); return; }
    const auto* e = g_gui.GetCurrentEditor();
    if (!e || !g_version.IsVersionLoaded()) { Status("Open a compatible map and load its client first."); return; }
    if (e->map.getVersion().client != g_version.GetCurrentVersion().getProtocolID()) {
        Status("Open map and loaded client are incompatible."); return;
    }
    ids_ = loadedClientAssetIds();
    if (!ids_) { Status("Cannot identify client OTB/DAT assets."); return; }
    const auto stamp = fingerprintGenerationAssets();
    if (!stamp) { Status("Cannot read the loaded client brush catalog."); return; }
    asset_stamp_ = *stamp; asset_check_ = std::chrono::steady_clock::now();
    generation_ = e->map.getGeneration(); revision_ = e->map.getChangeTracker().getGeneration();
    assets_ = g_version.GetAssetRevision(); map_width_ = e->map.getWidth(); map_height_ = e->map.getHeight();
    FloorBounds bounds{z_->GetValue(), x_->GetValue(), y_->GetValue(), width_->GetValue(), height_->GetValue()};
    if (bounds.x0 + bounds.width > map_width_ || bounds.y0 + bounds.height > map_height_) { Status("Rectangle lies outside map dimensions."); return; }
    auto sample = SemanticMap::create(std::span(&bounds, 1), {512 * 512});
    if (!sample) { Status("Cannot allocate sample."); return; }
    sample_ = std::move(*sample); source_valid_ = true; row_ = 0; reading_ = true;
    read_->Disable(); analyze_->Disable(); cancel_->Enable(); Status("Reading the rectangle...");
}
void StyleLearningDialog::Analyze() {
    asset_check_ = {};
    if (worker_.valid() || !captured_ || !sample_ || !CurrentSource()) { Invalidate(); Status("Source is stale. Read the rectangle again."); return; }
    const auto mappingText = mapping_->GetValue().ToStdString(wxConvUTF8);
    if (mappingText.size() > (1u << 20)) { Status("Mapping exceeds the 1 MiB limit."); return; }
    auto mapping = parseKeyMap(mappingText);
    if (!mapping) { Status("Invalid G4 mapping. Keep the schema, tab separators and exact brush names."); return; }
    auto sample = inspectLearningSample(*sample_, *mapping, permission_->GetValue());
    if (!sample) { Status(sample.error()); return; }
    Status(std::format("{} recognized, {} skipped, {} present. Fitting G4 surface locally...", sample->recognized, sample->present - sample->recognized, sample->present));
    result_.reset(); save_->Disable(); read_->Disable(); analyze_->Disable(); stop_ = std::stop_source{};
    const auto ids = *ids_; const auto token = stop_.get_token();
    analyzed_mapping_ = mapping_->GetValue().ToStdString(wxConvUTF8);
    try {
        worker_ = std::async(std::launch::async, [sample = std::move(*sample), mapping = std::move(*mapping), ids, token]() mutable {
            try { return fitLearningSample(sample, std::move(mapping), ids.client, ids.otbSha256, ids.datSha256, token); }
            catch (const std::exception&) { return std::expected<StyleProfile, std::string>(std::unexpected("Analysis failed; no profile saved.")); }
        });
        cancel_->Enable();
    } catch (const std::exception&) {
        read_->Enable(); analyze_->Enable(captured_); Status("Cannot start the local analysis worker. No profile saved.");
    }
}
void StyleLearningDialog::Tick() {
    if (source_valid_ && !CurrentSource()) {
        source_valid_ = false; Invalidate(); Status("Map or client changed. Result invalidated; read again.");
    }
    if (reading_) {
        const auto* e = g_gui.GetCurrentEditor();
        if (!e || !CurrentSource()) return;
        const auto read = readLearningRow(e->map, *sample_, row_++);
        if (!read) { Invalidate(); Status(read.error()); return; }
        const int rows = sample_->floors().front().bounds.height;
        progress_->SetValue(row_ * 100 / rows);
        if (row_ == rows) {
            reading_ = false; captured_ = true;
            const auto mapping = serializeKeyMap(proposeKeyMap(sample_->groundKeys(), defaultKeyRules()));
            if (!mapping) { Invalidate(); Status("Cannot display ground mapping."); return; }
            mapping_->ChangeValue(wxstr(*mapping)); read_->Enable(); analyze_->Enable(); cancel_->Disable();
            const auto counts = summarize(*sample_);
            Status(std::format("Read {} present tiles. Review/correct mapping; unknown or unbrushed ground is not guessed. Then Analyze.", counts.tilesPerFloor[size_t(z_->GetValue())]));
        }
    }
    if (worker_.valid()) {
        if (!stop_.stop_requested()) progress_->Pulse();
        if (worker_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        auto result = worker_.get(); read_->Enable(); analyze_->Enable(captured_);
        cancel_->Disable(); progress_->SetValue(0);
        if (stop_.stop_requested() || !CurrentSource() || analyzed_mapping_ != mapping_->GetValue().ToStdString(wxConvUTF8)) {
            result_.reset(); save_->Disable(); return;
        }
        if (!result) { Status(result.error()); return; }
        result_ = std::move(*result); save_->Enable(); progress_->SetValue(100);
        const auto& stats = result_->stats;
        uint64_t present = 0, natural = 0;
        for (size_t i = 0; i < styleKindCount; ++i) { present += stats.kindTiles[i]; if (i < naturalKindCount) natural += stats.kindTiles[i]; }
        Status(std::format("Fit ready. Used {} of {} present tiles ({} skipped). Enter a name and choose Save profile.", natural, present, present - natural));
    }
}
void StyleLearningDialog::Save() {
    asset_check_ = {};
    if (!result_ || !CurrentSource() || !permission_->GetValue() || analyzed_mapping_ != mapping_->GetValue().ToStdString(wxConvUTF8)) {
        Invalidate(); Status("Result stale or permission missing. Analyze again."); return;
    }
    const auto ids = loadedClientAssetIds();
    if (!ids || checkProfile(*result_, ids->client, ids->otbSha256, ids->datSha256, 7) != ProfileCompatibility::Compatible) {
        Invalidate(); Status("Client assets changed. Analyze again."); return;
    }
    const auto saved = saveLearnedProfile(styleProfileDirectory(), name_->GetValue().ToStdString(wxConvUTF8), *result_);
    if (!saved) { Status(saved.error()); return; }
    if (IsModal()) EndModal(wxID_OK); else { save_->Disable(); Status("Profile saved locally. Reopen the generator to choose it."); }
}
