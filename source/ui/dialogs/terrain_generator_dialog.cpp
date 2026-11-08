//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/dialogs/terrain_generator_dialog.h"

#include "app/managers/version_manager.h"
#include "app/settings.h"
#include "brushes/brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/ground/ground_brush.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/operations/draw_operations.h"
#include "editor/selection.h"
#include "live/live_manager.h"
#include "lua/lua_api.h"
#include "map/map.h"
#include "procedural/adapter/terrain_commit.h"
#include "procedural/adapter/terrain_target.h"
#include "rendering/drawers/tiles/tile_color_calculator.h"
#include "ui/gui.h"
#include "ui/theme.h"
#include "util/common.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <wx/dcmemory.h>
#include <format>
#include <new>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <variant>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/dcclient.h>
#include <wx/image.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

namespace {

using namespace rme::procedural;

constexpr uint64_t MiB = 1024ULL * 1024ULL;
constexpr int maxSide = 500;
constexpr PreparationBudget preparationBudget { 250000, 254016, 250000, 512 * MiB };
constexpr SelectionMaskBudget selectionBudget { 250000, 1 * MiB };
constexpr TerrainCommitBudget commitBudget { 250000, 768 * MiB };
// Native bitmap conversion allowance on top of the RGB image (32-bit DIB plus headers). Policy estimate.
constexpr uint64_t bitmapOverheadBytes = 64 * 1024;

const wxString staleMessage = "Map, selection or client data changed. Generate a new preview.";
const wxString memoryMessage = "Not enough memory for this preview. Nothing was changed; try a smaller area.";
const wxString tooLargeMessage = "This area is too large. Use at most 500 x 500 tiles with up to 250,000 changes.";
const wxString cancelledMessage = "The operation was cancelled. Nothing was changed.";
const wxString noClientMessage = "No client is loaded. Load the client for this map first.";
const wxString busyMessage = "Finish the current selection or edit first, then try again.";

wxString describe(TerrainTargetError error) {
	switch (error) {
		case TerrainTargetError::WrongThread:
			return "This action must be started from the editor window.";
		case TerrainTargetError::Cancelled:
			return cancelledMessage;
		case TerrainTargetError::NoEditor:
			return "No map is open. Switch to the map this generator was opened for.";
		case TerrainTargetError::WrongMap:
			return "This generator belongs to another map. Switch back to that map or reopen the generator.";
		case TerrainTargetError::LiveSession:
			return "Terrain generation is not available during a live session.";
		case TerrainTargetError::BusySelection:
			return busyMessage;
	}
	return staleMessage;
}

wxString describe(PreparedValidationError error) {
	switch (error) {
		case PreparedValidationError::WrongThread:
			return "This action must be started from the editor window.";
		case PreparedValidationError::Cancelled:
			return cancelledMessage;
		case PreparedValidationError::NoClient:
			return noClientMessage;
		case PreparedValidationError::StaleAssets:
		case PreparedValidationError::StaleRequest:
		case PreparedValidationError::StaleMap:
		case PreparedValidationError::InvalidPrepared:
			return staleMessage;
	}
	return staleMessage;
}

wxString describe(SelectionMaskError error) {
	switch (error) {
		case SelectionMaskError::WrongThread:
			return "This action must be started from the editor window.";
		case SelectionMaskError::Busy:
			return busyMessage;
		case SelectionMaskError::Empty:
			return "Nothing is selected. Select tiles on the map or use an empty rectangle.";
		case SelectionMaskError::InvalidSelection:
			return "The selection could not be read. Select the area again.";
		case SelectionMaskError::MultipleFloors:
			return "The selection spans several floors. Select tiles on one floor only.";
		case SelectionMaskError::CellLimit:
		case SelectionMaskError::MemoryLimit:
			return "The selection is too large. Select an area of at most 500 x 500 tiles.";
		case SelectionMaskError::Cancelled:
			return cancelledMessage;
		case SelectionMaskError::AllocationFailed:
			return memoryMessage;
		case SelectionMaskError::StaleSelection:
			return staleMessage;
	}
	return staleMessage;
}

wxString describe(PreparationInputError error) {
	switch (error) {
		case PreparationInputError::WrongThread:
			return "This action must be started from the editor window.";
		case PreparationInputError::NoClient:
			return noClientMessage;
		case PreparationInputError::InvalidProfile:
			return "Each brush name must be between 1 and 256 characters.";
		case PreparationInputError::MemoryLimit:
			return "This area needs more memory than the 512 MiB preview limit. Choose a smaller area.";
		case PreparationInputError::ContextLimit:
		case PreparationInputError::ChangeLimit:
			return tooLargeMessage;
		case PreparationInputError::Cancelled:
			return cancelledMessage;
		case PreparationInputError::InvalidParameters:
			return "The surface settings are not valid.";
		case PreparationInputError::AllocationFailed:
			return memoryMessage;
	}
	return staleMessage;
}

wxString describe(SnapshotError error) {
	switch (error) {
		case SnapshotError::WrongThread:
			return "This action must be started from the editor window.";
		case SnapshotError::InvalidInput:
			return "This area cannot be used. Make sure it lies inside the map and contains empty tiles.";
		case SnapshotError::Limit:
		case SnapshotError::Overflow:
			return tooLargeMessage;
		case SnapshotError::Unsupported:
			return "Some tiles around this area hold data the generator cannot copy safely. Choose a different area.";
		case SnapshotError::Cancelled:
			return cancelledMessage;
		case SnapshotError::AllocationFailed:
			return memoryMessage;
	}
	return staleMessage;
}

wxString describe(GenerationError error) {
	switch (error) {
		case GenerationError::InvalidRegion:
			return "The area lies outside the map. Adjust X, Y, Z, width or height.";
		case GenerationError::InvalidWeights:
			return "At least one terrain weight must be above zero.";
		case GenerationError::InvalidMask:
			return "The selection could not be used. Select the area again.";
		case GenerationError::NoWritableCells:
			return "The area has no empty tiles. Only empty tiles are changed.";
		case GenerationError::CellLimit:
		case GenerationError::OutputLimit:
		case GenerationError::SizeOverflow:
			return tooLargeMessage;
		case GenerationError::Cancelled:
			return cancelledMessage;
		case GenerationError::AllocationFailed:
			return memoryMessage;
	}
	return staleMessage;
}

wxString describe(ResolveError error) {
	switch (error) {
		case ResolveError::WrongThread:
			return "This action must be started from the editor window.";
		case ResolveError::NoClient:
			return noClientMessage;
		case ResolveError::StaleAssets:
		case ResolveError::InvalidInput:
			return staleMessage;
		case ResolveError::InvalidProfile:
			return "A brush name does not match a usable ground brush in the loaded client. Check the brush names.";
		case ResolveError::InvalidItem:
			return "A ground brush uses items the loaded client does not provide. Choose different brushes.";
		case ResolveError::PlanLimit:
			return "This area needs more memory than the 512 MiB preview limit. Choose a smaller area.";
		case ResolveError::Cancelled:
			return cancelledMessage;
		case ResolveError::AllocationFailed:
			return memoryMessage;
	}
	return staleMessage;
}

wxString describe(PreviewError error) {
	switch (error) {
		case PreviewError::WrongThread:
			return "This action must be started from the editor window.";
		case PreviewError::NoClient:
			return noClientMessage;
		case PreviewError::StaleSnapshot:
		case PreviewError::InvalidPlan:
			return staleMessage;
		case PreviewError::InvalidItem:
			return "A ground brush uses items the loaded client does not provide. Choose different brushes.";
		case PreviewError::Limit:
			return tooLargeMessage;
		case PreviewError::OutOfMask:
			return "Borders would extend outside the allowed area; choose a different area or profile.";
		case PreviewError::Cancelled:
			return cancelledMessage;
		case PreviewError::AllocationFailed:
			return memoryMessage;
	}
	return staleMessage;
}

wxString describe(TerrainCommitFailure error) {
	switch (error) {
		case TerrainCommitFailure::MemoryLimit:
			return "Placing needs more memory than the 768 MiB limit. Choose a smaller area.";
		case TerrainCommitFailure::ChangeLimit:
			return tooLargeMessage;
		case TerrainCommitFailure::BusyHistory:
			return "The editor is busy applying another change. Try again.";
		case TerrainCommitFailure::ActiveLuaTransaction:
			return "A script is changing the map. Wait for it to finish, then try again.";
		case TerrainCommitFailure::Cancelled:
			return cancelledMessage;
		case TerrainCommitFailure::AllocationFailed:
			return "Not enough memory to place the preview. Try a smaller area.";
		case TerrainCommitFailure::HistoryRefused:
			return wxstr(std::format("Undo history refused this change. Current limits: {} MiB / {} actions. "
				"For a large area, choose a smaller area or adjust Undo limits in File > Preferences > General, "
				"then generate a new preview.", g_settings.getInteger(Config::UNDO_MEM_SIZE),
				g_settings.getInteger(Config::UNDO_SIZE)));
	}
	return staleMessage;
}

template <typename... Alternatives>
wxString describe(const std::variant<Alternatives...>& error) {
	return std::visit([](const auto& alternative) { return describe(alternative); }, error);
}

wxString fromView(std::string_view text) {
	return wxString::FromUTF8(text.data(), text.size());
}

std::string formatMiB(uint64_t bytes) {
	return std::format("{:.1f} MiB", double(bytes) / double(MiB));
}

} // namespace

