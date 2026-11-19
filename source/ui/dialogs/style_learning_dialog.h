// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/style_learning.h"
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
class StyleLearningDialog : public wxDialog {
public:
    explicit StyleLearningDialog(wxWindow* parent);
    ~StyleLearningDialog() override;
private:
    bool CurrentSource();
    void Invalidate(bool discardSample = true);
    void Read();
    void Analyze();
    void Save();
    void Tick();
    void Status(const std::string& text);
    wxTimer timer_{this};
    wxSpinCtrl *x_ = nullptr, *y_ = nullptr, *z_ = nullptr, *width_ = nullptr, *height_ = nullptr;
    wxTextCtrl *mapping_ = nullptr, *name_ = nullptr;
    wxCheckBox* permission_ = nullptr;
    wxButton *read_ = nullptr, *analyze_ = nullptr, *save_ = nullptr;
    wxButton* cancel_ = nullptr;
    wxStaticText* status_ = nullptr;
    wxGauge* progress_ = nullptr;
    uint64_t generation_ = 0, revision_ = 0, assets_ = 0;
    uint64_t asset_stamp_ = 0;
    std::chrono::steady_clock::time_point asset_check_{};
    int map_width_ = 0, map_height_ = 0, row_ = 0;
    bool captured_ = false, reading_ = false, source_valid_ = false;
    std::optional<rme::procedural::SemanticMap> sample_;
    std::optional<rme::procedural::StyleProfile> result_;
    std::optional<rme::procedural::ClientAssetIds> ids_;
    std::string analyzed_mapping_;
    std::stop_source stop_;
    std::future<std::expected<rme::procedural::StyleProfile, std::string>> worker_;
};
