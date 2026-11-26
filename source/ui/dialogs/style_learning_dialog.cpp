// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "ui/dialogs/style_learning_dialog.h"
#include "procedural/adapter/open_map_learning.h"
#include "procedural/core/doodad_class.h"
#include "procedural/core/reference_header.h"
#include "procedural/adapter/generation_assets.h"
#include "procedural/adapter/generation_selection.h"
#include "app/managers/version_manager.h"
#include "app/client_version.h"
#include "editor/editor.h"
#include "ui/gui.h"
#include "ui/theme.h"
#include "util/common.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/gauge.h>
#include <wx/generic/msgdlgg.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <algorithm>
#include <chrono>
#include <format>
#include <ranges>

using namespace rme::procedural;
namespace {
bool sameAssets(const ClientAssetIds& a, const ClientAssetIds& b) {
    return a.client == b.client && a.otbSha256 == b.otbSha256 && a.datSha256 == b.datSha256;
}
} // namespace
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
    auto* fromSelection = new wxButton(this, wxID_ANY, "Use map s&election");
    fromSelection->SetName("learn-use-selection");
    fromSelection->SetToolTip("Fill the rectangle with the bounds of the tiles selected on the map (one floor, up to 512 x 512).");
    fromSelection->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (UseSelection()) Status("Rectangle taken from the map selection. Read it to continue.");
    });
    root->Add(fromSelection, 0, wxLEFT | wxRIGHT | wxTOP, margin);
    permission_ = new wxCheckBox(this, wxID_ANY, "I have the &right to use this map for local style learning.");
    permission_->SetName("learn-permission"); root->Add(permission_, 0, wxALL, margin);
    permission_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { Invalidate(false); });
    root->Add(new wxStaticText(this, wxID_ANY, "Ground &mapping (edit the class column, keep the tabs):"), 0, wxLEFT | wxRIGHT, margin);
    mapping_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(Theme::Grid(100), Theme::Grid(28)), wxTE_MULTILINE | wxHSCROLL);
    mapping_->SetName("learn-mapping"); root->Add(mapping_, 1, wxEXPAND | wxALL, margin);
    mapping_->SetToolTip("G4 classes (case sensitive): Grass, Dirt, Rock, Shore, Water, Other, Unmapped, NoGround. Keep unknown grounds Unmapped. Edit the class column; keep the exact brush name.");
    mapping_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { Invalidate(false); });
    root->Add(new wxStaticText(this, wxID_ANY, "Items in the &sample (most used first; what the learning does with each):"), 0, wxLEFT | wxRIGHT, margin);
    items_text_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(Theme::Grid(100), Theme::Grid(20)), wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
    items_text_->SetName("learn-items"); root->Add(items_text_, 1, wxEXPAND | wxALL, margin);
    auto* nameRow = new wxBoxSizer(wxHORIZONTAL);
    nameRow->Add(new wxStaticText(this, wxID_ANY, "Profile &name:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    name_ = new wxTextCtrl(this, wxID_ANY); name_->SetName("learn-name");
    nameRow->Add(name_, 1); root->Add(nameRow, 0, wxEXPAND | wxLEFT | wxRIGHT, margin);
    root->Add(new wxStaticText(this, wxID_ANY, "Use 1-64 ASCII letters, digits, hyphens or underscores."), 0, wxALL, margin);
    auto* protectRow = new wxBoxSizer(wxHORIZONTAL);
    protect_ = new wxCheckBox(this, wxID_ANY, "Also add this area to the cop&y index:");
    protect_->SetName("learn-protect");
    protect_->SetToolTip("The generator can then refuse to place terrain that copies this area (also rotated or mirrored). "
        "The index keeps window hashes of the five natural ground classes only, under the profile name; no map name or position.");
    protectRow->Add(protect_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    index_name_ = new wxTextCtrl(this, wxID_ANY, "my-maps"); index_name_->SetName("learn-index-name");
    index_name_->SetToolTip("Copy index file name in the local profile folder (1-64 letters, digits, '-' or '_'). "
        "A new index is created when none has this name; an existing one gets this area added (up to 16 areas).");
    protectRow->Add(index_name_, 1);
    root->Add(protectRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, margin);
    root->Add(new wxStaticText(this, wxID_ANY, std::format("Sa&mples for one profile (up to {}; may come from different maps of the same client):",
        maxLearningSamples)), 0, wxLEFT | wxRIGHT, margin);
    auto* sampleRow = new wxBoxSizer(wxHORIZONTAL);
    samples_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(Theme::Grid(70), Theme::Grid(16)));
    samples_->SetName("learn-samples");
    samples_->SetToolTip("With samples added, Analyze learns one profile from all of them, weighted by their tiles. "
        "Without any, it learns from the rectangle just read.");
    sampleRow->Add(samples_, 1, wxEXPAND | wxRIGHT, gap);
    auto* sampleButtons = new wxBoxSizer(wxVERTICAL);
    add_ = new wxButton(this, wxID_ANY, "A&dd sample"); add_->SetName("learn-add-sample");
    add_->SetToolTip("Keep the rectangle just read, with its reviewed mapping, as one sample of the profile.");
    add_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AddSample(); }); add_->Disable();
    remove_ = new wxButton(this, wxID_ANY, "Remo&ve sample"); remove_->SetName("learn-remove-sample");
    remove_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RemoveSample(); }); remove_->Disable();
    samples_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { remove_->Enable(samples_->GetSelection() != wxNOT_FOUND && !worker_.valid()); });
    sampleButtons->Add(add_, 0, wxEXPAND | wxBOTTOM, gap); sampleButtons->Add(remove_, 0, wxEXPAND);
    sampleRow->Add(sampleButtons, 0);
    root->Add(sampleRow, 0, wxEXPAND | wxALL, margin);
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
    if (const auto* e = g_gui.GetCurrentEditor(); e && e->selection.size() != 0 && UseSelection())
        Status("Rectangle taken from the map selection. Read it to continue. Data stays on this computer.");
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { Tick(); }, timer_.GetId());
    timer_.Start(20);
}
bool StyleLearningDialog::UseSelection() {
    if (worker_.valid() || reading_) return false;
    auto* e = g_gui.GetCurrentEditor();
    if (!e) { Status("Open a map first."); return false; }
    constexpr uint64_t maxCells = 512 * 512;
    const auto captured = captureGenerationSelection(*e, SelectionMaskBudget{maxCells, maxCells + 64 * 1024});
    if (!captured) {
        switch (captured.error()) {
            case SelectionMaskError::Empty: Status("Nothing is selected. Select an area on the map first."); break;
            case SelectionMaskError::MultipleFloors: Status("The selection spans several floors. Select tiles on one floor only."); break;
            case SelectionMaskError::CellLimit:
            case SelectionMaskError::MemoryLimit: Status("The selection is larger than 512 x 512 tiles."); break;
            case SelectionMaskError::Busy: Status("Finish the current selection first, then try again."); break;
            default: Status("The selection could not be read. Select the area again."); break;
        }
        return false;
    }
    const auto& r = captured->region;
    if (r.z < 0 || r.z > 7) { Status("Select an area on a surface floor (0-7)."); return false; }
    // A larger cell budget would admit a 1 x 262144 strip; the spin limits refuse it.
    if (r.width > 512 || r.height > 512) { Status("The selection is larger than 512 x 512 tiles."); return false; }
    x_->SetValue(r.x); y_->SetValue(r.y); z_->SetValue(r.z);
    width_->SetValue(int(r.width)); height_->SetValue(int(r.height));
    Invalidate();
    return true;
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
        counts_ = {};
        if (mapping_) mapping_->ChangeValue(wxEmptyString);
        if (items_text_) items_text_->ChangeValue(wxEmptyString);
    }
    if (cancel_) cancel_->Disable();
    if (progress_) progress_->SetValue(0);
    if (analyze_) analyze_->Enable((captured_ || !added_.empty()) && !worker_.valid());
    if (add_) add_->Enable(captured_ && !worker_.valid() && added_.size() < maxLearningSamples);
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
    sample_ = std::move(*sample); counts_ = {}; source_valid_ = true; row_ = 0; reading_ = true;
    read_->Disable(); analyze_->Disable(); cancel_->Enable(); Status("Reading the rectangle...");
}
std::expected<std::pair<LearningSample, std::vector<KeyMapEntry>>, std::string> StyleLearningDialog::InspectCurrent() {
    const auto mappingText = mapping_->GetValue().ToStdString(wxConvUTF8);
    if (mappingText.size() > (1u << 20)) return std::unexpected("Mapping exceeds the 1 MiB limit.");
    auto mapping = parseKeyMap(mappingText);
    if (!mapping) return std::unexpected("Invalid G4 mapping. Keep the schema, tab separators and exact brush names.");
    std::vector<GroundDoodadItems> doodadItems;
    for (const auto& [where, items] : counts_.doodads) doodadItems.push_back({where.first, where.second, items});
    std::vector<GroundOverlayItems> overlayItems;
    for (const auto& [where, counts] : counts_.overlays) overlayItems.push_back({where.first, where.second, counts.first, counts.second});
    std::vector<GroundDoodadItems> wallBrushes;
    for (const auto& [where, pieces] : counts_.walls) wallBrushes.push_back({where.first, where.second, pieces});
    std::vector<GroundDoodadItems> carpetItems;
    for (const auto& [where, pieces] : counts_.carpets) carpetItems.push_back({where.first, where.second, pieces});
    auto sample = inspectLearningSample(*sample_, *mapping, doodadItems, permission_->GetValue(), {}, overlayItems, wallBrushes, counts_.placed, carpetItems, counts_.grounds);
    if (!sample) return std::unexpected(sample.error());
    return std::pair(std::move(*sample), std::move(*mapping));
}
void StyleLearningDialog::AddSample() {
    asset_check_ = {};
    if (worker_.valid() || !captured_ || !sample_ || !CurrentSource()) { Invalidate(); Status("Source is stale. Read the rectangle again."); return; }
    if (added_.size() >= maxLearningSamples) { Status(std::format("A profile takes at most {} samples.", maxLearningSamples)); return; }
    if (!added_.empty() && !sameAssets(added_.front().ids, *ids_)) { Status("This map uses another client than the samples already added. Not added."); return; }
    auto inspected = InspectCurrent();
    if (!inspected) { Status(inspected.error()); return; }
    std::vector<std::vector<KeyMapEntry>> mappings;
    for (const auto& a : added_) mappings.push_back(a.mapping);
    mappings.push_back(inspected->second);
    if (const auto merged = mergeLearningMappings(mappings); !merged) { Status(merged.error() + " Correct the mapping; not added."); return; }
    const auto& b = sample_->floors().front().bounds;
    // The position is shown here only; profiles never store it.
    samples_->Append(wxstr(std::format("{}, {}, floor {}: {} x {}, {} tiles", b.x0, b.y0, b.z, b.width, b.height, inspected->first.present)));
    added_.push_back({std::move(inspected->first), std::move(inspected->second), *ids_});
    ++added_revision_; result_.reset(); save_->Disable();
    add_->Enable(added_.size() < maxLearningSamples); analyze_->Enable(true);
    Status(std::format("{} sample(s) added. Read another rectangle (this or another map) and add it, or Analyze to learn from all of them.", added_.size()));
}
void StyleLearningDialog::RemoveSample() {
    const int selected = samples_->GetSelection();
    if (worker_.valid() || selected == wxNOT_FOUND || size_t(selected) >= added_.size()) return;
    added_.erase(added_.begin() + selected); samples_->Delete(unsigned(selected));
    ++added_revision_; result_.reset(); save_->Disable(); remove_->Disable();
    add_->Enable(captured_ && added_.size() < maxLearningSamples); analyze_->Enable(captured_ || !added_.empty());
    Status(std::format("Sample removed; {} left.", added_.size()));
}
void StyleLearningDialog::Analyze() {
    asset_check_ = {};
    if (worker_.valid()) return;
    if (!added_.empty()) {
        // Several samples: no copy fingerprint (an index entry is one area), the read rectangle is not needed.
        const auto ids = loadedClientAssetIds();
        if (!ids || !sameAssets(*ids, added_.front().ids)) { Status("Load the client the samples were read with, then Analyze."); return; }
        std::vector<LearningSample> samples;
        std::vector<std::vector<KeyMapEntry>> mappings;
        for (const auto& a : added_) { samples.push_back(a.sample); mappings.push_back(a.mapping); }
        auto mapping = mergeLearningMappings(mappings);
        if (!mapping) { Status(mapping.error()); return; }
        result_.reset(); save_->Disable(); read_->Disable(); analyze_->Disable(); add_->Disable(); remove_->Disable(); stop_ = std::stop_source{};
        analyzed_added_ = true; analyzed_revision_ = added_revision_;
        const auto token = stop_.get_token();
        Status(std::format("Fitting one profile from {} samples locally...", samples.size()));
        try {
            worker_ = std::async(std::launch::async, [samples = std::move(samples), mapping = std::move(*mapping), ids = *ids, token]() mutable
                -> std::expected<Learned, std::string> {
                try {
                    auto merged = mergeLearningSamples(samples);
                    if (!merged) return std::unexpected(merged.error());
                    auto profile = fitLearningSample(*merged, std::move(mapping), ids.client, ids.otbSha256, ids.datSha256, token);
                    if (!profile) return std::unexpected(profile.error());
                    return Learned{std::move(*profile), std::nullopt};
                } catch (const std::exception&) { return std::unexpected("Analysis failed; no profile saved."); }
            });
            cancel_->Enable();
        } catch (const std::exception&) {
            read_->Enable(); analyze_->Enable(); Status("Cannot start the local analysis worker. No profile saved.");
        }
        return;
    }
    if (!captured_ || !sample_ || !CurrentSource()) { Invalidate(); Status("Source is stale. Read the rectangle again."); return; }
    auto inspected = InspectCurrent();
    if (!inspected) { Status(inspected.error()); return; }
    auto sample = std::optional(std::move(inspected->first));
    auto mapping = std::optional(std::move(inspected->second));
    analyzed_added_ = false;
    Status(std::format("{} recognized, {} skipped, {} present. Fitting G4 surface locally...", sample->recognized, sample->present - sample->recognized, sample->present));
    auto raster = sampleClassRaster(*sample_, *mapping);
    if (!raster) { Status("Not enough memory for the sample's ground classes."); return; }
    {
        Sha256 hash;
        hash.update(std::as_bytes(std::span(raster->data(), raster->size())));
        raster_sha_ = Sha256::hex(hash.finish());
    }
    const auto& bounds = sample_->floors().front().bounds;
    const auto width = uint32_t(bounds.width), height = uint32_t(bounds.height);
    result_.reset(); save_->Disable(); read_->Disable(); analyze_->Disable(); add_->Disable(); stop_ = std::stop_source{};
    const auto ids = *ids_; const auto token = stop_.get_token();
    analyzed_mapping_ = mapping_->GetValue().ToStdString(wxConvUTF8);
    try {
        worker_ = std::async(std::launch::async, [sample = std::move(*sample), mapping = std::move(*mapping), raster = std::move(*raster),
                                                  width, height, ids, token]() mutable -> std::expected<Learned, std::string> {
            try {
                auto profile = fitLearningSample(sample, std::move(mapping), ids.client, ids.otbSha256, ids.datSha256, token);
                if (!profile) return std::unexpected(profile.error());
                auto fingerprint = classFingerprint(width, height, raster, token);
                if (!fingerprint) return std::unexpected(fingerprint.error() == SimilarityError::Cancelled
                    ? "Analysis stopped." : "Cannot fingerprint the sample's ground classes; no profile saved.");
                return Learned{std::move(*profile), std::move(*fingerprint)};
            } catch (const std::exception&) { return std::unexpected("Analysis failed; no profile saved."); }
        });
        cancel_->Enable();
    } catch (const std::exception&) {
        read_->Enable(); analyze_->Enable(captured_); add_->Enable(captured_ && added_.size() < maxLearningSamples);
        Status("Cannot start the local analysis worker. No profile saved.");
    }
}
void StyleLearningDialog::Tick() {
    if (source_valid_ && !CurrentSource()) {
        source_valid_ = false; Invalidate(); Status("Map or client changed. Result invalidated; read again.");
    }
    if (reading_) {
        const auto* e = g_gui.GetCurrentEditor();
        if (!e || !CurrentSource()) return;
        const auto read = readLearningRow(e->map, *sample_, counts_, row_++);
        if (!read) { Invalidate(); Status(read.error()); return; }
        const int rows = sample_->floors().front().bounds.height;
        progress_->SetValue(row_ * 100 / rows);
        if (row_ == rows) {
            reading_ = false; captured_ = true;
            // Swamp is walkable natural ground: learned as a grass patch here, unlike the reference rules
            // (which keep it Other), so the generator can reproduce it.
            std::vector<KeyRule> rules{{"swamp", StyleKind::Grass}};
            rules.insert(rules.end(), defaultKeyRules().begin(), defaultKeyRules().end());
            auto proposed = proposeKeyMap(sample_->groundKeys(), rules);
            promoteMainLand(proposed, *sample_);
            const auto mapping = serializeKeyMap(proposed);
            if (!mapping) { Invalidate(); Status("Cannot display ground mapping."); return; }
            mapping_->ChangeValue(wxstr(*mapping)); read_->Enable(); analyze_->Enable(); cancel_->Disable();
            add_->Enable(added_.size() < maxLearningSamples);
            ShowItems();
            const auto counts = summarize(*sample_);
            const uint64_t present = counts.tilesPerFloor[size_t(z_->GetValue())];
            // Patch sizes and vegetation brushes need room to show; a few thousand tiles give extreme fits.
            constexpr uint64_t smallSample = 20000;
            Status(std::format("Read {} present tiles in {} x {}.{} Review/correct mapping; unknown or unbrushed ground is not guessed. Then Analyze.",
                present, width_->GetValue(), height_->GetValue(),
                present < smallSample ? std::format(" Small sample: select at least {} tiles of terrain for a reliable style.", smallSample) : std::string()));
        }
    }
    if (worker_.valid()) {
        if (!stop_.stop_requested()) progress_->Pulse();
        if (worker_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        auto result = worker_.get(); read_->Enable(); analyze_->Enable(captured_ || !added_.empty());
        add_->Enable(captured_ && added_.size() < maxLearningSamples); remove_->Enable(samples_->GetSelection() != wxNOT_FOUND);
        cancel_->Disable(); progress_->SetValue(0);
        if (stop_.stop_requested() || !ResultCurrent()) {
            result_.reset(); save_->Disable(); return;
        }
        if (!result) { Status(result.error()); return; }
        result_ = std::move(*result); save_->Enable(); progress_->SetValue(100);
        const auto& stats = result_->profile.stats;
        uint64_t present = 0, natural = 0;
        for (size_t i = 0; i < styleKindCount; ++i) { present += stats.kindTiles[i]; if (i < naturalKindCount) natural += stats.kindTiles[i]; }
        Status(std::format("Fit ready{}. Used {} of {} present tiles ({} skipped); {} ranking bands. Enter a name and choose Save profile.",
            analyzed_added_ ? std::format(" from {} samples", added_.size()) : std::string(), natural, present, present - natural,
            result_->profile.bands.size()));
    }
}
bool StyleLearningDialog::ResultCurrent() {
    // A profile of added samples depends on them, not on the rectangle shown; one read rectangle on its source and mapping.
    if (analyzed_added_) return analyzed_revision_ == added_revision_ && !added_.empty();
    return CurrentSource() && analyzed_mapping_ == mapping_->GetValue().ToStdString(wxConvUTF8);
}
void StyleLearningDialog::Save() {
    asset_check_ = {};
    if (!result_ || !ResultCurrent() || !permission_->GetValue()) {
        Invalidate(); Status("Result stale or permission missing. Analyze again."); return;
    }
    const auto name = name_->GetValue().ToStdString(wxConvUTF8);
    const auto directory = styleProfileDirectory();
    std::error_code ec;
    const bool replace = validLearnedProfileName(name) && std::filesystem::exists(directory / (name + ".profile"), ec);
    if (ec) { Status("Cannot inspect the local profile folder."); return; }
    if (replace) {
        wxGenericMessageDialog dialog(this, wxstr(std::format("Replace the profile \"{}\"?", name)),
            "Learn from open map", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
        dialog.SetName("learn-confirm");
        if (dialog.ShowModal() != wxID_YES) { Status("Profile not saved."); return; }
        // The timer can invalidate the result while the confirmation dialog is open.
        asset_check_ = {};
        if (!result_ || !ResultCurrent() || !permission_->GetValue()) {
            Invalidate(); Status("Result stale or permission missing. Analyze again."); return;
        }
    }
    const auto ids = loadedClientAssetIds();
    if (!ids || checkProfile(result_->profile, ids->client, ids->otbSha256, ids->datSha256, 7) != ProfileCompatibility::Compatible) {
        Invalidate(); Status("Client assets changed. Analyze again."); return;
    }
    // The index edit is checked before the profile is written, so a refused edit saves nothing.
    std::optional<Protection> protection;
    if (protect_->GetValue()) {
        if (!result_->fingerprint) { Status("A copy index entry is one area: add each area to the index on its own. Nothing saved."); return; }
        auto prepared = PrepareProtection(*ids);
        if (!prepared) { Status(prepared.error() + " Nothing saved."); return; }
        protection = std::move(*prepared);
    }
    const auto saved = saveLearnedProfile(directory, name, result_->profile, replace);
    if (!saved) { Status(saved.error()); return; }
    if (protection) {
        const auto indexName = index_name_->GetValue().ToStdString(wxConvUTF8);
        const auto written = saveCopyIndex(styleProfileDirectory(), indexName, protection->index, protection->previousSha256);
        if (!written) {
            save_->Disable();
            Status(std::format("Profile \"{}\" saved, but the copy index was not updated: {}", name, written.error()));
            return;
        }
    }
    if (IsModal()) EndModal(wxID_OK);
    else {
        save_->Disable();
        Status(protection ? "Profile and copy index saved locally. Reopen the generator to choose them."
                          : "Profile saved locally. Reopen the generator to choose it.");
    }
}
std::expected<StyleLearningDialog::Protection, std::string> StyleLearningDialog::PrepareProtection(const ClientAssetIds& ids) const {
    const auto indexName = index_name_->GetValue().ToStdString(wxConvUTF8);
    if (!validCopyAlias(indexName)) return std::unexpected("Use 1-64 letters, digits, '-' or '_' for the copy index name.");
    const auto path = styleProfileDirectory() / (indexName + ".i53");
    Protection protection;
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        if (copyIndexNeedsRebuild(path)) return std::unexpected("That copy index has the old format. Choose another index name.");
        protection.previousSha256 = fileSha256(path);
        auto existing = readCopyIndex(path, ids.client, ids.otbSha256, ids.datSha256);
        if (!existing || protection.previousSha256.empty())
            return std::unexpected("That copy index cannot be read or belongs to another client. Choose another index name.");
        protection.index = std::move(*existing);
    } else if (ec) {
        return std::unexpected("Cannot inspect the local profile folder.");
    } else {
        protection.index.client = ids.client; protection.index.otbSha256 = ids.otbSha256; protection.index.datSha256 = ids.datSha256;
    }
    const auto sha = [](std::string_view text) {
        Sha256 hash;
        hash.update(std::as_bytes(std::span(text.data(), text.size())));
        return Sha256::hex(hash.finish());
    };
    CopyReference reference{name_->GetValue().ToStdString(wxConvUTF8), raster_sha_,
        sha(serializeFingerprint(*result_->fingerprint)), sha(analyzed_mapping_), *result_->fingerprint};
    auto added = addCopyReference(std::move(protection.index), std::move(reference));
    if (!added) return std::unexpected(copyEditErrorText(added.error()));
    protection.index = std::move(*added);
    return protection;
}
void StyleLearningDialog::ShowItems() {
    std::vector<std::pair<uint16_t, const SampleItem*>> sorted;
    for (const auto& [id, item] : counts_.items) sorted.emplace_back(id, &item);
    std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a.second->count != b.second->count ? a.second->count > b.second->count : a.first < b.first; });
    const auto kindName = [](DecorKind kind) -> std::string_view {
        switch (kind) {
            case DecorKind::Tree: return "tree";
            case DecorKind::Bush: return "bush";
            case DecorKind::Stone: return "stone";
            case DecorKind::None: break;
        }
        return "";
    };
    std::string text = "count\tid\tname\tsource\tlearned as\n";
    constexpr size_t shown = 80;
    for (const auto& [id, item] : sorted | std::views::take(shown)) {
        // As readLearningRow: the item name decides when it says a kind, otherwise the brush name.
        const auto byName = classifyDoodadBrush(item->name);
        const auto kind = kindName(byName != DecorKind::None ? byName : classifyDoodadBrush(item->brush));
        const std::string learned = kind.empty() ? "overlay" : std::string(kind);
        text += std::format("{}\t{}\t{}\t{}\t{}\n", item->count, id, item->name,
            item->brush.empty() ? std::string("RAW, no brush") : std::format("brush \"{}\"", item->brush), learned);
    }
    if (sorted.size() > shown) text += std::format("... and {} more item types\n", sorted.size() - shown);
    items_text_->ChangeValue(wxstr(text));
}