TerrainGeneratorDialog::TerrainGeneratorDialog(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Generate Terrain", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
	if (Editor* editor = g_gui.GetCurrentEditor()) {
		source_map_generation_ = editor->map.getGeneration();
		has_source_map_ = true;
	}

	SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	SetForegroundColour(Theme::Get(Theme::Role::Text));
	SetFont(Theme::GetFont());

	const int gap = Theme::Grid(2);
	const int margin = Theme::Grid(4);
	const wxColour fieldBackground = Theme::Get(Theme::Role::Background);
	const wxColour fieldText = Theme::Get(Theme::Role::Text);

	const auto label = [this](const wxString& text) {
		auto* control = newd wxStaticText(this, wxID_ANY, text);
		control->SetForegroundColour(Theme::Get(Theme::Role::Text));
		return control;
	};
	const auto spin = [&](const wxString& name, int min, int max, int value) {
		auto* control = newd wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(Theme::Grid(24), -1), wxSP_ARROW_KEYS, min, max, value);
		control->SetName(name);
		control->SetBackgroundColour(fieldBackground);
		control->SetForegroundColour(fieldText);
		return control;
	};
	const auto text = [&](const wxString& name, const wxString& value) {
		auto* control = newd wxTextCtrl(this, wxID_ANY, value);
		control->SetName(name);
		control->SetBackgroundColour(fieldBackground);
		control->SetForegroundColour(fieldText);
		return control;
	};

	// A read-only list of the brushes the loaded client offers, so a name can only be picked.
	// Ground brushes feed the terrain fields, doodad brushes the vegetation fields.
	{
		std::string previousGround, previousDoodad;
		for (const auto& [brushName, brush] : g_brushes.getMap()) {
			if (brushName.empty()) continue;
			if (brush->is<GroundBrush>()) {
				if (brushName == previousGround) continue;
				previousGround = brushName;
				ground_names_.Add(wxstr(brushName));
			} else if (const auto* doodad = brush->as<DoodadBrush>()) {
				// The resolver only places single items of variation 0.
				if (brushName == previousDoodad || !doodad->hasSingleObjects(0)) continue;
				previousDoodad = brushName;
				doodad_names_.Add(wxstr(brushName));
			}
		}
	}
	const auto brushChoice = [&](const wxString& name, const wxString& preferred, bool doodad) {
		const wxArrayString& names = doodad ? doodad_names_ : ground_names_;
		// Only the preferred (or first) entry exists until the list is first opened: filling
		// thousands of entries into eight lists made opening the dialog slow.
		wxString initial = names.Index(preferred) != wxNOT_FOUND ? preferred : (names.IsEmpty() ? wxString() : names[0]);
		auto* control = newd wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
		control->SetName(name);
		control->SetBackgroundColour(fieldBackground);
		control->SetForegroundColour(fieldText);
		if (!initial.empty()) {
			control->Append(initial);
			control->SetSelection(0);
		}
		control->Bind(wxEVT_COMBOBOX_DROPDOWN, [control, &names](wxCommandEvent& event) {
			event.Skip();
			if (control->GetCount() > 1 || names.size() < 2) return;
			const wxString current = control->GetValue();
			control->Freeze();
			control->Clear();
			control->Append(names);
			control->SetStringSelection(current);
			control->Thaw();
		});
		return control;
	};

	// Inputs, in tab order.
	auto* form = newd wxFlexGridSizer(2, gap, Theme::Grid(3));
	form->AddGrowableCol(1, 1);

	form->Add(label("&Area:"), 0, wxALIGN_CENTER_VERTICAL);
	scope_choice_ = newd wxChoice(this, wxID_ANY);
	scope_choice_->SetName("terrain-scope");
	scope_choice_->Append("Empty rectangle");
	scope_choice_->Append("Current selection");
	scope_choice_->SetSelection(0);
	form->Add(scope_choice_, 1, wxEXPAND);

	rectangle_labels_[0] = label("&X:");
	form->Add(rectangle_labels_[0], 0, wxALIGN_CENTER_VERTICAL);
	x_spin_ = spin("terrain-x", 0, 65535, 100);
	form->Add(x_spin_, 1, wxEXPAND);

	rectangle_labels_[1] = label("&Y:");
	form->Add(rectangle_labels_[1], 0, wxALIGN_CENTER_VERTICAL);
	y_spin_ = spin("terrain-y", 0, 65535, 100);
	form->Add(y_spin_, 1, wxEXPAND);

	rectangle_labels_[2] = label("&Z (floor):");
	form->Add(rectangle_labels_[2], 0, wxALIGN_CENTER_VERTICAL);
	z_spin_ = spin("terrain-z", 0, 15, 7);
	form->Add(z_spin_, 1, wxEXPAND);

	rectangle_labels_[3] = label("&Width:");
	form->Add(rectangle_labels_[3], 0, wxALIGN_CENTER_VERTICAL);
	width_spin_ = spin("terrain-width", 1, maxSide, 100);
	form->Add(width_spin_, 1, wxEXPAND);

	rectangle_labels_[4] = label("&Height:");
	form->Add(rectangle_labels_[4], 0, wxALIGN_CENTER_VERTICAL);
	height_spin_ = spin("terrain-height", 1, maxSide, 100);
	form->Add(height_spin_, 1, wxEXPAND);

	form->Add(label("&Seed:"), 0, wxALIGN_CENTER_VERTICAL);
	seed_text_ = text("terrain-seed", "1729");
	form->Add(seed_text_, 1, wxEXPAND);

	form->Add(label("&Grass weight:"), 0, wxALIGN_CENTER_VERTICAL);
	grass_weight_spin_ = spin("terrain-grass-weight", 0, 1000, 3);
	form->Add(grass_weight_spin_, 1, wxEXPAND);

	form->Add(label("&Dirt weight:"), 0, wxALIGN_CENTER_VERTICAL);
	dirt_weight_spin_ = spin("terrain-dirt-weight", 0, 1000, 2);
	form->Add(dirt_weight_spin_, 1, wxEXPAND);

	form->Add(label("Water w&eight:"), 0, wxALIGN_CENTER_VERTICAL);
	water_weight_spin_ = spin("terrain-water-weight", 0, 1000, 1);
	form->Add(water_weight_spin_, 1, wxEXPAND);

	form->Add(label("G&rass brush:"), 0, wxALIGN_CENTER_VERTICAL);
	grass_brush_choice_ = brushChoice("terrain-grass-brush", "grass", false);
	form->Add(grass_brush_choice_, 1, wxEXPAND);

	form->Add(label("D&irt brush:"), 0, wxALIGN_CENTER_VERTICAL);
	dirt_brush_choice_ = brushChoice("terrain-dirt-brush", "dirt", false);
	form->Add(dirt_brush_choice_, 1, wxEXPAND);

	form->Add(label("Wa&ter brush:"), 0, wxALIGN_CENTER_VERTICAL);
	water_brush_choice_ = brushChoice("terrain-water-brush", "clear water", false);
	form->Add(water_brush_choice_, 1, wxEXPAND);

	form->Add(label("Terrain &style:"), 0, wxALIGN_CENTER_VERTICAL);
	style_choice_ = newd wxChoice(this, wxID_ANY);
	style_choice_->SetName("terrain-style");
	style_choice_->Append("Classic (random mix)");
	style_choice_->Append("Natural surface (lakes, rivers, rock)");
	style_choice_->SetSelection(0);
	form->Add(style_choice_, 1, wxEXPAND);

	form->Add(label("Style &profile:"), 0, wxALIGN_CENTER_VERTICAL);
	profile_choice_ = newd wxChoice(this, wxID_ANY);
	profile_choice_->SetName("terrain-profile");
	profile_choice_->Append("Built-in (manual settings)");
	profile_choice_->SetSelection(0);
	LoadStyleProfiles();
	form->Add(profile_choice_, 1, wxEXPAND);

	form->Add(label("Roc&k brush:"), 0, wxALIGN_CENTER_VERTICAL);
	rock_brush_choice_ = brushChoice("terrain-rock-brush", "rocky ground", false);
	form->Add(rock_brush_choice_, 1, wxEXPAND);

	form->Add(label("S&hore brush:"), 0, wxALIGN_CENTER_VERTICAL);
	shore_brush_choice_ = brushChoice("terrain-shore-brush", "sand", false);
	form->Add(shore_brush_choice_, 1, wxEXPAND);

	form->Add(label("Pat&h length:"), 0, wxALIGN_CENTER_VERTICAL);
	path_length_spin_ = spin("terrain-path-length", 0, 400, 0);
	path_length_spin_->SetToolTip("Winding paths over grass and dirt; skeleton cells per 65536 map cells. 0 = no paths. About 80 is similar to the reference maps.");
	form->Add(path_length_spin_, 1, wxEXPAND);

	form->Add(label("Path brus&h:"), 0, wxALIGN_CENTER_VERTICAL);
	path_brush_choice_ = brushChoice("terrain-path-brush", "cobblestone", false);
	form->Add(path_brush_choice_, 1, wxEXPAND);

	form->Add(label("&Buildings:"), 0, wxALIGN_CENTER_VERTICAL);
	building_density_spin_ = spin("terrain-building-density", 0, 100, 0);
	building_density_spin_->SetToolTip("Building foundations (floor rectangles, no walls) per 65536 map cells. 0 = none. About 30 is similar to the reference maps.");
	form->Add(building_density_spin_, 1, wxEXPAND);

	form->Add(label("Floor brus&h:"), 0, wxALIGN_CENTER_VERTICAL);
	floor_brush_choice_ = brushChoice("terrain-floor-brush", "stone floor", false);
	form->Add(floor_brush_choice_, 1, wxEXPAND);

	form->Add(label("Vegetation:"), 0, wxALIGN_CENTER_VERTICAL);
	vegetation_check_ = newd wxCheckBox(this, wxID_ANY, "Add trees, bushes and stones");
	vegetation_check_->SetName("terrain-vegetation");
	vegetation_check_->SetForegroundColour(Theme::Get(Theme::Role::Text));
	form->Add(vegetation_check_, 1, wxEXPAND);

	form->Add(label("Tree brush:"), 0, wxALIGN_CENTER_VERTICAL);
	tree_brush_choice_ = brushChoice("terrain-tree-brush", "trees", true);
	form->Add(tree_brush_choice_, 1, wxEXPAND);

	form->Add(label("Bush brush:"), 0, wxALIGN_CENTER_VERTICAL);
	bush_brush_choice_ = brushChoice("terrain-bush-brush", "bushes", true);
	form->Add(bush_brush_choice_, 1, wxEXPAND);

	form->Add(label("Stone brush:"), 0, wxALIGN_CENTER_VERTICAL);
	stone_brush_choice_ = brushChoice("terrain-stone-brush", "small rocks", true);
	form->Add(stone_brush_choice_, 1, wxEXPAND);
	auto* helper = label("Only empty tiles are changed. Preview leaves your map unchanged; placing adds one Undo step.");
	helper->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	helper->Wrap(Theme::Grid(64));

	client_label_ = label(wxEmptyString);
	client_label_->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));

	auto* left = newd wxBoxSizer(wxVERTICAL);
	left->Add(form, 0, wxEXPAND);
	left->AddSpacer(Theme::Grid(3));
	left->Add(helper, 0, wxEXPAND);
	left->AddSpacer(gap);
	left->Add(client_label_, 0, wxEXPAND);

	// Read-only minimap preview.
	auto* caption = label("Minimap preview");
	caption->SetFont(Theme::GetFont(9, true));
	preview_panel_ = newd wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(Theme::Grid(80), Theme::Grid(80)), wxFULL_REPAINT_ON_RESIZE);
	preview_panel_->SetCanFocus(false);
	preview_panel_->SetName("terrain-preview-image");
	preview_panel_->SetBackgroundStyle(wxBG_STYLE_PAINT);
	preview_panel_->SetMinSize(wxSize(Theme::Grid(60), Theme::Grid(60)));
	preview_panel_->Bind(wxEVT_PAINT, &TerrainGeneratorDialog::OnPaintPreview, this);

	auto* right = newd wxBoxSizer(wxVERTICAL);
	right->Add(caption, 0, wxBOTTOM, gap);
	region_label_ = newd wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
		wxSize(-1, Theme::Grid(8)), wxST_NO_AUTORESIZE);
	region_label_->SetName("terrain-region");
	region_label_->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	region_label_->SetMinSize(wxSize(Theme::Grid(60), Theme::Grid(8)));
	right->Add(region_label_, 0, wxEXPAND | wxBOTTOM, gap);
	right->Add(preview_panel_, 1, wxEXPAND);

	auto* body = newd wxBoxSizer(wxHORIZONTAL);
	body->Add(left, 0, wxEXPAND | wxRIGHT, margin);
	body->Add(right, 1, wxEXPAND);

	// Status with a stable height so the layout does not jump between messages.
	status_text_ = newd wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, Theme::Grid(16)), wxST_NO_AUTORESIZE);
	status_text_->SetName("terrain-status");
	status_text_->SetMinSize(wxSize(Theme::Grid(60), Theme::Grid(16)));

	preview_button_ = newd wxButton(this, wxID_ANY, "&Preview");
	preview_button_->SetName("terrain-preview");
	validate_button_ = newd wxButton(this, wxID_ANY, "&Validate");
	validate_button_->SetName("terrain-validate");
	place_button_ = newd wxButton(this, wxID_APPLY, "Place &on map");
	place_button_->SetName("terrain-place");
	cancel_button_ = newd wxButton(this, wxID_CANCEL, "&Cancel");
	cancel_button_->SetName("terrain-cancel");
	preview_button_->SetDefault();

	auto* buttons = newd wxBoxSizer(wxHORIZONTAL);
	buttons->AddStretchSpacer(1);
	buttons->Add(preview_button_, 0, wxRIGHT, gap);
	buttons->Add(validate_button_, 0, wxRIGHT, gap);
	buttons->Add(place_button_, 0, wxRIGHT, gap);
	buttons->Add(cancel_button_, 0);

	auto* root = newd wxBoxSizer(wxVERTICAL);
	root->Add(body, 1, wxEXPAND | wxALL, margin);
	root->Add(status_text_, 0, wxEXPAND | wxLEFT | wxRIGHT, margin);
	root->Add(buttons, 0, wxEXPAND | wxALL, margin);
	SetSizerAndFit(root);
	wxSize minimum = GetSize();
	minimum.IncTo(wxSize(Theme::Grid(180), Theme::Grid(130)));
	SetMinSize(minimum);
	wxSize preferred = minimum;
	preferred.IncTo(wxSize(Theme::Grid(220), Theme::Grid(150)));
	SetSize(preferred);
	status_text_->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
		status_text_->SetLabel(status_message_);
		status_text_->Wrap(std::max(event.GetSize().GetWidth(), Theme::Grid(60)));
		event.Skip();
	});
	// The brushes the dialog opened with: a profile choice always starts from them.
	{
		wxComboBox* const fields[] = { grass_brush_choice_, dirt_brush_choice_, rock_brush_choice_, shore_brush_choice_, water_brush_choice_ };
		for (size_t kind = 0; kind < rme::procedural::naturalKindCount; ++kind) default_brushes_[kind] = fields[kind]->GetValue();		wxComboBox* const decorFields[] = { tree_brush_choice_, bush_brush_choice_, stone_brush_choice_ };
		for (size_t kind = 0; kind < std::size(decorFields); ++kind) default_decor_brushes_[kind] = decorFields[kind]->GetValue();
	}
	CentreOnParent();

	// Every input change releases the owned preview.
	for (wxSpinCtrl* control : { x_spin_, y_spin_, z_spin_, width_spin_, height_spin_, grass_weight_spin_, dirt_weight_spin_, water_weight_spin_, path_length_spin_, building_density_spin_ }) {
		control->Bind(wxEVT_SPINCTRL, &TerrainGeneratorDialog::OnInputChanged, this);
		control->Bind(wxEVT_TEXT, &TerrainGeneratorDialog::OnInputChanged, this);
	}
	seed_text_->Bind(wxEVT_TEXT, &TerrainGeneratorDialog::OnInputChanged, this);
	for (wxComboBox* control : { grass_brush_choice_, dirt_brush_choice_, water_brush_choice_, rock_brush_choice_, shore_brush_choice_, path_brush_choice_, floor_brush_choice_, tree_brush_choice_, bush_brush_choice_, stone_brush_choice_ }) {
		control->Bind(wxEVT_COMBOBOX, &TerrainGeneratorDialog::OnInputChanged, this);
	}
	scope_choice_->Bind(wxEVT_CHOICE, &TerrainGeneratorDialog::OnInputChanged, this);
	style_choice_->Bind(wxEVT_CHOICE, &TerrainGeneratorDialog::OnInputChanged, this);
	profile_choice_->Bind(wxEVT_CHOICE, &TerrainGeneratorDialog::OnProfileChosen, this);
	vegetation_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
	preview_button_->Bind(wxEVT_BUTTON, &TerrainGeneratorDialog::OnPreview, this);
	validate_button_->Bind(wxEVT_BUTTON, &TerrainGeneratorDialog::OnValidate, this);
	place_button_->Bind(wxEVT_BUTTON, &TerrainGeneratorDialog::OnPlace, this);

	Bind(wxEVT_TIMER, &TerrainGeneratorDialog::OnPreviewTick, this, preview_timer_.GetId());
	Bind(wxEVT_BUTTON, &TerrainGeneratorDialog::OnCancel, this, wxID_CANCEL);
	Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
		ReleasePreview();
		event.Skip();
	});

	UpdateClientLabel();
	UpdateControlStates();
	SetStatus(has_source_map_ ? wxString("Choose an area and press Preview.") : describe(TerrainTargetError::NoEditor), !has_source_map_);
	scope_choice_->SetFocus();
}

