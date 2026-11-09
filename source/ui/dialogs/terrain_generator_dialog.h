//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_DIALOGS_TERRAIN_GENERATOR_DIALOG_H_
#define RME_UI_DIALOGS_TERRAIN_GENERATOR_DIALOG_H_

#include "app/main.h"
#include "procedural/adapter/generation_selection.h"
#include "procedural/adapter/style_profile_store.h"
#include "procedural/adapter/terrain_preparation.h"

#include <array>
#include <cstdint>
#include <optional>
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
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;

// Modal procedural terrain generator for the map that was current when it opened. Preview and
// Validate never change the map or history; "Place on map" commits the owned preview as one Undo
// step through DrawOperations and closes the dialog with wxID_OK. Every command looks the current
// editor up again and refuses a different map, live session, busy selection/history, active Lua
// transaction or missing client. Preview advances in slices on the GUI thread.
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
	void OnPreview(wxCommandEvent& event);
	void OnPreviewTick(wxTimerEvent& event);
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
	wxSpinCtrl* building_density_spin_ = nullptr;
	wxComboBox* floor_brush_choice_ = nullptr;
	wxCheckBox* walls_check_ = nullptr;
	wxComboBox* wall_brush_choice_ = nullptr;
	wxCheckBox* doors_check_ = nullptr;
	wxCheckBox* rock_passes_check_ = nullptr;
	wxCheckBox* smooth_beaches_check_ = nullptr;
	wxChoice* door_type_choice_ = nullptr;
	wxSpinCtrl* path_length_spin_ = nullptr;
	wxSpinCtrl* ford_length_spin_ = nullptr;
	wxCheckBox* vegetation_check_ = nullptr;
	wxArrayString ground_names_;
	wxArrayString doodad_names_;
	wxArrayString wall_names_;
	wxComboBox* tree_brush_choice_ = nullptr;
	wxComboBox* bush_brush_choice_ = nullptr;
	wxComboBox* stone_brush_choice_ = nullptr;
	std::array<wxStaticText*, 5> rectangle_labels_ {};
	wxStaticText* region_label_ = nullptr;
	wxStaticText* client_label_ = nullptr;
	wxStaticText* status_text_ = nullptr;
	wxPanel* preview_panel_ = nullptr;
	wxButton* preview_button_ = nullptr;
	wxButton* validate_button_ = nullptr;
	wxButton* place_button_ = nullptr;
	wxButton* cancel_button_ = nullptr;
};

#endif // RME_UI_DIALOGS_TERRAIN_GENERATOR_DIALOG_H_
