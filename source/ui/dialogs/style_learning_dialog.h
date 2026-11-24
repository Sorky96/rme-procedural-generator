// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/copy_guard.h"
#include "procedural/core/style_learning.h"
#include "procedural/adapter/open_map_learning.h"
#include "procedural/adapter/style_profile_store.h"
#include <future>
#include <chrono>
#include <wx/dialog.h>
#include <wx/timer.h>
class wxSpinCtrl;
class wxTextCtrl;
class wxStaticText;
class wxButton;
class wxCheckBox;
class wxGauge;
class wxListBox;
class StyleLearningDialog : public wxDialog {
public:
    explicit StyleLearningDialog(wxWindow* parent);
    ~StyleLearningDialog() override;
private:
    bool CurrentSource();
    void Invalidate(bool discardSample = true);
    bool UseSelection();
    void Read();
    void Analyze();
    bool ResultCurrent();
    void AddSample();
    void RemoveSample();
    // The read rectangle as a learning sample with its reviewed mapping, or why not.
    std::expected<std::pair<rme::procedural::LearningSample, std::vector<rme::procedural::KeyMapEntry>>, std::string> InspectCurrent();
    void Save();
    void Tick();
    void Status(const std::string& text);
    void ShowItems();
    struct Protection {
        rme::procedural::CopyIndex index; // the chosen copy index with the analyzed sample added
        std::string previousSha256;       // of the file it was read from; empty for a new index
    };
    // Reads (or starts) the chosen copy index and adds the analyzed sample to it in memory.
    std::expected<Protection, std::string> PrepareProtection(const rme::procedural::ClientAssetIds& ids) const;
    struct Learned {
        rme::procedural::StyleProfile profile;
        // Of the sample's natural ground classes (I57); none for a profile of several samples.
        std::optional<rme::procedural::Fingerprint> fingerprint;
    };
    // A sample added for a profile of several samples (I62); it outlives the map it was read from.
    struct AddedSample {
        rme::procedural::LearningSample sample;
        std::vector<rme::procedural::KeyMapEntry> mapping;
        rme::procedural::ClientAssetIds ids;
    };
    std::vector<AddedSample> added_;
    uint64_t added_revision_ = 0, analyzed_revision_ = 0;
    bool analyzed_added_ = false; // the result came from the added samples, not the read rectangle
    wxTimer timer_{this};
    wxSpinCtrl *x_ = nullptr, *y_ = nullptr, *z_ = nullptr, *width_ = nullptr, *height_ = nullptr;
    wxTextCtrl *mapping_ = nullptr, *name_ = nullptr, *items_text_ = nullptr;
    wxCheckBox* permission_ = nullptr;
    wxCheckBox* protect_ = nullptr;
    wxTextCtrl* index_name_ = nullptr;
    wxButton *read_ = nullptr, *analyze_ = nullptr, *save_ = nullptr;
    wxButton* cancel_ = nullptr;
    wxButton *add_ = nullptr, *remove_ = nullptr;
    wxListBox* samples_ = nullptr;
    wxStaticText* status_ = nullptr;
    wxGauge* progress_ = nullptr;
    uint64_t generation_ = 0, revision_ = 0, assets_ = 0;
    uint64_t asset_stamp_ = 0;
    std::chrono::steady_clock::time_point asset_check_{};
    int map_width_ = 0, map_height_ = 0, row_ = 0;
    bool captured_ = false, reading_ = false, source_valid_ = false;
    std::optional<rme::procedural::SemanticMap> sample_;
    rme::procedural::SampleCounts counts_;
    std::optional<Learned> result_;
    std::string raster_sha_; // SHA-256 of the class raster the fingerprint came from
    std::optional<rme::procedural::ClientAssetIds> ids_;
    std::string analyzed_mapping_;
    std::stop_source stop_;
    std::future<std::expected<Learned, std::string>> worker_;
};