bool TerrainGeneratorDialog::SelectionMode() const {
	return scope_choice_->GetSelection() == 1;
}

bool TerrainGeneratorDialog::NaturalSurface() const {
	return style_choice_->GetSelection() == 1;
}

// Offers the style profiles that were learned for exactly the loaded client and its assets. Read-only: the
// folder is never created and a profile that does not match is not listed, only counted in the tooltip.
void TerrainGeneratorDialog::LoadStyleProfiles() {
	namespace rp = rme::procedural;
	const auto folder = rp::styleProfileDirectory();
	const auto ids = rp::loadedClientAssetIds();
	wxString hint;
	if (!ids) {
		hint = "Style profiles need a loaded client with items.otb.";
	} else {
		auto scan = rp::scanStyleProfiles(folder, *ids);
		const wxString where(folder.generic_wstring());
		if (!scan.exists) {
			hint = "No style profiles. Put *.profile files in:\n" + where;
		} else {
			profiles_ = std::move(scan.offer.offered);
			for (const auto& entry : profiles_) profile_choice_->Append(wxstr(entry.name));
			hint = wxstr(std::format("{} style profile(s) offered from:\n", profiles_.size())) + where;
			const size_t hidden = scan.offer.otherClient + scan.offer.otherAssets + scan.offer.notUsable + scan.unreadable;
			if (hidden > 0) {
				hint += wxstr(std::format("\nHidden: {} for another client, {} for other items.otb or Tibia.dat, {} unusable.",
					scan.offer.otherClient, scan.offer.otherAssets, scan.offer.notUsable + scan.unreadable));
			}
		}
	}
	profile_choice_->SetToolTip(hint);
}

