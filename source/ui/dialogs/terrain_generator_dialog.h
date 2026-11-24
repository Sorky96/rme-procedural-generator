//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_DIALOGS_TERRAIN_GENERATOR_DIALOG_H_
#define RME_UI_DIALOGS_TERRAIN_GENERATOR_DIALOG_H_

#include "app/main.h"
#include "procedural/adapter/generation_selection.h"
#include "procedural/adapter/style_profile_store.h"
#include "procedural/adapter/terrain_preparation.h"
#include "procedural/core/candidate_series.h"
#include "procedural/core/terrain_preset.h"
#include "procedural/core/preview_viewport.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include <wx/arrstr.h>
#include <wx/bitmap.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/timer.h>

class Editor;
class wxButton;
class wxChoice;
class wxComboBox;
class wxPanel;
class wxListCtrl;
class wxListEvent;
class wxNotebook;
class wxImageList;
class wxGauge;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;

// Modal procedural terrain generator for the map that was current when it opened. Preview and
// Validate never change the map or history; "Place on map" commits the owned preview as one Undo
// step through DrawOperations and closes the dialog with wxID_OK. Every command looks the current
// editor up again and refuses a different map, live session, busy selection/history, active Lua
// transaction or missing client. Core workers own plain inputs; adapter access is GUI-only.
class TerrainGeneratorDialog : public wxDialog {
public:
	explicit TerrainGeneratorDialog(wxWindow* parent);
	~TerrainGeneratorDialog() override = default;

private:
	struct Request {
		rme::procedural::GenerationConfig config;
		rme::procedural::GroundProfile profile;
	};

	void OnInputChanged(wxCommandEvent& event);
	void OnProfileChosen(wxCommandEvent& event);
	void OnFeedback(rme::procedural::FeedbackAction action);
	void OnRollback(wxCommandEvent& event);
	void RefreshHistoryChoices();
	std::optional<rme::procedural::StyleHistoryStore> style_history_;
	std::string history_error_;
	wxTextCtrl* feedback_note_ = nullptr;
	wxButton *approve_button_ = nullptr, *reject_button_ = nullptr, *adjust_button_ = nullptr, *rollback_button_ = nullptr;
	wxChoice* history_choice_ = nullptr;
	std::vector<std::string> history_ids_;
	rme::procedural::StyleAdjustment default_adjustment_ {};
	void OnPreview(wxCommandEvent& event);
	void OnPreviewTick(wxTimerEvent& event);
    void OnCandidateSelected(wxListEvent& event);
    bool StartCandidate(uint64_t seed);
    void SelectCandidate(uint32_t index);
    void UpdateCandidateLabels();
    bool SeriesCurrent();
    bool CandidateAllowed() const;
    struct Candidate {
        rme::procedural::CandidateScore score;
        uint64_t initialSeed = 0, seed = 0;
        uint32_t attempts = 0;
        std::array<uint64_t, 5> fingerprints {};
        wxBitmap thumbnail;
        rme::procedural::CopyAssessment copyAssessment;
        uint64_t elapsedMs = 0; // preparation of the last attempt
        uint64_t longestStepMs = 0; // I61: longest GUI-thread step of that preparation
        wxBitmap surface;       // I60: Surface minimap for comparison, reduced when the series keeps too much
    };
    std::vector<Candidate> candidates_;
    std::optional<Request> series_request_;
    std::optional<uint32_t> selected_candidate_, recommended_candidate_;
    uint32_t candidate_count_ = 1;
    uint64_t series_revision_ = 0, series_assets_ = 0, working_bytes_ = 0;
    bool collecting_ = false, updating_candidates_ = false;
    std::span<const rme::procedural::CandidateBand> ranking_bands_;
    wxSpinCtrl* candidate_count_spin_ = nullptr;
    wxTextCtrl* copy_index_text_ = nullptr;
    // I57: "None", the local copy indices (usable or with the reason they are not), "Other file...".
    // Choosing an index fills copy_index_text_, which the series reads.
    wxChoice* copy_index_choice_ = nullptr;
    std::vector<rme::procedural::CopyIndexFile> copy_index_files_; // entry i is choice i + 1
    void LoadCopyIndices();
    void OnCopyIndexChosen(wxCommandEvent& event);
    std::optional<rme::procedural::CopyIndex> copy_index_;
    wxListCtrl* candidate_list_ = nullptr;
    wxImageList* candidate_images_ = nullptr; // owned by list via AssignImageList
    wxGauge* candidate_progress_ = nullptr;
    // I58: statistics and warnings of the Selected candidate (terrainReportOf).
    wxListCtrl* statistics_list_ = nullptr;
    wxListCtrl* warnings_list_ = nullptr;
    std::chrono::steady_clock::time_point candidate_started_;
    // I61: GUI-thread steps of the running preparation and the longest one.
    uint64_t preview_step_count_ = 0;
    std::chrono::steady_clock::duration preview_longest_step_ {};
    // Fills both lists; returns the number of {blocking, other} warnings.
    std::pair<size_t, size_t> ShowReport(const Request& request, const Candidate& row);
    // I59: named settings in the local profile folder's "presets" subfolder, and a random seed.
    wxChoice* preset_choice_ = nullptr; // "(none)", then the preset names
    wxTextCtrl* preset_name_text_ = nullptr;
    wxNotebook* sections_ = nullptr;
    void LoadPresets(const wxString& select = wxEmptyString);
    void OnPresetChosen(wxCommandEvent& event);
    void OnPresetSave(wxCommandEvent& event);
    void OnPresetDelete(wxCommandEvent& event);
    void OnRandomSeed(wxCommandEvent& event);
    [[nodiscard]] bool Confirm(const wxString& question);
    // The section inputs a preset holds, in tab order (not the area position or the copy check).
    [[nodiscard]] std::vector<wxWindow*> PresetControls() const;
    [[nodiscard]] rme::procedural::TerrainPreset CollectPreset() const;
    // Applies every value or none: on a refusal the previous settings return and the reason is given.
    [[nodiscard]] std::optional<std::string> ApplyPreset(const rme::procedural::TerrainPreset& preset);
    // I60: zoom and pan of the preview (one shared view), and a second candidate shown beside the Selected one.
    rme::procedural::PreviewViewport view_;
    rme::procedural::PreviewArea view_area_;
    std::optional<wxPoint> drag_from_;
    std::optional<rme::procedural::PreviewCell> hovered_cell_;
    wxStaticText* view_label_ = nullptr;
    wxChoice* compare_choice_ = nullptr; // "None", then the other candidates
    std::vector<uint32_t> compare_entries_; // candidate index of choice entry i + 1
    bool compare_reduced_ = false;
    [[nodiscard]] rme::procedural::PreviewArea PreviewAreaSize() const;
    [[nodiscard]] std::optional<uint32_t> ComparedCandidate() const;
    // Pane i of the panel (0 alone; 0 left and 1 right when comparing) and its rectangle.
    [[nodiscard]] std::vector<wxRect> PreviewPanes() const;
    [[nodiscard]] std::optional<size_t> PaneAt(wxPoint point) const;
    void SyncView(); // keeps the view for an area of the same size, otherwise fits
    void UpdateViewLabel();
    void RefreshCompareChoice();
    void OnPreviewMouse(wxMouseEvent& event);
    void OnPreviewKey(wxKeyEvent& event);
	void OnCancel(wxCommandEvent& event);
	void SchedulePreviewTick();
	void OnValidate(wxCommandEvent& event);
	void OnPlace(wxCommandEvent& event);
	void OnPaintPreview(wxPaintEvent& event);