void TerrainGeneratorDialog::OnInputChanged(wxCommandEvent& event) {
	event.Skip();
	const bool hadPreview = preview_steps_ || prepared_ || preview_bitmap_.IsOk();
	ReleasePreview();
	UpdateControlStates();
	if (hadPreview) {
		SetStatus("Settings changed. Press Preview to generate a new preview.", false);
	}
}

// A profile also names the ground and doodad brushes its style used (winter: small snowy rocks). Every choice
// starts from the brushes the dialog opened with, so brushes of a previously chosen profile do not carry over;
// then the profile's brushes are set. A name is applied only when this client has the brush and the generator
// can use it (a ground brush needs ground items, a doodad brush single items). Anything else is skipped and
// reported, never guessed. The fields stay editable; the vegetation check box is left as the user set it.
void TerrainGeneratorDialog::OnProfileChosen(wxCommandEvent& event) {
	OnInputChanged(event);
	wxComboBox* const groundFields[] = { grass_brush_choice_, dirt_brush_choice_, rock_brush_choice_, shore_brush_choice_, water_brush_choice_ };
	wxComboBox* const decorFields[] = { tree_brush_choice_, bush_brush_choice_, stone_brush_choice_ };
	static_assert(std::size(groundFields) == rme::procedural::naturalKindCount);
	static const char* const groundLabels[] = { "Grass", "Dirt", "Rock", "Shore", "Water" };
	static const char* const decorLabels[] = { "Tree", "Bush", "Stone" };
	const auto setBrush = [](wxComboBox* field, const wxString& name) {
		field->Clear();
		field->Append(name);
		field->SetSelection(0);
	};
	for (size_t i = 0; i < std::size(groundFields); ++i) if (!default_brushes_[i].empty()) setBrush(groundFields[i], default_brushes_[i]);
	for (size_t i = 0; i < std::size(decorFields); ++i) if (!default_decor_brushes_[i].empty()) setBrush(decorFields[i], default_decor_brushes_[i]);
	const int picked = profile_choice_->GetSelection();
	if (picked <= 0 || static_cast<size_t>(picked) > profiles_.size()) {
		SetStatus("Built-in settings: brushes set back to the defaults.", false);
		return;
	}
	const auto& entry = profiles_[static_cast<size_t>(picked) - 1];
	wxString applied, skipped;
	const auto note = [](wxString& list, const char* label, const wxString& name) {
		list += (list.empty() ? "" : ", ") + wxString(label) + ": " + name;
	};
	for (size_t kind = 0; kind < rme::procedural::naturalKindCount; ++kind) {
		const std::string& name = entry.profile.palette[kind];
		if (name.empty()) continue;
		const wxString wanted = wxstr(name);
		const Brush* brush = ground_names_.Index(wanted) == wxNOT_FOUND ? nullptr : g_brushes.getBrush(name);
		const auto* ground = brush ? brush->as<GroundBrush>() : nullptr;
		const bool usable = ground && ground->getTotalChance() > 0;
		if (usable) setBrush(groundFields[kind], wanted);
		note(usable ? applied : skipped, groundLabels[kind], wanted);
	}
	for (size_t kind = 0; kind < std::size(decorFields); ++kind) {
		const std::string& name = entry.profile.decorPalette[kind];
		if (name.empty()) continue;
		const wxString wanted = wxstr(name);
		const Brush* brush = doodad_names_.Index(wanted) == wxNOT_FOUND ? nullptr : g_brushes.getBrush(name);
		const auto* doodad = brush ? brush->as<DoodadBrush>() : nullptr;
		const bool usable = doodad && doodad->hasSingleObjects(0);
		if (usable) setBrush(decorFields[kind], wanted);
		note(usable ? applied : skipped, decorLabels[kind], wanted);
	}
	wxString message = "Style profile \"" + wxstr(entry.name) + "\": ";
	message += applied.empty() ? wxString("tuning only, brushes at their defaults.") : "brushes set (" + applied + ").";
	if (!skipped.empty()) message += " Not usable in this client, kept the default: " + skipped + ".";
	SetStatus(message, false);
}

void TerrainGeneratorDialog::ReleasePreview() {
	preview_timer_.Stop();
	preview_steps_.reset();
	pending_request_.reset();
	prepared_.reset();
	selection_.reset();
	preview_bitmap_ = wxNullBitmap;
	validate_button_->Disable();
	place_button_->Disable();
	preview_panel_->Refresh();
}

void TerrainGeneratorDialog::UpdateControlStates() {
	const bool natural = NaturalSurface();
	for (wxSpinCtrl* control : { grass_weight_spin_, dirt_weight_spin_, water_weight_spin_ }) control->Enable(!natural);
	for (wxComboBox* control : { rock_brush_choice_, shore_brush_choice_ }) control->Enable(natural);
	path_length_spin_->Enable(natural);
	building_density_spin_->Enable(natural);
	path_brush_choice_->Enable(natural && (path_length_spin_->GetValue() > 0 || building_density_spin_->GetValue() > 0));
	floor_brush_choice_->Enable(natural && building_density_spin_->GetValue() > 0);
	vegetation_check_->Enable(natural);
	profile_choice_->Enable(natural);
	for (wxComboBox* control : { tree_brush_choice_, bush_brush_choice_, stone_brush_choice_ }) control->Enable(natural && vegetation_check_->GetValue());
	const bool rectangle = !SelectionMode();
	for (wxSpinCtrl* control : { x_spin_, y_spin_, z_spin_, width_spin_, height_spin_ }) {
		control->Enable(rectangle);
		control->Show(rectangle);
	}
	for (auto* label : rectangle_labels_) label->Show(rectangle);
	if (rectangle) {
		region_label_->SetLabel("Rectangle coordinates are shown on the left.");
	} else if (selection_ && prepared_) {
		const auto& region = selection_->region;
		region_label_->SetLabel(wxstr(std::format("Selection: {} x {} at ({}, {}, {})\n{} selected tiles.",
			region.width, region.height, region.x, region.y, region.z, selection_->selectedCells)));
	} else {
		region_label_->SetLabel("Selection: press Preview to capture the current area.");
	}
	Layout();
	preview_button_->Enable(!preview_steps_);
	validate_button_->Enable(prepared_.has_value());
	place_button_->Enable(prepared_ && !prepared_->preview.getChanges().empty());
}