	[[nodiscard]] bool SelectionMode() const;
	[[nodiscard]] bool NaturalSurface() const;
	[[nodiscard]] std::optional<Request> ReadRequest();
	[[nodiscard]] Editor* CurrentTarget();
	[[nodiscard]] bool BuildPreviewBitmap();
	void ReleasePreview();
	void UpdateControlStates();
	void UpdateClientLabel();
	void LoadStyleProfiles();
	void SetStatus(const wxString& text, bool error);

	uint64_t source_map_generation_ = 0;
	bool has_source_map_ = false;

	std::optional<rme::procedural::PreparedTerrain> prepared_;
	std::optional<rme::procedural::GenerationSelection> selection_;
	std::optional<Request> pending_request_;
	std::optional<rme::procedural::TerrainPreparationSteps> preview_steps_;
	wxTimer preview_timer_{this};
	wxBitmap preview_bitmap_;
	wxString status_message_;

	wxChoice* scope_choice_ = nullptr;
	wxSpinCtrl* x_spin_ = nullptr;
	wxSpinCtrl* y_spin_ = nullptr;
	wxSpinCtrl* z_spin_ = nullptr;
	wxSpinCtrl* width_spin_ = nullptr;
	wxSpinCtrl* height_spin_ = nullptr;
	wxTextCtrl* seed_text_ = nullptr;
	wxSpinCtrl* retry_spin_ = nullptr;
    uint32_t preview_attempt_ = 0;
    uint64_t preview_first_seed_ = 0;
    bool preview_fallback_ = false;
    wxSpinCtrl* grass_weight_spin_ = nullptr;
	wxSpinCtrl* dirt_weight_spin_ = nullptr;
	wxSpinCtrl* water_weight_spin_ = nullptr;
	wxComboBox* grass_brush_choice_ = nullptr;
	wxComboBox* dirt_brush_choice_ = nullptr;
	wxComboBox* water_brush_choice_ = nullptr;
	wxChoice* style_choice_ = nullptr;
	wxChoice* profile_choice_ = nullptr;
	std::vector<rme::procedural::OfferedProfile> profiles_; // entry i is choice i + 1
	std::array<wxString, rme::procedural::naturalKindCount> default_brushes_; // grass, dirt, rock, shore, water
	std::array<wxString, 3> default_decor_brushes_;                            // tree, bush, stone
	wxComboBox* rock_brush_choice_ = nullptr;
	wxComboBox* shore_brush_choice_ = nullptr;
	wxComboBox* path_brush_choice_ = nullptr;
	wxComboBox* campfire_brush_choice_ = nullptr;
	wxComboBox* cave_entrance_brush_choice_ = nullptr;
	wxComboBox* cave_exit_brush_choice_ = nullptr;
	wxComboBox* cave_ground_brush_choice_ = nullptr;
	wxComboBox* cave_wall_brush_choice_ = nullptr;
    wxComboBox* cave_door_brush_choice_ = nullptr;
    wxCheckBox* cave_far_exits_check_ = nullptr;
    wxCheckBox* cave_door_corridors_check_ = nullptr;
    wxStaticText* cave_door_hint_ = nullptr;
	wxChoice* cave_entrance_style_choice_ = nullptr;
	wxSpinCtrl* building_density_spin_ = nullptr;
	wxComboBox* floor_brush_choice_ = nullptr;
	wxCheckBox* walls_check_ = nullptr;
	wxComboBox* wall_brush_choice_ = nullptr;
	wxCheckBox* doors_check_ = nullptr;
	wxCheckBox* rock_passes_check_ = nullptr;
	wxCheckBox* smooth_beaches_check_ = nullptr;
	wxCheckBox* repair_check_ = nullptr;
    wxComboBox* bridge_brush_choice_ = nullptr;
	wxChoice* door_type_choice_ = nullptr;
	wxSpinCtrl* path_length_spin_ = nullptr;
	wxSpinCtrl* ford_length_spin_ = nullptr;
	wxSpinCtrl* ruin_percent_spin_ = nullptr;
	// Building layout of the chosen learned profile (SurfaceTuning::ruinFloorPercent, compoundPercent); 0 = as built in.
	uint32_t learned_ruin_floor_ = 0, learned_compound_ = 0;
	wxSpinCtrl* camp_count_spin_ = nullptr;
	wxSpinCtrl* cave_entrances_spin_ = nullptr;
    wxSpinCtrl *plateau_levels_spin_ = nullptr, *plateau_percent_spin_ = nullptr;
    wxSpinCtrl* mountain_passes_spin_ = nullptr;
    wxCheckBox* building_storeys_check_ = nullptr;
    wxSpinCtrl* upper_floor_percent_spin_ = nullptr;
    wxComboBox* building_stairs_choice_ = nullptr;
    wxStaticText* building_storeys_hint_ = nullptr;
    wxCheckBox* building_dungeons_check_ = nullptr;
    wxSpinCtrl* dungeon_percent_spin_ = nullptr;
    wxComboBox *dungeon_floor_choice_ = nullptr, *dungeon_wall_choice_ = nullptr, *dungeon_stairs_choice_ = nullptr,
        *dungeon_descent_choice_ = nullptr;
    wxStaticText* building_dungeons_hint_ = nullptr;
    wxComboBox *plateau_ground_choice_ = nullptr, *plateau_ramp_choice_ = nullptr, *plateau_cliff_choice_ = nullptr;
    wxCheckBox* vegetation_check_ = nullptr;
	wxArrayString ground_names_;
	wxArrayString doodad_names_;
	wxArrayString wall_names_;
	wxArrayString cave_exit_names_;
	// Cave wall choices: the wall brushes first, then the ground brushes (iteration 50).
	wxArrayString cave_wall_names_;
	wxComboBox* tree_brush_choice_ = nullptr;
	wxComboBox* bush_brush_choice_ = nullptr;
	wxComboBox* stone_brush_choice_ = nullptr;
	std::array<wxStaticText*, 5> rectangle_labels_ {};
	wxStaticText* region_label_ = nullptr;
	wxStaticText* client_label_ = nullptr;
	wxStaticText* status_text_ = nullptr;
	wxPanel* preview_panel_ = nullptr;
	wxChoice* preview_floor_choice_ = nullptr;
	wxStaticText* preview_floor_label_ = nullptr;
	wxButton* preview_button_ = nullptr;
	wxButton* validate_button_ = nullptr;
	wxButton* place_button_ = nullptr;
	wxButton* cancel_button_ = nullptr;
};

#endif // RME_UI_DIALOGS_TERRAIN_GENERATOR_DIALOG_H_