void TerrainGeneratorDialog::UpdateClientLabel() {
	if (g_version.IsVersionLoaded() && g_version.getLoadedVersion()) {
		client_label_->SetLabel("Client: " + fromView(g_version.GetCurrentVersionIDView()));
	} else {
		client_label_->SetLabel("No client loaded");
	}
}

void TerrainGeneratorDialog::SetStatus(const wxString& text, bool error) {
	if (error) {
		spdlog::warn("Terrain generator: {}", text.utf8_string());
	} else {
		spdlog::info("Terrain generator: {}", text.utf8_string());
	}
	status_text_->SetForegroundColour(Theme::Get(error ? Theme::Role::Error : Theme::Role::Text));
	status_message_ = text;
	status_text_->SetLabel(status_message_);
	status_text_->Wrap(std::max(status_text_->GetClientSize().GetWidth(), Theme::Grid(60)));
}

std::optional<TerrainGeneratorDialog::Request> TerrainGeneratorDialog::ReadRequest() try {
	Request request;
	auto& config = request.config;

	const std::string seed = seed_text_->GetValue().ToStdString();
	uint64_t value = 0;
	const char* const begin = seed.data();
	const char* const end = begin + seed.size();
	const auto [parsed, error] = std::from_chars(begin, end, value, 10);
	if (seed.empty() || seed.front() < '0' || seed.front() > '9' || error != std::errc() || parsed != end) {
		SetStatus("Enter the seed as a whole number from 0 to 18446744073709551615.", true);
		seed_text_->SetFocus();
		return std::nullopt;
	}
	config.seed = value;

	const bool natural = NaturalSurface();
	if (natural) {
		// Weights belong to the classic mix only; the surface algorithm keeps the defaults.
		config.algorithm = GenerationAlgorithm::Surface;
		// A style profile replaces the built-in tuning; the choice was listed only for this client and these assets.
		if (const int picked = profile_choice_->GetSelection(); picked > 0 && static_cast<size_t>(picked) <= profiles_.size()) {
			const auto& profile = profiles_[static_cast<size_t>(picked) - 1].profile;
			config.surface = *profile.tuning;
			if (profile.decorTuning) config.decor = *profile.decorTuning;
		}
		config.surface.pathLength = uint32_t(path_length_spin_->GetValue()); // not part of a style profile
		config.surface.buildingDensity = uint32_t(building_density_spin_->GetValue());
	} else {
		config.grassWeight = uint32_t(grass_weight_spin_->GetValue());
		config.dirtWeight = uint32_t(dirt_weight_spin_->GetValue());
		config.waterWeight = uint32_t(water_weight_spin_->GetValue());
	}
	if (uint64_t(config.grassWeight) + config.dirtWeight + config.waterWeight == 0) {
		SetStatus("At least one terrain weight must be above zero.", true);
		grass_weight_spin_->SetFocus();
		return std::nullopt;
	}

	// Surface order: Grass, Dirt, Rock, Shore, Lake, River. Lake and river share the water brush.
	wxComboBox* const classicFields[] = { grass_brush_choice_, dirt_brush_choice_, water_brush_choice_ };
	wxComboBox* const surfaceFields[] = { grass_brush_choice_, dirt_brush_choice_, rock_brush_choice_, shore_brush_choice_, water_brush_choice_, water_brush_choice_ };
	wxComboBox* const* brushFields = natural ? surfaceFields : classicFields;
	const bool withFloors = natural && config.surface.buildingDensity > 0;
	const bool withPaths = natural && config.surface.pathLength > 0;
	request.profile.count = !natural ? GroundProfile::groundRollCount : withFloors ? GroundProfile::surfaceWithFloorCount :
		withPaths ? GroundProfile::surfaceWithPathCount : GroundProfile::surfaceCount;
	wxComboBox* const surfaceWithFloorFields[] = { grass_brush_choice_, dirt_brush_choice_, rock_brush_choice_, shore_brush_choice_, water_brush_choice_, water_brush_choice_, path_brush_choice_, floor_brush_choice_ };
	if (withPaths || withFloors) brushFields = surfaceWithFloorFields;
	for (size_t i = 0; i < request.profile.count; ++i) {
		request.profile.brushes[i] = std::string(brushFields[i]->GetValue().utf8_str());
		if (request.profile.brushes[i].empty()) {
			SetStatus("Choose a ground brush for every terrain field.", true);
			brushFields[i]->SetFocus();
			return std::nullopt;
		}
	}

	config.vegetation = natural && vegetation_check_->GetValue();
	if (config.vegetation) {
		wxComboBox* const decorFields[] = { tree_brush_choice_, bush_brush_choice_, stone_brush_choice_ };
		request.profile.decorCount = GroundProfile::decorationCount;
		for (size_t i = 0; i < GroundProfile::decorationCount; ++i) {
			request.profile.decor[i] = std::string(decorFields[i]->GetValue().utf8_str());
			if (request.profile.decor[i].empty()) {
				SetStatus("Choose a doodad brush for trees, bushes and stones.", true);
				decorFields[i]->SetFocus();
				return std::nullopt;
			}
		}
	}

	if (selection_) {
		config.region = selection_->region;
	} else {
		config.region = GenerationRegion { x_spin_->GetValue(), y_spin_->GetValue(), z_spin_->GetValue(),
			uint32_t(width_spin_->GetValue()), uint32_t(height_spin_->GetValue()) };
	}
	return request;
} catch (const std::bad_alloc&) {
	SetStatus(memoryMessage, true);
	return std::nullopt;
}

Editor* TerrainGeneratorDialog::CurrentTarget() {
	UpdateClientLabel();
	Editor* editor = g_gui.GetCurrentEditor();
	if (!has_source_map_ || !editor) {
		SetStatus(describe(TerrainTargetError::NoEditor), true);
		return nullptr;
	}
	if (editor->map.getGeneration() != source_map_generation_) {
		SetStatus(describe(TerrainTargetError::WrongMap), true);
		return nullptr;
	}
	if (editor->live_manager.IsLive()) {
		SetStatus(describe(TerrainTargetError::LiveSession), true);
		return nullptr;
	}
	if (editor->selection.isBusy() || editor->selection.isDeferred()) {
		SetStatus(busyMessage, true);
		return nullptr;
	}
	if (!editor->actionQueue || editor->actionQueue->isApplyingTerrain()) {
		SetStatus(describe(TerrainCommitFailure::BusyHistory), true);
		return nullptr;
	}
	if (LuaAPI::isTransactionActive()) {
		SetStatus(describe(TerrainCommitFailure::ActiveLuaTransaction), true);
		return nullptr;
	}
	if (!g_version.IsVersionLoaded() || !g_version.getLoadedVersion()) {
		SetStatus(noClientMessage, true);
		return nullptr;
	}
	return editor;
}

void TerrainGeneratorDialog::OnPreview(wxCommandEvent&) {
	if (preview_steps_) return;
	// At most one retained result or job.
	ReleasePreview();
	UpdateControlStates();

	Editor* editor = CurrentTarget();
	if (!editor) {
		return;
	}

	try {
		if (SelectionMode()) {
			auto captured = captureGenerationSelection(*editor, selectionBudget);
			if (!captured) {
				SetStatus(describe(captured.error()), true);
				return;
			}
			if (captured->region.width > uint32_t(maxSide) || captured->region.height > uint32_t(maxSide)) {
				SetStatus("The selection is too large. Select an area of at most 500 x 500 tiles.", true);
				return;
			}
			selection_ = std::move(*captured);
		}

		auto request = ReadRequest();
		if (!request) {
			selection_.reset();
			return;
		}
		const std::span<const uint8_t> mask = selection_ ? std::span<const uint8_t>(selection_->mask) : std::span<const uint8_t>();

		pending_request_ = std::move(*request);
		auto steps = prepareTerrainSteps(editor->map, pending_request_->config,
			pending_request_->profile, preparationBudget, mask);
		if (!steps) {
			ReleasePreview();
			SetStatus(describe(steps.error()), true);
			return;
		}
		preview_steps_.emplace(std::move(*steps));
		SetStatus("Preparing preview. Press Cancel to stop.", false);
		SchedulePreviewTick();
	} catch (const std::bad_alloc&) {
		ReleasePreview();
		SetStatus(memoryMessage, true);
	}
	UpdateControlStates();
}

void TerrainGeneratorDialog::SchedulePreviewTick() {
	if (!preview_timer_.StartOnce(1)) {
		ReleasePreview();
		SetStatus("Could not schedule the preview. Press Preview to try again.", true);
		UpdateControlStates();
	}
}

void TerrainGeneratorDialog::OnCancel(wxCommandEvent& event) {
	if (!preview_steps_) {
		event.Skip();
		return;
	}
	ReleasePreview();
	UpdateControlStates();
	SetStatus("Preview cancelled. Press Preview to try again.", false);
	preview_button_->SetFocus();
}

void TerrainGeneratorDialog::OnPreviewTick(wxTimerEvent&) {
	if (!preview_steps_) return;
	if (!CurrentTarget()) {
		ReleasePreview();
		UpdateControlStates();
		return;
	}

	try {
		auto outcome = preview_steps_->advance();
		if (!outcome) {
			SchedulePreviewTick();
			return;
		}
		preview_steps_.reset();
		auto request = std::move(pending_request_);
		pending_request_.reset();
		UpdateControlStates();
		auto& result = *outcome;
		if (!result) {
			selection_.reset();
			SetStatus(describe(result.error()), true);
			return;
		}

		// Fresh target and source validation before the result is retained or displayed.
		const auto target = selection_ ?
			validateSelectedTerrainTarget(*result, *selection_, request->config, request->profile) :
			validateTerrainTarget(*result, request->config, request->profile);
		if (!target) {
			selection_.reset();
			SetStatus(describe(target.error()), true);
			return;
		}
		const auto fingerprint = result->preview.currentFingerprint();
		if (!fingerprint || *fingerprint != result->preview.getFingerprint()) {
			selection_.reset();
			SetStatus(staleMessage, true);
			return;
		}

		prepared_ = std::move(*result);
		if (!BuildPreviewBitmap()) {
			ReleasePreview();
			SetStatus(memoryMessage, true);
			return;
		}

		const auto& region = prepared_->snapshot.input.config.region;
		const uint64_t imageBytes = uint64_t(region.width) * region.height * 7 + bitmapOverheadBytes;
		SetStatus(wxstr(std::format("Preview ready: {} tiles will change in a {} x {} area at floor {} (seed {}). "
			"Estimated memory: {} for the preview and image; placing may use up to {}.",
			prepared_->preview.getChanges().size(), region.width, region.height, region.z,
			prepared_->snapshot.input.config.seed,
			formatMiB(prepared_->memory.totalBytes + imageBytes + (selection_ ? selection_->estimatedBytes : 0)),
			formatMiB(commitBudget.maxBytes))), false);
	} catch (const std::bad_alloc&) {
		ReleasePreview();
		SetStatus(memoryMessage, true);
	}
	UpdateControlStates();
	preview_panel_->Refresh();
}

bool TerrainGeneratorDialog::BuildPreviewBitmap() {
	const auto& snapshot = prepared_->snapshot;
	const auto& region = snapshot.input.config.region;
	const auto& context = snapshot.context;
	const BaseMap& buffer = prepared_->preview.getBuffer();
	const wxColour empty = Theme::Get(Theme::Role::Background);

	wxImage image(int(region.width), int(region.height), false);
	if (!image.IsOk()) {
		return false;
	}
	unsigned char* pixel = image.GetData();
	for (uint32_t row = 0; row < region.height; ++row) {
		for (uint32_t column = 0; column < region.width; ++column) {
			const Position position(region.x + int(column), region.y + int(row), region.z);
			// Concrete preview tile where something changes, otherwise the captured original.
			const Tile* tile = buffer.getTile(position);
			if (!tile) {
				const size_t index = size_t(position.y - context.y) * context.width + size_t(position.x - context.x);
				tile = snapshot.cells[index].original.get();
			}
			uint8_t r = empty.Red(), g = empty.Green(), b = empty.Blue();
			if (tile) {
				TileColorCalculator::GetMinimapColor(tile, r, g, b);
			}
			*pixel++ = r;
			*pixel++ = g;
			*pixel++ = b;
		}
	}
	preview_bitmap_ = wxBitmap(image);
	return preview_bitmap_.IsOk();
}

void TerrainGeneratorDialog::OnValidate(wxCommandEvent&) {
	if (!prepared_) {
		return;
	}
	if (!CurrentTarget()) {
		ReleasePreview();
		UpdateControlStates();
		return;
	}
	const auto request = ReadRequest();
	if (!request) {
		ReleasePreview();
		UpdateControlStates();
		return;
	}
	const auto target = selection_ ?
		validateSelectedTerrainTarget(*prepared_, *selection_, request->config, request->profile) :
		validateTerrainTarget(*prepared_, request->config, request->profile);
	const auto fingerprint = target ? prepared_->preview.currentFingerprint() : std::nullopt;
	if (!target || !fingerprint || *fingerprint != prepared_->preview.getFingerprint()) {
		const wxString message = target ? staleMessage : describe(target.error());
		ReleasePreview();
		UpdateControlStates();
		SetStatus(message, true);
		return;
	}
	SetStatus(wxstr(std::format("Preview validated. {} tiles will change when you place it.",
		prepared_->preview.getChanges().size())), false);
}

void TerrainGeneratorDialog::OnPlace(wxCommandEvent&) {
	if (!prepared_) {
		return;
	}
	if (!CurrentTarget()) {
		ReleasePreview();
		UpdateControlStates();
		return;
	}
	const auto request = ReadRequest();
	if (!request) {
		ReleasePreview();
		UpdateControlStates();
		return;
	}
	// The commit validates again and copies the owned diff; it never regenerates or reborders.
	const auto receipt = selection_ ?
		DrawOperations::commitSelectedTerrainPreview(*prepared_, *selection_, request->config, request->profile, commitBudget) :
		DrawOperations::commitTerrainPreview(*prepared_, request->config, request->profile, commitBudget);
	if (receipt) {
		spdlog::info("Terrain generator: placed {} tiles as one undo step", prepared_->preview.getChanges().size());
		EndModal(wxID_OK);
		return;
	}
	const wxString message = describe(receipt.error());
	ReleasePreview();
	UpdateControlStates();
	SetStatus(message, true);
}

void TerrainGeneratorDialog::OnPaintPreview(wxPaintEvent&) {
	wxPaintDC dc(preview_panel_);
	const wxSize size = preview_panel_->GetClientSize();
	dc.SetPen(wxPen(Theme::Get(Theme::Role::Border)));
	dc.SetBrush(wxBrush(Theme::Get(Theme::Role::Background)));
	dc.DrawRectangle(wxPoint(0, 0), size);

	if (!preview_bitmap_.IsOk()) {
		dc.SetTextForeground(Theme::Get(Theme::Role::TextSubtle));
		dc.SetFont(Theme::GetFont());
		const wxString hint = "Press Preview to see the result here.";
		const wxSize extent = dc.GetTextExtent(hint);
		dc.DrawText(hint, (size.GetWidth() - extent.GetWidth()) / 2, (size.GetHeight() - extent.GetHeight()) / 2);
		return;
	}

	// One map tile per image pixel, scaled to fit inside the border.
	const int inset = Theme::Grid(1);
	const double availableWidth = std::max(1, size.GetWidth() - 2 * inset);
	const double availableHeight = std::max(1, size.GetHeight() - 2 * inset);
	const double scale = std::min(availableWidth / preview_bitmap_.GetWidth(), availableHeight / preview_bitmap_.GetHeight());
	const double drawnWidth = preview_bitmap_.GetWidth() * scale;
	const double drawnHeight = preview_bitmap_.GetHeight() * scale;
	const double left = (size.GetWidth() - drawnWidth) / 2.0;
	const double top = (size.GetHeight() - drawnHeight) / 2.0;
	// Keep destination margins in window pixels, including when one source pixel is enlarged.
	wxMemoryDC source;
	source.SelectObjectAsSource(preview_bitmap_);
	dc.StretchBlit(wxCoord(std::lround(left)), wxCoord(std::lround(top)),
		wxCoord(std::lround(drawnWidth)), wxCoord(std::lround(drawnHeight)), &source,
		0, 0, preview_bitmap_.GetWidth(), preview_bitmap_.GetHeight(), wxCOPY, false);
}
