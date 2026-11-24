//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/dialogs/terrain_generator_dialog.h"
#include "ui/dialogs/style_learning_dialog.h"
#include "ui/dialogs/terrain_generator_limits.h"

#include "app/managers/version_manager.h"
#include "app/settings.h"
#include "brushes/brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/operations/draw_operations.h"
#include "editor/selection.h"
#include "live/live_manager.h"
#include "lua/lua_api.h"
#include "map/map.h"
#include "procedural/adapter/door_types.h"
#include "procedural/adapter/surface_resolver.h"
#include "procedural/adapter/terrain_commit.h"
#include "procedural/adapter/terrain_target.h"
#include "rendering/drawers/tiles/tile_color_calculator.h"
#include "ui/gui.h"
#include "ui/theme.h"
#include "util/common.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <random>
#include <wx/generic/msgdlgg.h>
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
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/gauge.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>
#include "procedural/adapter/decor_source.h"
#include "procedural/core/doodad_class.h"

namespace {

// A vegetation source of a profile (doodad brush or "raw:<id>") whose first item the loaded client can place.
bool usableDecor(const std::string& name) {
	const auto source = rme::procedural::findDecorSource(name, 100000);
	return source && rme::procedural::decorItemPlaceable(source.item(1));
}

using namespace rme::procedural;

using namespace rme::ui::terrain_generator;
constexpr uint32_t mountainMinTopCells = 64;
// Native bitmap conversion allowance on top of the RGB image (32-bit DIB plus headers). Policy estimate.
constexpr uint64_t bitmapOverheadBytes = 64 * 1024;

const wxString staleMessage = "Map, selection or client data changed. Generate a new preview.";
const wxString memoryMessage = "Not enough memory for this preview. Nothing was changed; try a smaller area.";
const wxString tooLargeMessage = wxstr(std::format(
    "This area is too large. Use at most {} x {} tiles with up to {} changed tiles across all generated floors.",
    maxSide, maxSide, maxChanges
));
const wxString cancelledMessage = "The operation was cancelled. Nothing was changed.";
std::array<uint64_t, 5> candidateFingerprints(const PreparedTerrain& prepared) {
    std::array<uint64_t, 5> result {};
    size_t i = 0;
    result[i++] = prepared.preview.getFingerprint();
    if (prepared.cave) result[i++] = prepared.cave->preview.getFingerprint();
    for (auto* cap = prepared.plateau.get(); cap && i < result.size(); cap = cap->plateau.get())
        result[i++] = cap->preview.getFingerprint();
    return result;
}
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
			return "This series exceeds the joint preparation memory budget. Choose a smaller area or explicitly reduce N.";
		case PreparationInputError::ContextLimit:
		case PreparationInputError::ChangeLimit:
			return tooLargeMessage;
		case PreparationInputError::Cancelled:
			return cancelledMessage;
		case PreparationInputError::InvalidParameters:
			return "The surface settings are not valid.";
        case PreparationInputError::UpperFloorOccupied:
            return "The floor above the area is not empty: mountain tops need an empty rectangle.";
        case PreparationInputError::NoUpperFloor:
            return "The requested mountain levels extend above z=0; reduce Mountain levels or choose a lower surface floor.";
        case PreparationInputError::NoLowerFloor:
			return "There is no floor below this one for a cave.";
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
			return "This series exceeds the joint preparation memory budget. Choose a smaller area or explicitly reduce N.";
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
	wxWindow* host = this;  // parent of the controls created next: the dialog, or the page of the current tab

	const auto label = [&host](const wxString& text) {
		auto* control = newd wxStaticText(host, wxID_ANY, text);
		control->SetForegroundColour(Theme::Get(Theme::Role::Text));
		return control;
	};
	const auto spin = [&](const wxString& name, int min, int max, int value) {
		auto* control = newd wxSpinCtrl(host, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(Theme::Grid(24), -1), wxSP_ARROW_KEYS, min, max, value);
		control->SetName(name);
		control->SetBackgroundColour(fieldBackground);
		control->SetForegroundColour(fieldText);
		return control;
	};
	const auto text = [&](const wxString& name, const wxString& value) {
		auto* control = newd wxTextCtrl(host, wxID_ANY, value);
		control->SetName(name);
		control->SetBackgroundColour(fieldBackground);
		control->SetForegroundColour(fieldText);
		return control;
	};

	// A read-only list of the brushes the loaded client offers, so a name can only be picked.
	// Ground brushes feed the terrain fields, doodad brushes the vegetation fields.
	{
		std::string previousGround, previousDoodad, previousExit;
		for (const auto& [brushName, brush] : g_brushes.getMap()) {
			if (brushName.empty()) continue;
			if (brush->is<GroundBrush>()) {
				if (brushName == previousGround) continue;
				previousGround = brushName;
				ground_names_.Add(wxstr(brushName));
			} else if (brush->is<WallBrush>() && !brush->is<WallDecorationBrush>()) {
				wall_names_.Add(wxstr(brushName));
			} else if (const auto* doodad = brush->as<DoodadBrush>()) {
				// Cave exits are composites (ramps), so they get their own list with the resolver's own check.
				if (brushName != previousExit && resolveCaveExitBrush(*doodad)) {
					previousExit = brushName;
					cave_exit_names_.Add(wxstr(brushName));
				}
				// The resolver only places single items of variation 0.
				if (brushName == previousDoodad || !doodad->hasSingleObjects(0)) continue;
				previousDoodad = brushName;
				doodad_names_.Add(wxstr(brushName));
			}
		}
	}
	cave_wall_names_ = wall_names_;
	for (const auto& name : ground_names_) cave_wall_names_.Add(name);
	// listKind: 0 ground brushes, 1 doodad brushes, 2 wall brushes, 3 cave exit (ramp) brushes, 4 cave wall (wall and ground) brushes.
	const auto brushChoice = [&](const wxString& name, const wxString& preferred, int listKind) {
		const wxArrayString& names = listKind == 4 ? cave_wall_names_ : listKind == 3 ? cave_exit_names_ : listKind == 2 ? wall_names_ :
			listKind == 1 ? doodad_names_ : ground_names_;
		// Only the preferred (or first) entry exists until the list is first opened: filling
		// thousands of entries into eight lists made opening the dialog slow.
		wxString initial = names.Index(preferred) != wxNOT_FOUND ? preferred : (names.IsEmpty() ? wxString() : names[0]);
		auto* control = newd wxComboBox(host, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
		control->SetName(name);
		control->SetBackgroundColour(fieldBackground);
		control->SetForegroundColour(fieldText);
		if (!initial.empty()) {
			control->Append(initial);
			control->SetSelection(0);
		}
		control->Bind(wxEVT_COMBOBOX_DROPDOWN, [this, control, &names](wxCommandEvent& event) {
			event.Skip();
			const auto& activeNames = control == cave_entrance_brush_choice_ && cave_entrance_style_choice_->GetSelection() == 1 ? ground_names_ : names;
			if (control->GetCount() > 1 || activeNames.size() < 2) return;
			const wxString current = control->GetValue();
			control->Freeze();
			control->Clear();
			control->Append(activeNames);
            if (control == bridge_brush_choice_) control->Insert(wxEmptyString, 0);
			control->SetStringSelection(current);
			control->Thaw();
		});
		return control;
	};

	// Inputs, in tab order.
	auto* sections = newd wxNotebook(this, wxID_ANY);
	sections_ = sections;
	sections->SetName("terrain-sections");
	sections->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	wxFlexGridSizer* form = nullptr;
	const auto section = [&](const wxString& title) {
		auto* page = newd wxPanel(sections, wxID_ANY);
		page->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
		host = page;
		form = newd wxFlexGridSizer(2, gap, Theme::Grid(3));
		form->AddGrowableCol(1, 1);
		auto* box = newd wxBoxSizer(wxVERTICAL);
		box->Add(form, 0, wxEXPAND | wxALL, margin);
		page->SetSizer(box);
		sections->AddPage(page, title);
	};
	section("Area");

	form->Add(label("&Area:"), 0, wxALIGN_CENTER_VERTICAL);
	scope_choice_ = newd wxChoice(host, wxID_ANY);
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
	{
		auto* seedRow = newd wxBoxSizer(wxHORIZONTAL);
		seedRow->Add(seed_text_, 1, wxEXPAND | wxRIGHT, gap);
		auto* random = newd wxButton(host, wxID_ANY, "&Random");
		random->SetName("terrain-seed-random");
		random->SetToolTip("Fill in a new random seed. Nothing is generated until you press Preview.");
		random->Bind(wxEVT_BUTTON, &TerrainGeneratorDialog::OnRandomSeed, this);
		seedRow->Add(random, 0);
		form->Add(seedRow, 1, wxEXPAND);
	}
    form->Add(label("Retry seeds until all places are reachable:"), 0, wxALIGN_CENTER_VERTICAL);
    retry_spin_ = spin("terrain-retry-seeds", 0, 16, 0);
    retry_spin_->SetToolTip("Maximum attempts including the initial seed. 0 disables retries. If none succeeds, the initial seed is shown.");
    form->Add(retry_spin_, 1, wxEXPAND);
    form->Add(label("Candidates (1-16):"), 0, wxALIGN_CENTER_VERTICAL);
    candidate_count_spin_ = spin("terrain-candidate-count", 1, int(maxCandidateCount), int(defaultCandidateCount));
    form->Add(candidate_count_spin_, 1, wxEXPAND);
    candidate_count_spin_->SetToolTip(wxstr(std::format(
        "Joint series preparation budget: {}. Includes gallery, selection and one working preview; N is never reduced automatically.",
        formatMiB(preparationBudget.maxBytes))));
    candidate_count_spin_->Bind(wxEVT_SPINCTRL, &TerrainGeneratorDialog::OnInputChanged, this);
    candidate_count_spin_->Bind(wxEVT_TEXT, &TerrainGeneratorDialog::OnInputChanged, this);
    form->Add(label("Cop&y check:"), 0, wxALIGN_CENTER_VERTICAL);
    copy_index_choice_ = new wxChoice(host, wxID_ANY);
    copy_index_choice_->SetName("terrain-copy-index-choice");
    copy_index_choice_->SetToolTip("Refuse to place terrain that copies an area saved in a local copy index (Learn from open map can add areas). "
        "Indices of another client or in an old format are listed but cannot be chosen.");
    copy_index_choice_->Bind(wxEVT_CHOICE, &TerrainGeneratorDialog::OnCopyIndexChosen, this);
    form->Add(copy_index_choice_, 1, wxEXPAND);
    form->Add(label("Copy index file:"), 0, wxALIGN_CENTER_VERTICAL);
    copy_index_text_ = text("terrain-copy-index", "");
    copy_index_text_->Bind(wxEVT_TEXT, &TerrainGeneratorDialog::OnInputChanged, this);
    copy_index_text_->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
        // Typing a path shows "None", the matching listed index, or "Other file...".
        const auto value = copy_index_text_->GetValue();
        int selected = value.empty() ? 0 : int(copy_index_choice_->GetCount()) - 1;
        for (size_t i = 0; !value.empty() && i < copy_index_files_.size(); ++i)
            if (std::filesystem::path(value.ToStdWstring()) == copy_index_files_[i].path) selected = int(i) + 1;
        if (copy_index_choice_->GetCount() > 0) copy_index_choice_->SetSelection(selected);
        event.Skip();
    });
    copy_index_text_->SetToolTip("Explicit local I53 index path. Empty: similarity is not evaluated. Invalid or incompatible: entire series refused. Five G4 natural ground classes only; items and underground excluded.");
    form->Add(copy_index_text_, 1, wxEXPAND);
    retry_spin_->Bind(wxEVT_SPINCTRL, &TerrainGeneratorDialog::OnInputChanged, this);
    retry_spin_->Bind(wxEVT_TEXT, &TerrainGeneratorDialog::OnInputChanged, this);

	section("Terrain");
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
	style_choice_ = newd wxChoice(host, wxID_ANY);
	style_choice_->SetName("terrain-style");
	style_choice_->Append("Classic (random mix)");
	style_choice_->Append("Natural surface (lakes, rivers, rock)");
	style_choice_->SetSelection(0);
	form->Add(style_choice_, 1, wxEXPAND);

	form->Add(label("Style &profile:"), 0, wxALIGN_CENTER_VERTICAL);
	profile_choice_ = newd wxChoice(host, wxID_ANY);
	profile_choice_->SetName("terrain-profile");
	profile_choice_->Append("Built-in (manual settings)");
	profile_choice_->SetSelection(0);
	LoadStyleProfiles();
	LoadCopyIndices();
	form->Add(profile_choice_, 1, wxEXPAND);

    form->Add(label("Local style learning:"), 0, wxALIGN_CENTER_VERTICAL);
    auto* learn = new wxButton(host, wxID_ANY, "Learn from open map...");
    learn->SetName("terrain-learn");
    learn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        ReleasePreview(); UpdateControlStates();
        StyleLearningDialog dialog(this);
        if (dialog.ShowModal() == wxID_OK) {
            LoadStyleProfiles(); LoadCopyIndices(); RefreshHistoryChoices();
            SetStatus("Profile saved locally. Choose it in Style profile to generate a preview.", false);
        }
    });
    form->Add(learn, 1, wxEXPAND);

	form->Add(label("Roc&k brush:"), 0, wxALIGN_CENTER_VERTICAL);
	rock_brush_choice_ = brushChoice("terrain-rock-brush", "rocky ground", false);
	form->Add(rock_brush_choice_, 1, wxEXPAND);

	form->Add(label("S&hore brush:"), 0, wxALIGN_CENTER_VERTICAL);
	shore_brush_choice_ = brushChoice("terrain-shore-brush", "sand", false);
	form->Add(shore_brush_choice_, 1, wxEXPAND);
	for (auto* brush : {grass_brush_choice_, dirt_brush_choice_, water_brush_choice_, rock_brush_choice_, shore_brush_choice_}) {
		brush->SetToolTip("Natural surface can leave a one-tile gap next to existing ground outside the writable area.");
	}

	form->Add(label("Smooth &beaches:"), 0, wxALIGN_CENTER_VERTICAL);
	smooth_beaches_check_ = newd wxCheckBox(host, wxID_ANY, "Round the sand edge near lakes");
	smooth_beaches_check_->SetName("terrain-smooth-beaches");
	smooth_beaches_check_->SetForegroundColour(Theme::Get(Theme::Role::Text));
	smooth_beaches_check_->SetValue(true);
	smooth_beaches_check_->SetToolTip("Removes single sand cells, notches and checkerboard corners along the beaches of lakes.");
	form->Add(smooth_beaches_check_, 1, wxEXPAND);

	section("Paths");
	form->Add(label("Pat&h length:"), 0, wxALIGN_CENTER_VERTICAL);
	path_length_spin_ = spin("terrain-path-length", 0, 400, 0);
	path_length_spin_->SetToolTip("Winding paths over grass and dirt; skeleton cells per 65536 map cells. 0 = no paths. About 80 is similar to the reference maps.");
	form->Add(path_length_spin_, 1, wxEXPAND);

	form->Add(label("&Ford length:"), 0, wxALIGN_CENTER_VERTICAL);
	ford_length_spin_ = spin("terrain-ford-length", 0, 256, 0);
	ford_length_spin_->SetToolTip("Joins land cut off by a river or lake with a sand crossing up to this many water tiles wide. 0 = no fords. Uses the shore brush.");
	form->Add(ford_length_spin_, 1, wxEXPAND);

	form->Add(label("&Repair:"), 0, wxALIGN_CENTER_VERTICAL);
	repair_check_ = newd wxCheckBox(host, wxID_ANY, "Repair unreachable places");
	repair_check_->SetName("terrain-repair");
	repair_check_->SetForegroundColour(Theme::Get(Theme::Role::Text));
	repair_check_->SetValue(true);
	repair_check_->SetToolTip("Repairs access across all floors, including blockers found after autobordering. Clears blocking vegetation and borders on open ground, and bridges water up to Ford length. Shares a total limit of 64 repairs.");
	form->Add(repair_check_, 1, wxEXPAND);

    form->Add(label("Bridge brush:"), 0, wxALIGN_CENTER_VERTICAL);
    bridge_brush_choice_ = brushChoice("terrain-bridge-brush", "bridge", 2);
    if (wall_names_.Index("bridge") == wxNOT_FOUND) { bridge_brush_choice_->Clear(); bridge_brush_choice_->Append(wxEmptyString); bridge_brush_choice_->SetSelection(0); }
    bridge_brush_choice_->SetToolTip("Walkable bridge ground from a wall brush. Empty uses sand fords. Used by reachability repair.");
    form->Add(bridge_brush_choice_, 1, wxEXPAND);

	form->Add(label("Rock &passes:"), 0, wxALIGN_CENTER_VERTICAL);
	rock_passes_check_ = newd wxCheckBox(host, wxID_ANY, "Paths may cross bands of rock");
	rock_passes_check_->SetName("terrain-rock-passes");
	rock_passes_check_->SetForegroundColour(Theme::Get(Theme::Role::Text));
	rock_passes_check_->SetToolTip("Paths take the narrowest way through a rock band instead of going around it. Needs a path length above 0.");
	form->Add(rock_passes_check_, 1, wxEXPAND);

	form->Add(label("Path brus&h:"), 0, wxALIGN_CENTER_VERTICAL);
	path_brush_choice_ = brushChoice("terrain-path-brush", "cobblestone", false);
	form->Add(path_brush_choice_, 1, wxEXPAND);

	section("Buildings");
	form->Add(label("&Buildings:"), 0, wxALIGN_CENTER_VERTICAL);
	building_density_spin_ = spin("terrain-building-density", 0, 100, 0);
	building_density_spin_->SetToolTip("Building foundations (floor rectangles, optional walls and doors) per 65536 map cells. 0 = none. About 30 is similar to the reference maps.");
	form->Add(building_density_spin_, 1, wxEXPAND);

	form->Add(label("Floor brus&h:"), 0, wxALIGN_CENTER_VERTICAL);
	floor_brush_choice_ = brushChoice("terrain-floor-brush", "stone floor", false);
	form->Add(floor_brush_choice_, 1, wxEXPAND);

	form->Add(label("&Walls:"), 0, wxALIGN_CENTER_VERTICAL);
	walls_check_ = newd wxCheckBox(host, wxID_ANY, "Add walls around the buildings");
	walls_check_->SetName("terrain-walls");
	walls_check_->SetForegroundColour(Theme::Get(Theme::Role::Text));
	form->Add(walls_check_, 1, wxEXPAND);

	form->Add(label("Wall brus&h:"), 0, wxALIGN_CENTER_VERTICAL);
	wall_brush_choice_ = brushChoice("terrain-wall-brush", "stone wall", 2);
	form->Add(wall_brush_choice_, 1, wxEXPAND);

	form->Add(label("&Doors:"), 0, wxALIGN_CENTER_VERTICAL);
	doors_check_ = newd wxCheckBox(host, wxID_ANY, "One door per building, with a trail to it");
	doors_check_->SetName("terrain-doors");
	doors_check_->SetForegroundColour(Theme::Get(Theme::Role::Text));
	doors_check_->SetToolTip("Replaces one wall piece of every building with a door facing the nearest trail. The wall brush must have doors of the chosen kind.");
	form->Add(doors_check_, 1, wxEXPAND);

	form->Add(label("Door &kind:"), 0, wxALIGN_CENTER_VERTICAL);
	door_type_choice_ = newd wxChoice(host, wxID_ANY);
	door_type_choice_->SetName("terrain-door-type");
	for (const auto& kind : doorKinds) door_type_choice_->Append(wxString::FromUTF8(kind.first.data(), kind.first.size()));
	door_type_choice_->SetSelection(0);
	form->Add(door_type_choice_, 1, wxEXPAND);

	form->Add(label("Ruins (%):"), 0, wxALIGN_CENTER_VERTICAL);
	ruin_percent_spin_ = spin("terrain-ruin-percent", 0, 100, 0);
	ruin_percent_spin_->SetToolTip("Share of the buildings that become ruins: no door and gaps in the wall. 0 = none. Needs buildings.");
	form->Add(ruin_percent_spin_, 1, wxEXPAND);

    form->Add(label("Upper floors:"), 0, wxALIGN_CENTER_VERTICAL);
    building_storeys_check_ = newd wxCheckBox(host, wxID_ANY, "Floor z-1 with stairs in some buildings");
    building_storeys_check_->SetName("terrain-building-storeys");
    building_storeys_check_->SetForegroundColour(Theme::Get(Theme::Role::Text));
    building_storeys_check_->SetToolTip("Buildings with a door and at least 16 interior cells may get a walled floor above them, reached by stairs inside. The floor z-1 must be empty in the whole rectangle.");
    form->Add(building_storeys_check_, 1, wxEXPAND);
    form->Add(label("Upper floor share (%):"), 0, wxALIGN_CENTER_VERTICAL);
    upper_floor_percent_spin_ = spin("terrain-upper-floor-percent", 0, 100, 30);
    form->Add(upper_floor_percent_spin_, 1, wxEXPAND);
    form->Add(label("Stairs brush:"), 0, wxALIGN_CENTER_VERTICAL);
    building_stairs_choice_ = brushChoice("terrain-building-stairs", "ramp", 3);
    form->Add(building_stairs_choice_, 1, wxEXPAND);
    form->AddSpacer(0);
    building_storeys_hint_ = label("");
    form->Add(building_storeys_hint_, 1, wxEXPAND);

	form->Add(label("Camps:"), 0, wxALIGN_CENTER_VERTICAL);
	camp_count_spin_ = spin("terrain-camp-count", 0, 50, 0);
	camp_count_spin_->SetToolTip("Clearings with an open tent floor per 65536 map cells. 0 = none. Uses the path and floor brushes.");
	form->Add(camp_count_spin_, 1, wxEXPAND);

	form->Add(label("Camp&fire brush:"), 0, wxALIGN_CENTER_VERTICAL);
	campfire_brush_choice_ = brushChoice("terrain-campfire-brush", "campfire", 1);
	campfire_brush_choice_->SetToolTip("Doodad brush for the fire in the middle of every camp. Empty when the loaded client has no brush named campfire: pick another doodad brush to get a fire.");
	if (doodad_names_.Index("campfire") == wxNOT_FOUND) campfire_brush_choice_->Clear();
	form->Add(campfire_brush_choice_, 1, wxEXPAND);

    section("Levels");
    form->Add(label("Cave entrances:"), 0, wxALIGN_CENTER_VERTICAL);
	cave_entrances_spin_ = spin("terrain-cave-entrances", 0, 20, 0);
	cave_entrances_spin_->SetToolTip("Z entrances per 65536 cells in the selected style, each with a cave below (floor z+1) and a ramp back up. Tiles already on floor z+1 are kept: the cave goes around them.");
	form->Add(cave_entrances_spin_, 1, wxEXPAND);

	form->Add(label("Entrance style:"), 0, wxALIGN_CENTER_VERTICAL);
	cave_entrance_style_choice_ = newd wxChoice(host, wxID_ANY);
	cave_entrance_style_choice_->SetName("terrain-cave-entrance-style");
	cave_entrance_style_choice_->SetBackgroundColour(fieldBackground);
	cave_entrance_style_choice_->SetForegroundColour(fieldText);
	cave_entrance_style_choice_->Append("Rock face");
	cave_entrance_style_choice_->Append("Floor hole");
	cave_entrance_style_choice_->SetSelection(0);
	form->Add(cave_entrance_style_choice_, 1, wxEXPAND);
	form->Add(label("Z entrance brush:"), 0, wxALIGN_CENTER_VERTICAL);
	cave_entrance_brush_choice_ = brushChoice("terrain-cave-entrance-brush", "cave enter", 1);
	if (doodad_names_.Index("cave enter") == wxNOT_FOUND) cave_entrance_brush_choice_->Clear();
	cave_entrance_style_choice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent& event) {
		const bool hole = cave_entrance_style_choice_->GetSelection() == 1;
		const auto& names = hole ? ground_names_ : doodad_names_;
		const wxString preferred = hole ? "cave hole" : "cave enter";
		cave_entrance_brush_choice_->Clear();
		if (names.Index(preferred) != wxNOT_FOUND) { cave_entrance_brush_choice_->Append(preferred); cave_entrance_brush_choice_->SetSelection(0); }
		OnInputChanged(event);
	});
	form->Add(cave_entrance_brush_choice_, 1, wxEXPAND);

	form->Add(label("Cave exit brush:"), 0, wxALIGN_CENTER_VERTICAL);
	cave_exit_brush_choice_ = brushChoice("terrain-cave-exit-brush", "ramp", 3);
	cave_exit_brush_choice_->SetToolTip("Ramp-like doodad brushes only: every variant a floor-change tile, a tile on the floor above and at most one tail tile.");
	if (cave_exit_names_.Index("ramp") == wxNOT_FOUND) cave_exit_brush_choice_->Clear();
	form->Add(cave_exit_brush_choice_, 1, wxEXPAND);

	form->Add(label("Cave ground brush:"), 0, wxALIGN_CENTER_VERTICAL);
	cave_ground_brush_choice_ = brushChoice("terrain-cave-ground-brush", "cave", 0);
	if (ground_names_.Index("cave") == wxNOT_FOUND) cave_ground_brush_choice_->Clear();
	form->Add(cave_ground_brush_choice_, 1, wxEXPAND);

	form->Add(label("Cave wall brush:"), 0, wxALIGN_CENTER_VERTICAL);
	// Default to mountain ground. A wall brush can align pieces on all sides of the cave floor.
	const wxString caveWall = "mountain";
	cave_wall_brush_choice_ = brushChoice("terrain-cave-wall-brush", caveWall, 4);
	cave_wall_brush_choice_->SetToolTip("Wall brush drawn on cave ground around the cave floor, or a ground brush for the whole wall band.");
	if (cave_wall_names_.Index(caveWall) == wxNOT_FOUND) cave_wall_brush_choice_->Clear();
	form->Add(cave_wall_brush_choice_, 1, wxEXPAND);

    form->Add(label("Far cave exits:"), 0, wxALIGN_CENTER_VERTICAL);
    cave_far_exits_check_ = newd wxCheckBox(host, wxID_ANY, "Exit at the other end of each cave");
    cave_far_exits_check_->SetName("terrain-cave-far-exits");
    cave_far_exits_check_->SetForegroundColour(Theme::Get(Theme::Role::Text));
    cave_far_exits_check_->SetValue(true);
    cave_far_exits_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
    form->Add(cave_far_exits_check_, 1, wxEXPAND);
    form->Add(label("Cave door brush:"), 0, wxALIGN_CENTER_VERTICAL);
    cave_door_brush_choice_ = brushChoice("terrain-cave-door-brush", "stone wall", 2);
    form->Add(cave_door_brush_choice_, 1, wxEXPAND);
    form->Add(label("Cave corridors:"), 0, wxALIGN_CENTER_VERTICAL);
    cave_door_corridors_check_ = newd wxCheckBox(host, wxID_ANY, "Narrow corridors with closed, unlocked doors");
    cave_door_corridors_check_->SetName("terrain-cave-door-corridors");
    cave_door_corridors_check_->SetForegroundColour(Theme::Get(Theme::Role::Text));
    cave_door_corridors_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
    cave_door_brush_choice_->Bind(wxEVT_COMBOBOX, &TerrainGeneratorDialog::OnInputChanged, this);
    cave_door_brush_choice_->Bind(wxEVT_TEXT, &TerrainGeneratorDialog::OnInputChanged, this);
    form->Add(cave_door_corridors_check_, 1, wxEXPAND);
    form->AddSpacer(0);
    cave_door_hint_ = label("");
    form->Add(cave_door_hint_, 1, wxEXPAND);

    form->Add(label("Dungeons:"), 0, wxALIGN_CENTER_VERTICAL);
    building_dungeons_check_ = newd wxCheckBox(host, wxID_ANY, "Dungeon z+1 below some buildings");
    building_dungeons_check_->SetName("terrain-building-dungeons");
    building_dungeons_check_->SetForegroundColour(Theme::Get(Theme::Role::Text));
    building_dungeons_check_->SetToolTip("Buildings with a door and at least 16 interior cells may get walled rooms on the cave floor, "
        "joined by corridors with closed doors and reached by stairs down inside. Needs cave entrances and buildings with walls and doors.");
    form->Add(building_dungeons_check_, 1, wxEXPAND);
    form->Add(label("Dungeon share (%):"), 0, wxALIGN_CENTER_VERTICAL);
    dungeon_percent_spin_ = spin("terrain-dungeon-percent", 0, 100, 20);
    form->Add(dungeon_percent_spin_, 1, wxEXPAND);
    form->Add(label("Dungeon floor brush:"), 0, wxALIGN_CENTER_VERTICAL);
    dungeon_floor_choice_ = brushChoice("terrain-dungeon-floor", "dark tiled stone floor", 0);
    form->Add(dungeon_floor_choice_, 1, wxEXPAND);
    form->Add(label("Dungeon wall brush:"), 0, wxALIGN_CENTER_VERTICAL);
    dungeon_wall_choice_ = brushChoice("terrain-dungeon-wall", "brick wall", 2);
    form->Add(dungeon_wall_choice_, 1, wxEXPAND);
    form->Add(label("Dungeon stairs brush:"), 0, wxALIGN_CENTER_VERTICAL);
    dungeon_stairs_choice_ = brushChoice("terrain-dungeon-stairs", "ramp", 3);
    form->Add(dungeon_stairs_choice_, 1, wxEXPAND);
    form->Add(label("Dungeon stairs down:"), 0, wxALIGN_CENTER_VERTICAL);
    dungeon_descent_choice_ = brushChoice("terrain-dungeon-descent", "stairs down", 0);
    dungeon_descent_choice_->SetToolTip("Ground brush whose floor-change-down ground is the stairs down inside the building, facing north like the stairs up below it.");
    form->Add(dungeon_descent_choice_, 1, wxEXPAND);
    form->AddSpacer(0);
    building_dungeons_hint_ = label("");
    form->Add(building_dungeons_hint_, 1, wxEXPAND);

    form->Add(label("Mountain area z-1 (%):"), 0, wxALIGN_CENTER_VERTICAL);
    plateau_percent_spin_ = spin("terrain-plateau-percent", 0, 100, 10);
    plateau_percent_spin_->SetToolTip(
        "Maximum cap area as a percentage of the whole generated rectangle. 0 disables mountains. "
        "Whole rock components are selected; components exceeding the remaining limit are skipped. "
        "Each higher cap uses the same percentage of the cap below it."
    );
    form->Add(plateau_percent_spin_, 1, wxEXPAND);
    form->Add(label("Mountain passes:"), 0, wxALIGN_CENTER_VERTICAL);
    mountain_passes_spin_ = spin("terrain-mountain-passes", 0, 8, 0);
    form->Add(mountain_passes_spin_, 1, wxEXPAND);
    form->Add(label("Plateau ground brush:"), 0, wxALIGN_CENTER_VERTICAL);
    plateau_ground_choice_ = brushChoice("terrain-plateau-ground", "grass", 0);
    form->Add(plateau_ground_choice_, 1, wxEXPAND);
    form->Add(label("Plateau cliff brush:"), 0, wxALIGN_CENTER_VERTICAL);
    plateau_cliff_choice_ = brushChoice("terrain-plateau-cliff", "mountain", 0);
    form->Add(plateau_cliff_choice_, 1, wxEXPAND);
    form->Add(label("Mountain levels:"), 0, wxALIGN_CENTER_VERTICAL);
    plateau_levels_spin_ = spin("terrain-plateau-levels", 1, 3, 1);
    form->Add(plateau_levels_spin_, 1, wxEXPAND);
    form->Add(label("Plateau ramp brush:"), 0, wxALIGN_CENTER_VERTICAL);
    plateau_ramp_choice_ = brushChoice("terrain-plateau-ramp", "ramp", 3);
    form->Add(plateau_ramp_choice_, 1, wxEXPAND);

    section("Vegetation");
	form->Add(label("Vegetation:"), 0, wxALIGN_CENTER_VERTICAL);
	vegetation_check_ = newd wxCheckBox(host, wxID_ANY, "Add trees, bushes and stones");
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
	host = this;
    auto* helper = label("Only empty tiles are changed. Preview leaves your map unchanged; Place adds one Undo step for all generated floors.");
    helper->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	helper->Wrap(Theme::Grid(64));

	client_label_ = label(wxEmptyString);
	client_label_->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));

	auto* presetRow = newd wxBoxSizer(wxHORIZONTAL);
	presetRow->Add(label("Pre&set:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
	preset_choice_ = newd wxChoice(this, wxID_ANY);
	preset_choice_->SetName("terrain-preset");
	preset_choice_->Bind(wxEVT_CHOICE, &TerrainGeneratorDialog::OnPresetChosen, this);
	presetRow->Add(preset_choice_, 1, wxEXPAND | wxRIGHT, gap);
	auto* presetDelete = newd wxButton(this, wxID_ANY, "Delete preset");
	presetDelete->SetName("terrain-preset-delete");
	presetDelete->Bind(wxEVT_BUTTON, &TerrainGeneratorDialog::OnPresetDelete, this);
	presetRow->Add(presetDelete, 0);
	auto* presetSaveRow = newd wxBoxSizer(wxHORIZONTAL);
	presetSaveRow->Add(label("Preset name:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
	preset_name_text_ = text("terrain-preset-name", wxEmptyString);
	presetSaveRow->Add(preset_name_text_, 1, wxEXPAND | wxRIGHT, gap);
	auto* presetSave = newd wxButton(this, wxID_ANY, "Save preset");
	presetSave->SetName("terrain-preset-save");
	presetSave->Bind(wxEVT_BUTTON, &TerrainGeneratorDialog::OnPresetSave, this);
	presetSaveRow->Add(presetSave, 0);

	auto* left = newd wxBoxSizer(wxVERTICAL);
	left->Add(presetRow, 0, wxEXPAND | wxBOTTOM, gap);
	left->Add(presetSaveRow, 0, wxEXPAND | wxBOTTOM, gap);
	left->Add(sections, 1, wxEXPAND);
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
	preview_floor_label_ = label("Preview floor:");
	preview_floor_choice_ = newd wxChoice(this, wxID_ANY);
	preview_floor_choice_->Append("Surface");
	preview_floor_choice_->Append("Cave below");
    preview_floor_choice_->Append("Plateau z-1");
    preview_floor_choice_->Append("Plateau z-2");
    preview_floor_choice_->Append("Plateau z-3");
    preview_floor_choice_->SetSelection(0);
	preview_floor_choice_->SetName("terrain-preview-floor");
	preview_floor_choice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
		if (!prepared_) return;
		if (!BuildPreviewBitmap()) { ReleasePreview(); SetStatus(memoryMessage, true); }
		preview_panel_->Refresh();
	});
	right->Add(preview_floor_label_, 0, wxBOTTOM, gap);
	right->Add(preview_floor_choice_, 0, wxEXPAND | wxBOTTOM, gap);
	preview_floor_label_->Hide();
	preview_floor_choice_->Hide();
	right->Add(preview_panel_, 1, wxEXPAND);
    right->Add(label("Candidates: arrows select; Selected and Recommended are labelled."), 0, wxTOP, gap);
    candidate_list_ = newd wxListCtrl(this, wxID_ANY, wxDefaultPosition,
        wxSize(-1, Theme::Grid(28)), wxLC_REPORT | wxLC_SINGLE_SEL);
    candidate_list_->SetName("terrain-candidates");
    candidate_list_->SetBackgroundColour(Theme::Get(Theme::Role::Background));
    candidate_list_->SetForegroundColour(Theme::Get(Theme::Role::Text));
    candidate_list_->InsertColumn(0, "Candidate", wxLIST_FORMAT_LEFT, Theme::Grid(34));
    candidate_list_->InsertColumn(1, "Seed / initial / attempts", wxLIST_FORMAT_LEFT, Theme::Grid(60));
    candidate_list_->InsertColumn(2, "Validation", wxLIST_FORMAT_LEFT, Theme::Grid(32));
    const int thumb = std::min(128, Theme::Grid(12));
    auto images = std::make_unique<wxImageList>(thumb, thumb, true);
    candidate_images_ = images.get();
    candidate_list_->AssignImageList(images.release(), wxIMAGE_LIST_SMALL);
    candidate_list_->Bind(wxEVT_LIST_ITEM_SELECTED, &TerrainGeneratorDialog::OnCandidateSelected, this);
    right->Add(candidate_list_, 0, wxEXPAND | wxTOP, gap);
    candidate_progress_ = newd wxGauge(this, wxID_ANY, int(maxCandidateCount));
    candidate_progress_->SetName("terrain-candidate-progress");
    right->Add(candidate_progress_, 0, wxEXPAND | wxTOP, gap);
    const auto reportList = [&](const char* name, const char* first, const char* second, int firstWidth, int secondWidth) {
        auto* list = newd wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, Theme::Grid(30)), wxLC_REPORT | wxLC_SINGLE_SEL);
        list->SetName(name);
        list->SetBackgroundColour(Theme::Get(Theme::Role::Background));
        list->SetForegroundColour(Theme::Get(Theme::Role::Text));
        list->InsertColumn(0, first, wxLIST_FORMAT_LEFT, Theme::Grid(firstWidth));
        list->InsertColumn(1, second, wxLIST_FORMAT_LEFT, Theme::Grid(secondWidth));
        return list;
    };
    auto* reports = newd wxBoxSizer(wxHORIZONTAL);
    auto* statisticsColumn = newd wxBoxSizer(wxVERTICAL);
    statisticsColumn->Add(label("Statistics of the Selected candidate:"), 0, wxBOTTOM, gap);
    statistics_list_ = reportList("terrain-statistics", "Statistic", "Value", 36, 40);
    statisticsColumn->Add(statistics_list_, 1, wxEXPAND);
    auto* warningsColumn = newd wxBoxSizer(wxVERTICAL);
    warningsColumn->Add(label("Warnings:"), 0, wxBOTTOM, gap);
    warnings_list_ = reportList("terrain-warnings", "Severity", "Warning", 14, 80);
    warningsColumn->Add(warnings_list_, 1, wxEXPAND);
    reports->Add(statisticsColumn, 1, wxEXPAND | wxRIGHT, gap);
    reports->Add(warningsColumn, 1, wxEXPAND);
    right->Add(reports, 0, wxEXPAND | wxTOP, gap);
    right->Add(label("Feedback for Selected candidate (independent of Place):"), 0, wxTOP, gap);
    right->Add(label("Optional note (maximum 1024 UTF-8 bytes):"), 0, wxTOP, gap);
    feedback_note_ = newd wxTextCtrl(this, wxID_ANY);
    feedback_note_->SetName("terrain-feedback-note");
    feedback_note_->SetBackgroundColour(Theme::Get(Theme::Role::Background));
    feedback_note_->SetForegroundColour(Theme::Get(Theme::Role::Text));
    right->Add(feedback_note_, 0, wxEXPAND | wxTOP, gap);
    auto* feedbackButtons = newd wxBoxSizer(wxHORIZONTAL);
    const auto feedbackButton = [&](const char* caption, const char* name, FeedbackAction action) {
        auto* button = newd wxButton(this, wxID_ANY, caption);
        button->SetName(name);
        button->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent&) { OnFeedback(action); });
        feedbackButtons->Add(button, 0, wxRIGHT, gap);
        return button;
    };
    approve_button_ = feedbackButton("&Approve", "terrain-approve", FeedbackAction::Approve);
    reject_button_ = feedbackButton("&Reject", "terrain-reject", FeedbackAction::Reject);
    adjust_button_ = feedbackButton("Ad&just", "terrain-adjust", FeedbackAction::Adjust);
    right->Add(feedbackButtons, 0, wxTOP, gap);
    auto* adjustScope = label(adjustmentScope);
    adjustScope->Wrap(Theme::Grid(110));
    right->Add(adjustScope, 0, wxEXPAND | wxTOP, gap);
    right->Add(label("Local version history (choose, then Activate / rollback):"), 0, wxTOP, gap);
    auto* historyRow = newd wxBoxSizer(wxHORIZONTAL);
    history_choice_ = newd wxChoice(this, wxID_ANY);
    history_choice_->SetName("terrain-history");
    historyRow->Add(history_choice_, 1, wxEXPAND | wxRIGHT, gap);
    rollback_button_ = newd wxButton(this, wxID_ANY, "Activate / roll&back");
    rollback_button_->SetName("terrain-rollback");
    rollback_button_->Bind(wxEVT_BUTTON, &TerrainGeneratorDialog::OnRollback, this);
    historyRow->Add(rollback_button_, 0);
    right->Add(historyRow, 0, wxEXPAND | wxTOP, gap);

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
	LoadPresets();
	CentreOnParent();
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        ReleasePreview(); // Stop and join owned workers before the window or map can disappear.
        event.Skip();
    });

	// Every input change releases the owned preview.
    for (wxSpinCtrl* control :
         {x_spin_, y_spin_, z_spin_, width_spin_, height_spin_, grass_weight_spin_, dirt_weight_spin_, water_weight_spin_,
          path_length_spin_, ford_length_spin_, building_density_spin_, ruin_percent_spin_, camp_count_spin_, cave_entrances_spin_,
           plateau_levels_spin_, plateau_percent_spin_, mountain_passes_spin_, upper_floor_percent_spin_, dungeon_percent_spin_}) {
        control->Bind(wxEVT_SPINCTRL, &TerrainGeneratorDialog::OnInputChanged, this);
		control->Bind(wxEVT_TEXT, &TerrainGeneratorDialog::OnInputChanged, this);
    }
    seed_text_->Bind(wxEVT_TEXT, &TerrainGeneratorDialog::OnInputChanged, this);
    for (wxComboBox* control :
         {grass_brush_choice_, dirt_brush_choice_, water_brush_choice_, rock_brush_choice_, shore_brush_choice_, path_brush_choice_,
          floor_brush_choice_, wall_brush_choice_, tree_brush_choice_, bush_brush_choice_, stone_brush_choice_, cave_entrance_brush_choice_,
          cave_exit_brush_choice_, cave_ground_brush_choice_, cave_wall_brush_choice_, plateau_ground_choice_, plateau_cliff_choice_, plateau_ramp_choice_, bridge_brush_choice_, building_stairs_choice_,
          dungeon_floor_choice_, dungeon_wall_choice_, dungeon_stairs_choice_, dungeon_descent_choice_}) {
        control->Bind(wxEVT_COMBOBOX, &TerrainGeneratorDialog::OnInputChanged, this);
    }
    scope_choice_->Bind(wxEVT_CHOICE, &TerrainGeneratorDialog::OnInputChanged, this);
	style_choice_->Bind(wxEVT_CHOICE, &TerrainGeneratorDialog::OnInputChanged, this);
	profile_choice_->Bind(wxEVT_CHOICE, &TerrainGeneratorDialog::OnProfileChosen, this);
	vegetation_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
	walls_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
	doors_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
    building_storeys_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
    building_dungeons_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
	rock_passes_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
	smooth_beaches_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
	repair_check_->Bind(wxEVT_CHECKBOX, &TerrainGeneratorDialog::OnInputChanged, this);
	door_type_choice_->Bind(wxEVT_CHOICE, &TerrainGeneratorDialog::OnInputChanged, this);
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
	default_adjustment_ = {uint32_t(path_length_spin_->GetValue()), uint32_t(building_density_spin_->GetValue()),
        uint32_t(ruin_percent_spin_->GetValue()), uint32_t(camp_count_spin_->GetValue()), uint32_t(ford_length_spin_->GetValue()),
        uint32_t(rock_passes_check_->GetValue()), smooth_beaches_check_->GetValue() ? 2u : 0u, uint32_t(repair_check_->GetValue())};
    RefreshHistoryChoices();
    if (profile_choice_->GetSelection() > 0) {
        style_choice_->SetSelection(1);
        wxCommandEvent restored; OnProfileChosen(restored);
    }
	UpdateControlStates();
	SetStatus(has_source_map_ ? wxString("Choose an area and press Preview.") : describe(TerrainTargetError::NoEditor), !has_source_map_);
	if (!history_error_.empty()) SetStatus(wxstr(history_error_), true);
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
void TerrainGeneratorDialog::LoadCopyIndices() {
    const auto current = copy_index_text_->GetValue();
    copy_index_choice_->Clear();
    copy_index_files_.clear();
    copy_index_choice_->Append("None");
    if (const auto ids = loadedClientAssetIds())
        copy_index_files_ = listCopyIndices(styleProfileDirectory(), ids->client, ids->otbSha256, ids->datSha256);
    int selected = current.empty() ? 0 : -1;
    for (size_t i = 0; i < copy_index_files_.size(); ++i) {
        const auto& file = copy_index_files_[i];
        copy_index_choice_->Append(wxstr(file.problem.empty()
            ? std::format("{} ({} area{})", file.name, file.references, file.references == 1 ? "" : "s")
            : std::format("{} - cannot be used: {}", file.name, file.problem)));
        if (!current.empty() && std::filesystem::path(current.ToStdWstring()) == file.path) selected = int(i) + 1;
    }
    copy_index_choice_->Append("Other file...");
    copy_index_choice_->SetSelection(selected >= 0 ? selected : int(copy_index_choice_->GetCount()) - 1);
}
void TerrainGeneratorDialog::OnCopyIndexChosen(wxCommandEvent& event) {
    const int choice = copy_index_choice_->GetSelection();
    if (choice == 0) copy_index_text_->SetValue(wxEmptyString);
    else if (size_t(choice) <= copy_index_files_.size()) {
        const auto& file = copy_index_files_[size_t(choice) - 1];
        if (!file.problem.empty()) {
            LoadCopyIndices();
            SetStatus(wxstr(std::format("Copy index \"{}\" cannot be used: {}.", file.name, file.problem)), true);
            return;
        }
        copy_index_text_->SetValue(wxString(file.path.wstring()));
    } else {
        copy_index_text_->SetFocus();
        SetStatus("Type the path of a copy index file below.", false);
    }
    event.Skip();
}
void TerrainGeneratorDialog::LoadStyleProfiles() {
	namespace rp = rme::procedural;
	const auto folder = rp::styleProfileDirectory();
	const auto ids = rp::loadedClientAssetIds();
	profile_choice_->Clear(); profile_choice_->Append("Built-in (manual settings)"); profile_choice_->SetSelection(0);
	profiles_.clear(); style_history_.reset(); history_error_.clear();
	auto history = rp::StyleHistoryStore::open(folder / "history.json");
	if (history) style_history_ = std::move(*history);
	else history_error_ = history.error();
	wxString hint;
	if (!ids) {
		hint = "Style profiles need a loaded client with items.otb.";
	} else {
		auto scan = rp::scanStyleProfiles(folder, *ids);
		if (style_history_) rp::offerStyleHistory(scan.offer, style_history_->data(), *ids);
		const wxString where(folder.generic_wstring());
		if (!scan.exists) {
			hint = "No style profiles. Put *.profile files in:\n" + where;
		} else {
			profiles_ = std::move(scan.offer.offered);
			for (const auto& entry : profiles_) profile_choice_->Append(wxstr(entry.name));
			if (style_history_) for (size_t i = 0; i < profiles_.size(); ++i)
                if (!profiles_[i].versionId.empty() && profiles_[i].versionId == style_history_->data().active)
                    profile_choice_->SetSelection(int(i + 1));
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
	const bool hadPreview = series_request_ || preview_steps_ || prepared_ || preview_bitmap_.IsOk();
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
	learned_ruin_floor_ = learned_compound_ = 0;
	const int picked = profile_choice_->GetSelection();
	if (style_history_ && picked > 0 && size_t(picked) <= profiles_.size()) {
        const auto& version = profiles_[size_t(picked)-1].versionId;
        if (!version.empty() && version != style_history_->data().active) {
            const auto ids = loadedClientAssetIds();
            const auto saved = ids ? style_history_->activate(version, ids->client, ids->otbSha256, ids->datSha256) :
                std::expected<void,std::string>(std::unexpected("Cannot identify client assets."));
            if (!saved) {
                profile_choice_->SetSelection(0);
                for (size_t i=0;i<profiles_.size();++i)
                    if (profiles_[i].versionId==style_history_->data().active) profile_choice_->SetSelection(int(i+1));
                wxCommandEvent restore; OnProfileChosen(restore);
                SetStatus(wxstr(saved.error()), true); return;
            }
            RefreshHistoryChoices();
        }
    }
    if (picked > 0 && size_t(picked) <= profiles_.size() && !profiles_[size_t(picked)-1].versionId.empty()) {
	const auto adjustment = profiles_[size_t(picked)-1].adjustment ?
        *profiles_[size_t(picked)-1].adjustment : default_adjustment_;
    path_length_spin_->SetValue(int(adjustment[0])); building_density_spin_->SetValue(int(adjustment[1]));
    ruin_percent_spin_->SetValue(int(adjustment[2])); camp_count_spin_->SetValue(int(adjustment[3]));
    ford_length_spin_->SetValue(int(adjustment[4])); rock_passes_check_->SetValue(adjustment[5] != 0);
    smooth_beaches_check_->SetValue(adjustment[6] != 0); repair_check_->SetValue(adjustment[7] != 0);
    }
	if (picked <= 0 || static_cast<size_t>(picked) > profiles_.size()) {
		SetStatus("Built-in settings: brushes set back to the defaults.", false);
		return;
	}
	const auto& entry = profiles_[static_cast<size_t>(picked) - 1];
	// A saved adjusted version keeps its own generation settings (restored above); learned defaults do not replace them.
	const bool adjusted = !entry.versionId.empty() && entry.adjustment.has_value();
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
		// A RAW item ("raw:<id>") is not in the brush list; it is usable when the loaded client can place it.
		const bool usable = (rme::procedural::rawDecorId(name) || doodad_names_.Index(wanted) != wxNOT_FOUND) && usableDecor(name);
		if (usable) setBrush(decorFields[kind], wanted);
		note(usable ? applied : skipped, decorLabels[kind], wanted);
	}
	// A learned style comes with rivers, rock and sea cut from a real map: they may leave land that cannot be walked to,
	// which blocks Place. Repairing reachability (fords, passes) keeps such a series usable.
	if (std::ranges::any_of(entry.profile.sources, [](const auto& source) { return source.alias == "user-local-surface"; })) {
		if (!adjusted) {
			repair_check_->SetValue(true);
			if (entry.profile.tuning && entry.profile.tuning->riverCount && ford_length_spin_->GetValue() == 0) ford_length_spin_->SetValue(8);
			if (entry.profile.tuning && entry.profile.tuning->rockPermille) rock_passes_check_->SetValue(true);
		}
		note(applied, "reachability repair", wxString(repair_check_->GetValue() ? "on" : "off"));
	}
	// The top of a mountain level is the style's own land (sand in a desert), not the built-in grass.
	if (const std::string& land = entry.profile.palette[size_t(rme::procedural::StyleKind::Grass)]; !land.empty()) {
		const Brush* brush = g_brushes.getBrush(land);
		const auto* ground = brush ? brush->as<GroundBrush>() : nullptr;
		if (ground && ground->getTotalChance() > 0) {
			setBrush(plateau_ground_choice_, wxstr(land));
			note(applied, "mountain top", wxstr(land));
		}
	}
	// Buildings and ruins the profile learned: density, ruin share, floor and walls (a brush this client lacks is skipped).
	if (const auto& buildings = entry.profile.buildings; buildings.density) {
		if (!adjusted) {
			building_density_spin_->SetValue(int(buildings.density));
			ruin_percent_spin_->SetValue(int(buildings.ruinPercent));
		}
		learned_ruin_floor_ = buildings.floorPercent < 100 ? buildings.floorPercent : 0;
		learned_compound_ = buildings.compoundPercent;
		const Brush* floorBrush = g_brushes.getBrush(buildings.floor);
		const auto* floorGround = floorBrush ? floorBrush->as<GroundBrush>() : nullptr;
		if (floorGround && floorGround->getTotalChance() > 0) setBrush(floor_brush_choice_, wxstr(buildings.floor));
		note(floorGround ? applied : skipped, "building floor", wxstr(buildings.floor));
		const Brush* wallBrush = buildings.wall.empty() ? nullptr : g_brushes.getBrush(buildings.wall);
		const bool walls = wallBrush && wallBrush->as<WallBrush>() && !wallBrush->is<WallDecorationBrush>();
		walls_check_->SetValue(walls);
		if (walls) setBrush(wall_brush_choice_, wxstr(buildings.wall));
		if (!buildings.wall.empty()) note(walls ? applied : skipped, "building walls", wxstr(buildings.wall));
		note(applied, "buildings", wxstr(std::format("{} per 65536 tiles, {}% ruins with {}% floor, {}% in halls",
			buildings.density, buildings.ruinPercent, buildings.floorPercent, buildings.compoundPercent)));
	}
	for (const auto& mix : entry.profile.grassMix)
		if (!mix.brush.empty()) note(applied, "grass patches", wxstr(std::format("{} {}%", mix.brush, mix.permille / 10)));
	for (const auto& mix : entry.profile.treeMix)
		if (!mix.brush.empty()) note(applied, "trees with vegetation", wxstr(std::format("{} {}%", mix.brush, mix.permille / 10)));
	for (const auto& mix : entry.profile.bushMix)
		if (!mix.brush.empty()) note(applied, "bushes with vegetation", wxstr(std::format("{} {}%", mix.brush, mix.permille / 10)));
	wxString message = "Style profile \"" + wxstr(entry.name) + "\": ";
	message += applied.empty() ? wxString("tuning only, brushes at their defaults.") : "brushes set (" + applied + ").";
	if (!skipped.empty()) message += " Not usable in this client, kept the default: " + skipped + ".";
	SetStatus(message, false);
	UpdateControlStates();
}

void TerrainGeneratorDialog::RefreshHistoryChoices() {
    history_choice_->Clear(); history_ids_.clear();
    if (style_history_) for (const auto& p : profiles_) {
        if (p.versionId.empty()) continue;
        history_choice_->Append(wxstr(p.name + (p.versionId == style_history_->data().active ? " Active" : "")));
        history_ids_.push_back(p.versionId);
        if (p.versionId == style_history_->data().active) history_choice_->SetSelection(int(history_ids_.size()-1));
    }
    if (history_choice_->GetSelection() == wxNOT_FOUND && !history_ids_.empty()) history_choice_->SetSelection(0);
}

void TerrainGeneratorDialog::OnRollback(wxCommandEvent&) {
    const int picked = history_choice_->GetSelection();
    if (!NaturalSurface() || !style_history_ || picked < 0 || size_t(picked) >= history_ids_.size()) return;
    const auto ids = loadedClientAssetIds();
    if (!ids) { SetStatus("Load a compatible client before activating a version.", true); return; }
    const auto saved = style_history_->activate(history_ids_[size_t(picked)], ids->client, ids->otbSha256, ids->datSha256);
    if (!saved) { SetStatus(wxstr(saved.error()), true); return; }
    const auto id = style_history_->data().active;
    for (size_t i=0; i<profiles_.size(); ++i) if (profiles_[i].versionId == id) profile_choice_->SetSelection(int(i+1));
    wxCommandEvent changed; OnProfileChosen(changed); RefreshHistoryChoices(); UpdateControlStates();
    SetStatus("Local version activated. History retained; press Preview for a new series.", false);
}

void TerrainGeneratorDialog::OnFeedback(FeedbackAction action) try {
    if (!style_history_) { SetStatus(wxstr(history_error_), true); return; }
    if (!NaturalSurface() || collecting_ || preview_steps_ || !prepared_ || !selected_candidate_ ||
        *selected_candidate_ >= candidates_.size() || !SeriesCurrent()) {
        SetStatus("Feedback refused: generate and select a current candidate first.", true); return;
    }
    const auto request = ReadRequest();
    if (!request || !series_request_ || request->config != series_request_->config || request->profile != series_request_->profile ||
        candidateFingerprints(*prepared_) != candidates_[*selected_candidate_].fingerprints) {
        SetStatus("Feedback refused: preview is stale.", true); return;
    }
    if (action != FeedbackAction::Reject && !CandidateAllowed()) {
        SetStatus("Approve and Adjust require an admissible candidate. Reject can record hardFail.", true); return;
    }
    const wxString noteText = feedback_note_->GetValue();
    const auto note = noteText.utf8_str();
    if (!noteText.empty() && !note.data()) { SetStatus("Cannot encode the note as UTF-8.", true); return; }
    if (note.length() > 1024) { SetStatus("Note exceeds 1024 UTF-8 bytes. Shorten the note before saving.", true); feedback_note_->SetFocus(); return; }
    const auto ids = loadedClientAssetIds();
    if (!ids) { SetStatus("Cannot identify loaded client assets for feedback.", true); return; }
    const int picked = profile_choice_->GetSelection();
    const OfferedProfile* p = picked > 0 && size_t(picked) <= profiles_.size() ? &profiles_[size_t(picked)-1] : nullptr;
    if (action == FeedbackAction::Adjust && !p) { SetStatus("Adjust requires a base profile.", true); return; }
    if (p && checkProfile(p->profile, ids->client, ids->otbSha256, ids->datSha256, request->config.region.z) != ProfileCompatibility::Compatible) {
        SetStatus("Feedback refused: profile/client assets mismatch.", true); return;
    }
    StyleFeedback f;
    f.action = action;
    if (note.data()) f.note.assign(note.data(), note.length());
    f.settings = p ? p->profile : StyleProfile{};
    f.settings.client=ids->client; f.settings.otbSha256=ids->otbSha256; f.settings.datSha256=ids->datSha256;
    const auto& effective = prepared_->snapshot.input.config;
    f.settings.tuning = effective.surface;
    f.settings.decorTuning = effective.decor;
    f.generation = effective.surface;
    f.vegetation = effective.vegetation;
    f.adjustment = styleAdjustment(effective.surface);
    f.base=p ? (p->baseId.empty() ? styleIdentity(p->profile) : p->baseId) : "";
    f.version=p ? p->versionId : "";
    f.policy=ranking_bands_.empty() ? "unranked/I54-v1" : "g6-bands-v1/A/I54-v1";
    const auto& c=candidates_[*selected_candidate_];
    f.candidate=*selected_candidate_; f.userSeed=series_request_->config.seed;
    f.initialSeed=c.initialSeed; f.actualSeed=c.seed; f.fingerprints=c.fingerprints; f.hardFail=c.score.hardFail;
    const auto saved=style_history_->record(std::move(f), p ? &p->profile : nullptr, p ? p->versionId : "");
    if (!saved) { SetStatus(wxstr(saved.error()),true); return; }
    feedback_note_->Clear();
    if (action==FeedbackAction::Adjust) {
        ReleasePreview(); LoadStyleProfiles(); RefreshHistoryChoices();
        wxCommandEvent changed; OnProfileChosen(changed); UpdateControlStates();
        SetStatus("Adjust saved as a new local version. Previous versions retained; press Preview.",false);
    } else SetStatus("Feedback saved locally for Selected candidate. Map and Undo unchanged.",false);
} catch (const std::exception& e) { SetStatus(wxstr(std::string("Feedback refused: ")+e.what()),true); }

void TerrainGeneratorDialog::ReleasePreview() {
	preview_timer_.Stop();
    collecting_ = false;
    series_request_.reset();
    candidates_.clear();
    selected_candidate_.reset(); recommended_candidate_.reset();
    candidate_list_->DeleteAllItems(); candidate_images_->RemoveAll();
    candidate_progress_->SetValue(0);
    statistics_list_->DeleteAllItems(); warnings_list_->DeleteAllItems();
	preview_steps_.reset();
	copy_index_.reset();
	pending_request_.reset();
	prepared_.reset();
	selection_.reset();
	preview_bitmap_ = wxNullBitmap;
	preview_floor_choice_->SetSelection(0);
	preview_floor_choice_->Hide();
	preview_floor_label_->Hide();
	validate_button_->Disable();
	place_button_->Disable();
	preview_panel_->Refresh();
}

void TerrainGeneratorDialog::UpdateControlStates() {
	const bool natural = NaturalSurface();
    preview_floor_choice_->Show(prepared_ && (prepared_->cave || prepared_->plateau));
    preview_floor_label_->Show(prepared_ && (prepared_->cave || prepared_->plateau));
    const auto selectedFloor = preview_floor_choice_->GetStringSelection();
    preview_floor_choice_->Clear();
    preview_floor_choice_->Append("Surface");
    if (prepared_ && prepared_->cave) {
        preview_floor_choice_->Append("Cave below");
    }
    if (prepared_) {
        int level = 1;
        for (auto* p = prepared_->plateau.get(); p; p = p->plateau.get(), ++level) {
            preview_floor_choice_->Append(wxstr(p->plan.config.surface.plateauMinTop ? std::format("Plateau z-{}", level) : std::format("Upper floor z-{}", level)));
        }
    }
    const int prior = preview_floor_choice_->FindString(selectedFloor);
    preview_floor_choice_->SetSelection(prior == wxNOT_FOUND ? 0 : prior);
    plateau_percent_spin_->Enable(natural);
    mountain_passes_spin_->Enable(natural);
    plateau_levels_spin_->Enable(natural && plateau_percent_spin_->GetValue() > 0);
    for (auto* c : {plateau_ground_choice_, plateau_cliff_choice_, plateau_ramp_choice_}) {
        c->Enable(natural && plateau_percent_spin_->GetValue() > 0);
    }
    for (wxSpinCtrl* control : { grass_weight_spin_, dirt_weight_spin_, water_weight_spin_ }) control->Enable(!natural);
	for (wxComboBox* control : { rock_brush_choice_, shore_brush_choice_ }) control->Enable(natural);
	path_length_spin_->Enable(natural);
	building_density_spin_->Enable(natural);
	ruin_percent_spin_->Enable(natural && building_density_spin_->GetValue() > 0);
	camp_count_spin_->Enable(natural);
	campfire_brush_choice_->Enable(natural && camp_count_spin_->GetValue() > 0);
	const bool caveProfile = natural;
	cave_entrances_spin_->Enable(caveProfile);
	cave_entrance_style_choice_->Enable(caveProfile && cave_entrances_spin_->GetValue() > 0);
	for (wxComboBox* control : { cave_entrance_brush_choice_, cave_exit_brush_choice_, cave_ground_brush_choice_, cave_wall_brush_choice_ }) control->Enable(caveProfile && cave_entrances_spin_->GetValue() > 0);
	ford_length_spin_->Enable(natural);
	rock_passes_check_->Enable(natural && path_length_spin_->GetValue() > 0);
	smooth_beaches_check_->Enable(natural);
	repair_check_->Enable(natural);
    bridge_brush_choice_->Enable(natural && repair_check_->GetValue());
    retry_spin_->Enable(natural);
	path_brush_choice_->Enable(natural && (path_length_spin_->GetValue() > 0 || building_density_spin_->GetValue() > 0 || camp_count_spin_->GetValue() > 0 || cave_entrances_spin_->GetValue() > 0));
	floor_brush_choice_->Enable(natural && (building_density_spin_->GetValue() > 0 || camp_count_spin_->GetValue() > 0 || cave_entrances_spin_->GetValue() > 0));
    const bool caves = natural && cave_entrances_spin_->GetValue() > 0;
    cave_far_exits_check_->Enable(caves);
    cave_door_brush_choice_->Enable(caves);
    const bool doorAvailable = bool(rme::procedural::resolveCaveDoorBrush(std::string(cave_door_brush_choice_->GetValue().utf8_str())));
    cave_door_corridors_check_->Enable(caves && doorAvailable);
    if (!doorAvailable) cave_door_corridors_check_->SetValue(false);
    cave_door_hint_->SetLabel(doorAvailable ? wxString{} : wxString("Choose a wall brush with closed, unlocked normal doors to enable corridors."));
	walls_check_->Enable(natural && building_density_spin_->GetValue() > 0);
	wall_brush_choice_->Enable(natural && building_density_spin_->GetValue() > 0 && walls_check_->GetValue());
	doors_check_->Enable(natural && building_density_spin_->GetValue() > 0 && walls_check_->GetValue());
	door_type_choice_->Enable(natural && building_density_spin_->GetValue() > 0 && walls_check_->GetValue() && doors_check_->GetValue());
    const bool walledBuildings = natural && building_density_spin_->GetValue() > 0 && walls_check_->GetValue();
    const bool stairsAvailable = bool(rme::procedural::resolveBuildingStairsBrush(std::string(building_stairs_choice_->GetValue().utf8_str())));
    building_stairs_choice_->Enable(walledBuildings);
    building_storeys_check_->Enable(walledBuildings && stairsAvailable);
    if (!stairsAvailable) building_storeys_check_->SetValue(false);
    upper_floor_percent_spin_->Enable(walledBuildings && stairsAvailable && building_storeys_check_->GetValue());
    building_storeys_hint_->SetLabel(stairsAvailable ? wxString{} : wxString("Choose a stairs brush with a floor-change item and an upper part to enable upper floors."));
    // Dungeons (iteration 45) are drawn on the cave floor below doored, walled buildings.
    const bool dungeonPlaces = caves && walledBuildings && doors_check_->GetValue();
    rme::procedural::GroundProfile dungeonProfile;
    dungeonProfile.dungeonFloor = std::string(dungeon_floor_choice_->GetValue().utf8_str());
    dungeonProfile.dungeonWall = std::string(dungeon_wall_choice_->GetValue().utf8_str());
    dungeonProfile.dungeonStairs = std::string(dungeon_stairs_choice_->GetValue().utf8_str());
    dungeonProfile.dungeonDescent = std::string(dungeon_descent_choice_->GetValue().utf8_str());
    const bool dungeonBrushes = bool(rme::procedural::resolveDungeonBrushes(dungeonProfile));
    for (auto* control : {dungeon_floor_choice_, dungeon_wall_choice_, dungeon_stairs_choice_, dungeon_descent_choice_}) control->Enable(dungeonPlaces);
    building_dungeons_check_->Enable(dungeonPlaces && dungeonBrushes);
    if (!dungeonBrushes) building_dungeons_check_->SetValue(false);
    dungeon_percent_spin_->Enable(dungeonPlaces && dungeonBrushes && building_dungeons_check_->GetValue());
    building_dungeons_hint_->SetLabel(!dungeonBrushes ? wxString("Choose a floor brush, a wall brush with closed, unlocked normal doors and a stairs brush with an upper part and a stairs-down brush with a floor-change-down ground to enable dungeons.")
        : !dungeonPlaces ? wxString("Dungeons need cave entrances and buildings with walls and doors.") : wxString{});
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
	place_button_->Enable(CandidateAllowed());
    candidate_count_spin_->Enable(natural);
    copy_index_text_->Enable(natural);
    copy_index_choice_->Enable(natural);
    const bool feedback = natural && style_history_ && !collecting_ && !preview_steps_ && prepared_ && selected_candidate_ && *selected_candidate_ < candidates_.size();
    reject_button_->Enable(feedback);
    approve_button_->Enable(feedback && CandidateAllowed());
    adjust_button_->Enable(feedback && CandidateAllowed() && profile_choice_->GetSelection() > 0);
    history_choice_->Enable(natural && !history_ids_.empty());
    rollback_button_->Enable(natural && style_history_ && !history_ids_.empty());
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
		config.surface.ruinPercent = config.surface.buildingDensity ? uint32_t(ruin_percent_spin_->GetValue()) : 0;
		config.surface.ruinFloorPercent = config.surface.ruinPercent ? learned_ruin_floor_ : 0;
		config.surface.compoundPercent = config.surface.buildingDensity ? learned_compound_ : 0;
		config.surface.campCount = uint32_t(camp_count_spin_->GetValue());
		config.surface.fordLength = uint32_t(ford_length_spin_->GetValue());
		config.surface.rockPasses = rock_passes_check_->GetValue() && config.surface.pathLength > 0 ? 1 : 0;
		config.surface.shoreSmoothing = smooth_beaches_check_->GetValue() ? 2 : 0;
		config.surface.repairReach = repair_check_->GetValue() ? 1 : 0;
        if (config.surface.repairReach) request.profile.bridgeBrush = std::string(bridge_brush_choice_->GetValue().utf8_str());
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
	config.surface.caveEntrances = natural ? uint32_t(cave_entrances_spin_->GetValue()) : 0;
	const bool withFloors = natural && (config.surface.buildingDensity > 0 || config.surface.campCount > 0 || config.surface.caveEntrances > 0);
    config.surface.plateauLevels = uint32_t(plateau_levels_spin_->GetValue());
    config.surface.plateauMaxPercent = natural ? uint32_t(plateau_percent_spin_->GetValue()) : 0;
    config.surface.passes = natural ? uint32_t(mountain_passes_spin_->GetValue()) : 0;
    // Keep the component-size filter internal; the dialog enables mountains by area percentage.
    config.surface.plateauMinTop = config.surface.plateauMaxPercent ? mountainMinTopCells : 0;
    if (config.surface.plateauMinTop) {
        request.profile.plateauCliff = std::string(plateau_cliff_choice_->GetValue().utf8_str());
        request.profile.plateauGround = std::string(plateau_ground_choice_->GetValue().utf8_str());
        request.profile.plateauRamp = std::string(plateau_ramp_choice_->GetValue().utf8_str());
    }
    config.surface.caveEntranceStyle = cave_entrance_style_choice_->GetSelection() == 1 ? CaveEntranceStyle::FloorHole : CaveEntranceStyle::RockFace;
	const bool withPaths = natural && config.surface.pathLength > 0;
	request.profile.count = !natural ? GroundProfile::groundRollCount : withFloors ? GroundProfile::surfaceWithFloorCount :
		withPaths ? GroundProfile::surfaceWithPathCount : GroundProfile::surfaceCount;
	wxComboBox* const surfaceWithFloorFields[] = { grass_brush_choice_, dirt_brush_choice_, rock_brush_choice_, shore_brush_choice_, water_brush_choice_, water_brush_choice_, path_brush_choice_, floor_brush_choice_ };
	if (withPaths || withFloors) brushFields = surfaceWithFloorFields;
	for (size_t i = 0; i < request.profile.count; ++i) {
		request.profile.brushes[i] = std::string(brushFields[i]->GetValue().utf8_str());
        if (i == 6 && config.surface.passes && request.profile.brushes[i].empty())
            request.profile.brushes[i] = request.profile.brushes[1];
		if (request.profile.brushes[i].empty()) {
			SetStatus("Choose a ground brush for every terrain field.", true);
			brushFields[i]->SetFocus();
			return std::nullopt;
		}
	}

	if (withFloors && config.surface.buildingDensity > 0 && walls_check_->GetValue()) {
		request.profile.wall = std::string(wall_brush_choice_->GetValue().utf8_str());
		if (request.profile.wall.empty()) {
			SetStatus("Choose a wall brush or untick Walls.", true);
			wall_brush_choice_->SetFocus();
			return std::nullopt;
		}
		if (doors_check_->GetValue()) {
			const wxString kind = door_type_choice_->GetStringSelection();
			request.profile.door = std::string(kind.utf8_str());
		}
        config.surface.buildingStoreys = building_storeys_check_->IsEnabled() && building_storeys_check_->GetValue();
        config.surface.upperFloorPercent = uint32_t(upper_floor_percent_spin_->GetValue());
        if (config.surface.buildingStoreys) request.profile.buildingStairs = std::string(building_stairs_choice_->GetValue().utf8_str());
	}

	if (withFloors && config.surface.campCount > 0) request.profile.campfire = std::string(campfire_brush_choice_->GetValue().utf8_str());
	if (withFloors && config.surface.caveEntrances > 0) {
		request.profile.caveEntrance = std::string(cave_entrance_brush_choice_->GetValue().utf8_str());
		request.profile.caveExit = std::string(cave_exit_brush_choice_->GetValue().utf8_str());
		request.profile.caveGround = std::string(cave_ground_brush_choice_->GetValue().utf8_str());
		request.profile.caveWall = std::string(cave_wall_brush_choice_->GetValue().utf8_str());
        config.surface.caveFarExits = cave_far_exits_check_->GetValue();
        config.surface.caveDoorCorridors = cave_door_corridors_check_->IsEnabled() && cave_door_corridors_check_->GetValue();
        if (config.surface.caveDoorCorridors) request.profile.caveDoor = std::string(cave_door_brush_choice_->GetValue().utf8_str());
        config.surface.buildingDungeons = building_dungeons_check_->IsEnabled() && building_dungeons_check_->GetValue() && !request.profile.door.empty();
        config.surface.dungeonPercent = uint32_t(dungeon_percent_spin_->GetValue());
        if (config.surface.buildingDungeons) {
            request.profile.dungeonFloor = std::string(dungeon_floor_choice_->GetValue().utf8_str());
            request.profile.dungeonWall = std::string(dungeon_wall_choice_->GetValue().utf8_str());
            request.profile.dungeonStairs = std::string(dungeon_stairs_choice_->GetValue().utf8_str());
            request.profile.dungeonDescent = std::string(dungeon_descent_choice_->GetValue().utf8_str());
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

	// Learned mixes of the chosen profile; a brush this client cannot place is left out, as for the palette.
	if (const int picked = profile_choice_->GetSelection(); natural && picked > 0 && static_cast<size_t>(picked) <= profiles_.size()) {
		const auto& profile = profiles_[static_cast<size_t>(picked) - 1].profile;
		for (size_t i = 0, used = 0; i < profile.grassMix.size() && used < GroundProfile::grassMixCount; ++i) {
			const auto& mix = profile.grassMix[i];
			const auto* brush = mix.brush.empty() ? nullptr : g_brushes.getBrush(mix.brush);
			const auto* ground = brush ? brush->as<GroundBrush>() : nullptr;
			if (!ground || ground->getTotalChance() <= 0) continue;
			request.profile.grassMix[used] = mix.brush;
			request.profile.grassMixPermille[used] = uint16_t(mix.permille);
			// The patch vegetation applies with vegetation on; a brush this client lacks falls back to the main one.
			if (config.vegetation) {
				const auto& decor = profile.grassMixDecor[i];
				for (size_t k = 0; k < GroundProfile::decorationCount; ++k) {
					request.profile.grassMixDecorKeep[used][k] = uint16_t(decor.keepPermille[k]);
					if (!decor.brushes[k].empty() && usableDecor(decor.brushes[k])) request.profile.grassMixDecor[used][k] = decor.brushes[k];
				}
			}
			++used;
		}
		if (profile.grassPatchStep) request.profile.grassPatchStep = uint16_t(profile.grassPatchStep);
		const auto copyMix = [&](const auto& from, auto& names, auto& permille) {
			for (size_t i = 0, used = 0; config.vegetation && i < from.size() && used < GroundProfile::treeMixCount; ++i) {
				if (from[i].brush.empty() || !usableDecor(from[i].brush)) continue;
				names[used] = from[i].brush;
				permille[used++] = uint16_t(from[i].permille);
			}
		};
		copyMix(profile.treeMix, request.profile.treeMix, request.profile.treeMixPermille);
		copyMix(profile.bushMix, request.profile.bushMix, request.profile.bushMixPermille);
		if (config.vegetation && !profile.shoreTree.empty() && usableDecor(profile.shoreTree)) request.profile.waterTree = profile.shoreTree;
		for (size_t i = 0, used = 0; config.vegetation && i < profile.interiors.size() && used < GroundProfile::overlayCount; ++i) {
			const auto& interior = profile.interiors[i];
			if (interior.name.empty() || !usableDecor(interior.name) ||
				!rme::procedural::decorItemPassable(rme::procedural::findDecorSource(interior.name, 100000).item(1))) continue;
			request.profile.interior[used] = interior.name;
			request.profile.interiorPpm[used++] = interior.onGroundPpm;
		}
		// The outline is drawn when the sample outlined most of its mountains (at least a quarter of the land beside them).
		request.profile.rockOutline = profile.rockOutlinePermille >= 250;
		request.profile.unreachedCapPermille = profile.unreachedRockPermille;
		// The sample is small, so its share of cut-off ground is a rough figure: a generated map may have up to twice as much.
		request.profile.unreachedWalkPermille = std::min<uint32_t>(1000, profile.unreachedWalkPermille * 2);
		if (config.vegetation && !profile.carpet.name.empty()) {
			const auto* brush = g_brushes.getBrush(profile.carpet.name);
			if (brush && brush->as<CarpetBrush>()) {
				request.profile.carpet = profile.carpet.name;
				request.profile.carpetPpm = profile.carpet.onGroundPpm;
			}
		}
		// A learned item this client has and the generator places as a plain item: no ground, container, teleport, depot or
		// podium (profiles learned before such stacks were left out may still list them).
		const auto placeable = [](uint16_t id) {
			const auto definition = g_item_definitions.get(id);
			return definition && definition.clientId() && !definition.isMetaItem() && !definition.isGroundTile() && !definition.isContainer() &&
				!definition.isDepot() && !definition.isTeleport() && !definition.isPodium();
		};
		// Wall stacks whose items this client has; they come with the walls.
		for (size_t i = 0, used = 0; i < profile.wallStacks.size() && used < GroundProfile::wallStackCount; ++i) {
			const auto& stack = profile.wallStacks[i];
			if (stack.items.size() > 4 || !std::ranges::all_of(stack.items, placeable)) continue;
			auto& placed = request.profile.wallStacks[used++];
			placed.sides = uint8_t(stack.sides);
			placed.ppm = stack.ppm;
			std::ranges::copy(stack.items, placed.items.begin());
		}
		// Structures whose items this client has.
		for (const auto& structure : profile.structures) {
			if (request.profile.structures.size() == GroundProfile::structureCount || structure.width > GroundProfile::structureSide ||
				structure.height > GroundProfile::structureSide || structure.cells.size() > GroundProfile::structureCells) continue;
			GroundProfile::Structure placed {structure.ppm, uint8_t(structure.width), uint8_t(structure.height), {}, structure.water};
			bool usable = true;
			for (const auto& cell : structure.cells) {
				if (cell.items.size() > 8) { usable = false; break; }
				GroundProfile::StructureCell copied {uint8_t(cell.dx), uint8_t(cell.dy), uint16_t(cell.ground), {}};
				if (cell.ground) {
					const auto definition = g_item_definitions.get(uint16_t(cell.ground));
					usable = usable && definition && definition.clientId() && !definition.isMetaItem() && definition.isGroundTile();
				}
				for (size_t k = 0; k < cell.items.size(); ++k) {
					usable = usable && placeable(cell.items[k]);
					copied.items[k] = cell.items[k];
				}
				placed.cells.push_back(copied);
			}
			if (usable) request.profile.structures.push_back(std::move(placed));
		}
		for (size_t i = 0, used = 0; config.vegetation && i < profile.overlays.size() && used < GroundProfile::overlayCount; ++i) {
			const auto& overlay = profile.overlays[i];
			// An overlay one cannot walk through would block the planned paths; it is left out.
			if (overlay.name.empty() || !usableDecor(overlay.name) || !rme::procedural::decorItemPassable(rme::procedural::findDecorSource(overlay.name, 100000).item(1))) continue;
			request.profile.overlay[used] = overlay.name;
			request.profile.overlayTreePpm[used] = overlay.onTreePpm;
			request.profile.overlayGroundPpm[used++] = overlay.onGroundPpm;
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
				SetStatus(wxstr(std::format("The selection is too large. Select an area of at most {} x {} tiles.", maxSide, maxSide)), true);
				return;
			}
			selection_ = std::move(*captured);
		}

		auto request = ReadRequest();
		if (!request) {
			selection_.reset();
			return;
		}

        series_request_ = *request;
        series_revision_ = editor->map.getChangeTracker().getGeneration();
        series_assets_ = g_version.GetAssetRevision();
        candidate_count_ = NaturalSurface() ? uint32_t(candidate_count_spin_->GetValue()) : 1;
        ranking_bands_ = {};
        const auto picked = profile_choice_->GetSelection();
        if (NaturalSurface() && picked > 0 && size_t(picked) <= profiles_.size())
            ranking_bands_ = candidateBandsFor(profiles_[size_t(picked) - 1].profile);
        const uint64_t imageBytes = uint64_t(request->config.region.width) * request->config.region.height * 7 + bitmapOverheadBytes;
        const auto available = candidateWorkingBudget(preparationBudget.maxBytes, candidate_count_,
            selection_ ? selection_->estimatedBytes : 0, imageBytes);
        if (!available) {
            ReleasePreview();
            SetStatus("Complete series exceeds the memory budget. Reduce the area or explicitly choose fewer candidates; N was not reduced.", true);
            return;
        }
        working_bytes_ = *available;
        if (NaturalSurface() && !copy_index_text_->GetValue().empty()) {
            const auto ids = loadedClientAssetIds();
            auto index = ids ? readCopyIndex(std::filesystem::path(copy_index_text_->GetValue().ToStdWstring()),
                ids->client, ids->otbSha256, ids->datSha256, working_bytes_) :
                std::expected<CopyIndex, SimilarityError>(std::unexpected(SimilarityError::BadFormat));
            if (!index || index->estimatedBytes >= working_bytes_) {
                ReleasePreview(); UpdateControlStates();
                const std::filesystem::path path(copy_index_text_->GetValue().ToStdWstring());
                SetStatus(copyIndexNeedsRebuild(path) ?
                    wxstr(std::format("Local similarity {}. Entire series refused.", copyIndexRebuildHint)) :
                    wxString("Local similarity index is unreadable, incompatible, insufficient or exceeds the joint budget. Entire series refused."), true);
                return;
            }
            copy_index_ = std::move(*index);
            working_bytes_ -= copy_index_->estimatedBytes;
        }
        candidates_.reserve(candidate_count_);
        collecting_ = true;
        preview_attempt_ = 1;
        preview_first_seed_ = candidateSeed(request->config.seed, 0);
        preview_fallback_ = false;
        candidate_progress_->SetRange(int(candidate_count_));
        StartCandidate(preview_first_seed_);
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
		ReleasePreview();
		event.Skip();
		return;
	}
	ReleasePreview();
	UpdateControlStates();
	SetStatus("Preview cancelled. Press Preview to try again.", false);
	preview_button_->SetFocus();
}

bool TerrainGeneratorDialog::SeriesCurrent() {
    auto* editor = CurrentTarget();
    if (!editor || !series_request_) return false;
    if (editor->map.getChangeTracker().getGeneration() != series_revision_ ||
        g_version.GetAssetRevision() != series_assets_) return false;
    if (selection_ && !validateGenerationSelection(*editor, *selection_)) return false;
    return true;
}

bool TerrainGeneratorDialog::CandidateAllowed() const {
    return !collecting_ && !preview_steps_ && prepared_ && selected_candidate_ &&
        *selected_candidate_ < candidates_.size() && !candidates_[*selected_candidate_].score.hardFail &&
        !prepared_->copyAssessment.hardFail() && !prepared_->preview.getChanges().empty();
}

bool TerrainGeneratorDialog::StartCandidate(uint64_t seed) {
    if (!SeriesCurrent()) {
        ReleasePreview(); SetStatus(staleMessage, true); UpdateControlStates(); return false;
    }
    prepared_.reset();
    pending_request_ = *series_request_;
    preview_bitmap_ = selected_candidate_ ? candidates_[*selected_candidate_].thumbnail : wxNullBitmap;
    pending_request_->config.seed = seed;
    auto budget = preparationBudget;
    budget.maxBytes = working_bytes_;
    budget.measureCandidate = !ranking_bands_.empty();
    budget.copyIndex = copy_index_ ? &*copy_index_ : nullptr;
    const auto mask = selection_ ? std::span<const uint8_t>(selection_->mask) : std::span<const uint8_t>();
    auto* editor = CurrentTarget();
    auto steps = prepareTerrainSteps(editor->map, pending_request_->config, pending_request_->profile, budget, mask);
    if (!steps) {
        const auto message = describe(steps.error());
        ReleasePreview(); SetStatus(message + " Entire series refused; N was not reduced.", true); UpdateControlStates(); return false;
    }
    preview_steps_.emplace(std::move(*steps));
    candidate_started_ = std::chrono::steady_clock::now();
    SetStatus(wxstr(std::format("{} candidate {}/{}; seed {}, initial {}, attempt {}. Cancel stops the whole series.",
        collecting_ ? "Generating" : "Loading", collecting_ ? candidates_.size() + 1 : *selected_candidate_ + 1,
        candidate_count_, seed, preview_first_seed_, preview_attempt_)), false);
    UpdateControlStates();
    SchedulePreviewTick();
    return true;
}

void TerrainGeneratorDialog::UpdateCandidateLabels() {
    for (size_t i = 0; i < candidates_.size(); ++i) {
        std::string text = std::format("#{}", i);
        if (recommended_candidate_ == i) text += " Recommended";
        if (selected_candidate_ == i) text += " Selected";
        candidate_list_->SetItem(long(i), 0, wxstr(text));
    }
}

void TerrainGeneratorDialog::SelectCandidate(uint32_t index) {
    if (collecting_ || index >= candidates_.size()) return;
    preview_timer_.Stop();
    preview_steps_.reset(); // Requests stop and joins before changing request storage.
    pending_request_.reset();
    prepared_.reset();
    selected_candidate_ = index;
    preview_bitmap_ = candidates_[index].thumbnail;
    preview_panel_->Refresh();
    UpdateCandidateLabels();
    preview_attempt_ = candidates_[index].attempts;
    preview_first_seed_ = candidates_[index].initialSeed;
    StartCandidate(candidates_[index].seed);
}

void TerrainGeneratorDialog::OnCandidateSelected(wxListEvent& event) {
    if (updating_candidates_ || collecting_) return;
    try { SelectCandidate(uint32_t(event.GetIndex())); }
    catch (const std::bad_alloc&) { ReleasePreview(); UpdateControlStates(); SetStatus(memoryMessage, true); }
}

void TerrainGeneratorDialog::OnPreviewTick(wxTimerEvent&) {
    if (!series_request_) return;
    if (!SeriesCurrent()) {
        ReleasePreview(); UpdateControlStates(); SetStatus(staleMessage, true); return;
    }
    if (!preview_steps_) { preview_timer_.StartOnce(100); return; }
    try {
        auto outcome = preview_steps_->advance();
        if (!outcome) { SchedulePreviewTick(); return; }
        preview_steps_.reset();
        auto request = std::move(pending_request_);
        pending_request_.reset();
        auto& result = *outcome;
        if (!result) {
            const auto message = describe(result.error());
            ReleasePreview(); UpdateControlStates();
            SetStatus(message + " Entire series refused; N was not reduced.", true); return;
        }
        if (collecting_ && result->levels && !result->levels->report.allReached() && !preview_fallback_) {
            const auto limit = uint32_t(std::max(1, retry_spin_->GetValue()));
            if (preview_attempt_ < limit || (preview_attempt_ > 1 && request->config.seed != preview_first_seed_)) {
                uint64_t seed = preview_first_seed_;
                if (preview_attempt_ < limit) seed += preview_attempt_++;
                else { preview_fallback_ = true; ++preview_attempt_; }
                result = std::unexpected(PreparationInputError::Cancelled);
                StartCandidate(seed); return;
            }
        }
        const auto target = selection_ ?
            validateSelectedTerrainTarget(*result, *selection_, request->config, request->profile) :
            validateTerrainTarget(*result, request->config, request->profile);
        if (!target) {
            ReleasePreview(); UpdateControlStates(); SetStatus(describe(target.error()), true); return;
        }
        // A profile without learned out-of-reach shares keeps the rule from before they were learned: every place and
        // floor transition reached, single cells not counted.
        const bool learnedReach = request->profile.unreachedCapPermille || request->profile.unreachedWalkPermille;
        const bool hardFail = result->copyAssessment.hardFail() || (result->levels && (learnedReach ?
            candidateHardFail(true, result->levels->plan, result->levels->report, result->levels->repair,
                {request->profile.unreachedCapPermille, request->profile.unreachedWalkPermille}) :
            candidateHardFail(true, result->levels->report, result->levels->repair)));
        const auto fingerprints = candidateFingerprints(*result);
        if (!collecting_) {
            const auto& row = candidates_[*selected_candidate_];
            if (hardFail != row.score.hardFail || fingerprints != row.fingerprints || result->copyAssessment != row.copyAssessment) {
                ReleasePreview(); UpdateControlStates(); SetStatus("Candidate rebuild differs from the validated gallery. Generate a new series.", true); return;
            }
            prepared_ = std::move(*result);
            if (!BuildPreviewBitmap()) { ReleasePreview(); UpdateControlStates(); SetStatus(memoryMessage, true); return; }
            UpdateControlStates();
            const auto [blocking, other] = ShowReport(*request, row.elapsedMs);
            SetStatus(wxstr(std::format("Selected #{}; seed {}, initial {}, {} attempt(s). {}. {} Similarity: {}. {} blocking and {} other warning(s); see Warnings.",
                *selected_candidate_, row.seed, row.initialSeed, row.attempts,
                hardFail ? "hardFail; Place blocked" : "Validated; Place writes this preview as one Undo",
                ranking_bands_.empty() ? "No ranking policy; default is first admissible." : "A / g6-bands-v1 baseline; quality gate I51 failed.",
                copyStateName(row.copyAssessment.state), blocking, other)), hardFail);
            preview_panel_->Refresh(); preview_timer_.StartOnce(100); return;
        }
        Candidate row;
        row.copyAssessment = result->copyAssessment;
        row.initialSeed = preview_first_seed_; row.seed = request->config.seed; row.attempts = preview_attempt_;
        row.fingerprints = fingerprints;
        row.elapsedMs = uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - candidate_started_).count());
        row.score = {hardFail, 0, 0, uint32_t(candidates_.size())};
        if (!ranking_bands_.empty()) {
            if (!result->candidateMetrics) { ReleasePreview(); UpdateControlStates(); SetStatus("Candidate measurement missing. Entire series refused.", true); return; }
            const auto score = scoreCandidateMetrics(*result->candidateMetrics, ranking_bands_, hardFail, row.score.index);
            if (!score) { ReleasePreview(); UpdateControlStates(); SetStatus("Candidate measurement failed. Entire series refused.", true); return; }
            row.score = *score;
        }
        prepared_ = std::move(*result);
        preview_floor_choice_->SetSelection(0);
        if (!BuildPreviewBitmap()) { ReleasePreview(); UpdateControlStates(); SetStatus(memoryMessage, true); return; }
        const int thumb = std::min(128, Theme::Grid(12));
        row.thumbnail = wxBitmap(preview_bitmap_.ConvertToImage().Scale(thumb, thumb, wxIMAGE_QUALITY_NORMAL));
        const auto index = long(candidates_.size());
        const int image = candidate_images_->Add(row.thumbnail);
        candidate_list_->InsertItem(index, wxstr(std::format("#{}", index)), image);
        candidate_list_->SetItem(index, 1, wxstr(std::format("{} / {} / {}", row.seed, row.initialSeed, row.attempts)));
        candidate_list_->SetItem(index, 2, wxstr(std::format("{}; similarity: {}", hardFail ? "hardFail - Place blocked" : "Validated / admissible",
            copyStateName(row.copyAssessment.state))));
        candidates_.push_back(std::move(row));
        prepared_.reset();
        candidate_progress_->SetValue(int(candidates_.size()));
        if (candidates_.size() < candidate_count_) {
            preview_attempt_ = 1; preview_fallback_ = false;
            preview_first_seed_ = candidateSeed(series_request_->config.seed, uint32_t(candidates_.size()));
            StartCandidate(preview_first_seed_); return;
        }
        collecting_ = false;
        std::array<CandidateScore, maxCandidateCount> scores;
        for (size_t i = 0; i < candidates_.size(); ++i) scores[i] = candidates_[i].score;
        recommended_candidate_ = recommendedCandidate(std::span(scores).first(candidates_.size()), !ranking_bands_.empty());
        const uint32_t initial = recommended_candidate_.value_or(0);
        updating_candidates_ = true;
        candidate_list_->SetItemState(long(initial), wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
            wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
        updating_candidates_ = false;
        candidate_list_->SetFocus();
        SelectCandidate(initial);
    } catch (const std::bad_alloc&) {
        ReleasePreview(); UpdateControlStates(); SetStatus(memoryMessage, true);
    }
}

namespace {
std::filesystem::path presetDirectory() { return rme::procedural::styleProfileDirectory() / "presets"; }
// Inputs a preset never holds: the area position belongs to the map, the copy check to a local file.
bool presetExcluded(std::string_view name) {
    return name == "terrain-scope" || name == "terrain-x" || name == "terrain-y" || name == "terrain-z" ||
        name == "terrain-copy-index-choice" || name == "terrain-copy-index";
}
std::string presetValueOf(const wxWindow& control) {
    if (const auto* spin = dynamic_cast<const wxSpinCtrl*>(&control)) return std::to_string(spin->GetValue());
    if (const auto* check = dynamic_cast<const wxCheckBox*>(&control)) return check->GetValue() ? "1" : "0";
    // wxMSW derives wxComboBox from wxChoice: test the combo box first.
    if (const auto* combo = dynamic_cast<const wxComboBox*>(&control)) return combo->GetValue().utf8_string();
    if (const auto* choice = dynamic_cast<const wxChoice*>(&control)) return choice->GetStringSelection().utf8_string();
    if (const auto* field = dynamic_cast<const wxTextCtrl*>(&control)) return field->GetValue().utf8_string();
    return {};
}
// Sets one input as the user would, with the event its handlers listen to; a refusal names the reason.
std::optional<std::string> applyPresetValue(wxWindow& control, const std::string& value) {
    const auto send = [&](wxCommandEvent&& event) { event.SetEventObject(&control); control.GetEventHandler()->ProcessEvent(event); };
    const auto name = control.GetName().utf8_string();
    if (auto* spin = dynamic_cast<wxSpinCtrl*>(&control)) {
        int number = 0;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
        if (error != std::errc() || end != value.data() + value.size() || number < spin->GetMin() || number > spin->GetMax())
            return std::format("{} = \"{}\" is not a number from {} to {}", name, value, spin->GetMin(), spin->GetMax());
        spin->SetValue(number);
        wxSpinEvent event(wxEVT_SPINCTRL, spin->GetId()); event.SetPosition(number); send(std::move(event));
    } else if (auto* check = dynamic_cast<wxCheckBox*>(&control)) {
        if (value != "0" && value != "1") return std::format("{} = \"{}\" is not 0 or 1", name, value);
        check->SetValue(value == "1");
        wxCommandEvent event(wxEVT_CHECKBOX, check->GetId()); event.SetInt(value == "1"); send(std::move(event));
    } else if (auto* combo = dynamic_cast<wxComboBox*>(&control)) { // before wxChoice, its base on wxMSW
        if (!value.empty() && !g_brushes.getBrush(value)) return std::format("{}: brush \"{}\" is not available in the loaded client", name, value);
        combo->Clear();
        if (!value.empty()) { combo->Append(wxString::FromUTF8(value)); combo->SetSelection(0); }
        wxCommandEvent event(wxEVT_COMBOBOX, combo->GetId()); event.SetInt(value.empty() ? -1 : 0); event.SetString(combo->GetValue()); send(std::move(event));
    } else if (auto* choice = dynamic_cast<wxChoice*>(&control)) {
        const int index = choice->FindString(wxString::FromUTF8(value), true);
        if (index == wxNOT_FOUND) return std::format("{} has no entry \"{}\"", name, value);
        choice->SetSelection(index);
        wxCommandEvent event(wxEVT_CHOICE, choice->GetId()); event.SetInt(index); event.SetString(choice->GetString(index)); send(std::move(event));
    } else if (auto* field = dynamic_cast<wxTextCtrl*>(&control)) {
        field->SetValue(wxString::FromUTF8(value)); // sends wxEVT_TEXT
    } else {
        return std::format("{} cannot be set", name);
    }
    return std::nullopt;
}
} // namespace

std::vector<wxWindow*> TerrainGeneratorDialog::PresetControls() const {
    std::vector<wxWindow*> controls;
    const auto visit = [&](const auto& self, const wxWindow& parent) -> void {
        for (wxWindow* child : parent.GetChildren()) {
            const auto name = child->GetName().utf8_string();
            const bool input = dynamic_cast<wxSpinCtrl*>(child) || dynamic_cast<wxCheckBox*>(child) || dynamic_cast<wxChoice*>(child) ||
                dynamic_cast<wxComboBox*>(child) || dynamic_cast<wxTextCtrl*>(child);
            if (input && rme::procedural::validPresetKey(name) && !presetExcluded(name)) controls.push_back(child);
            else if (!input) self(self, *child);
        }
    };
    visit(visit, *sections_);
    return controls;
}

rme::procedural::TerrainPreset TerrainGeneratorDialog::CollectPreset() const {
    rme::procedural::TerrainPreset preset;
    for (const wxWindow* control : PresetControls()) preset.values[control->GetName().utf8_string()] = presetValueOf(*control);
    return preset;
}

std::optional<std::string> TerrainGeneratorDialog::ApplyPreset(const rme::procedural::TerrainPreset& preset) {
    const auto controls = PresetControls();
    for (const auto& [key, value] : preset.values) {
        if (std::ranges::none_of(controls, [&](const wxWindow* control) { return control->GetName().utf8_string() == key; }))
            return std::format("{} is not a setting of this generator", key);
    }
    const auto previous = CollectPreset();
    // A choice can reset later inputs (a profile sets its brushes), so repeat until every value holds.
    const auto settle = [&](const rme::procedural::TerrainPreset& target) -> std::optional<std::string> {
        for (int pass = 0; pass < 3; ++pass) {
            bool changed = false;
            for (wxWindow* control : controls) {
                const auto found = target.values.find(control->GetName().utf8_string());
                if (found == target.values.end() || presetValueOf(*control) == found->second) continue;
                if (auto refused = applyPresetValue(*control, found->second)) return refused;
                changed = true;
            }
            if (!changed) return std::nullopt;
        }
        for (const wxWindow* control : controls) {
            const auto found = target.values.find(control->GetName().utf8_string());
            if (found != target.values.end() && presetValueOf(*control) != found->second)
                return std::format("{} does not keep \"{}\"", found->first, found->second);
        }
        return std::nullopt;
    };
    auto refused = settle(preset);
    if (refused) static_cast<void>(settle(previous));
    return refused;
}

void TerrainGeneratorDialog::LoadPresets(const wxString& select) {
    preset_choice_->Clear();
    preset_choice_->Append("(none)");
    for (const auto& name : rme::procedural::listTerrainPresets(presetDirectory())) preset_choice_->Append(wxstr(name));
    const int index = select.empty() ? wxNOT_FOUND : preset_choice_->FindString(select, true);
    preset_choice_->SetSelection(index == wxNOT_FOUND ? 0 : index);
}

bool TerrainGeneratorDialog::Confirm(const wxString& question) {
    // A wx dialog rather than the native message box, so it follows the dialog's keyboard handling and can be tested.
    wxGenericMessageDialog dialog(this, question, "Terrain generator", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    dialog.SetName("terrain-confirm");
    return dialog.ShowModal() == wxID_YES;
}

void TerrainGeneratorDialog::OnPresetChosen(wxCommandEvent&) {
    const int index = preset_choice_->GetSelection();
    if (index <= 0) return;
    const auto name = preset_choice_->GetString(index);
    const auto preset = rme::procedural::readTerrainPreset(presetDirectory() / (name.utf8_string() + std::string(rme::procedural::terrainPresetExtension)));
    if (!preset) {
        preset_choice_->SetSelection(0);
        SetStatus(wxstr(std::format("Preset \"{}\" not loaded: {}.", name.utf8_string(), rme::procedural::presetErrorText(preset.error()))), true);
        return;
    }
    if (const auto refused = ApplyPreset(*preset)) {
        preset_choice_->SetSelection(0);
        SetStatus(wxstr(std::format("Preset \"{}\" not loaded; settings unchanged. {}.", name.utf8_string(), *refused)), true);
        return;
    }
    preset_name_text_->ChangeValue(name);
    SetStatus(wxstr(std::format("Preset \"{}\" loaded. Press Preview to generate.", name.utf8_string())), false);
}

void TerrainGeneratorDialog::OnPresetSave(wxCommandEvent&) {
    const auto name = preset_name_text_->GetValue().utf8_string();
    const auto preset = CollectPreset();
    auto saved = rme::procedural::saveTerrainPreset(presetDirectory(), name, preset, false);
    if (!saved && saved.error() == rme::procedural::PresetSaveError::Exists) {
        if (!Confirm(wxstr(std::format("Replace the preset \"{}\"?", name)))) { SetStatus("Preset not saved.", false); return; }
        saved = rme::procedural::saveTerrainPreset(presetDirectory(), name, preset, true);
    }
    if (!saved) { SetStatus(wxstr(rme::procedural::presetSaveErrorText(saved.error())), true); return; }
    LoadPresets(wxstr(name));
    SetStatus(wxstr(std::format("Preset \"{}\" saved with {} settings.", name, preset.values.size())), false);
}

void TerrainGeneratorDialog::OnPresetDelete(wxCommandEvent&) {
    const int index = preset_choice_->GetSelection();
    if (index <= 0) { SetStatus("Choose a preset to delete.", true); return; }
    const auto name = preset_choice_->GetString(index).utf8_string();
    if (!Confirm(wxstr(std::format("Delete the preset \"{}\"?", name)))) return;
    if (!rme::procedural::deleteTerrainPreset(presetDirectory(), name)) { SetStatus("Cannot delete the preset.", true); return; }
    LoadPresets();
    SetStatus(wxstr(std::format("Preset \"{}\" deleted.", name)), false);
}

void TerrainGeneratorDialog::OnRandomSeed(wxCommandEvent&) {
    std::random_device device;
    const uint64_t seed = (uint64_t(device()) << 32) | device();
    seed_text_->SetValue(wxstr(std::to_string(seed))); // the edit releases the preview like any other input
}

std::pair<size_t, size_t> TerrainGeneratorDialog::ShowReport(const Request& request, uint64_t elapsedMs) {
    TerrainReportInput input;
    const auto& region = prepared_->snapshot.input.config.region;
    input.width = region.width; input.height = region.height;
    input.surfaceCells = prepared_->surfaceCells;
    if (prepared_->levels) {
        input.plan = &prepared_->levels->plan;
        input.reach = &prepared_->levels->report;
        input.repair = &prepared_->levels->repair;
    }
    // The same rule as the hardFail decision in OnPreviewTick.
    if (request.profile.unreachedCapPermille || request.profile.unreachedWalkPermille)
        input.tolerance = ReachTolerance {request.profile.unreachedCapPermille, request.profile.unreachedWalkPermille};
    std::vector<std::string> aliases;
    if (copy_index_) {
        input.copy = &prepared_->copyAssessment;
        for (const auto& reference : copy_index_->references) aliases.push_back(reference.alias);
        input.aliases = aliases;
    }
    input.memoryBytes = prepared_->memory.totalBytes;
    input.memoryBudget = working_bytes_;
    input.elapsedMs = elapsedMs;
    const auto report = terrainReportOf(input);
    statistics_list_->Freeze(); warnings_list_->Freeze();
    statistics_list_->DeleteAllItems(); warnings_list_->DeleteAllItems();
    for (const auto& [name, value] : report.rows) {
        const long item = statistics_list_->InsertItem(statistics_list_->GetItemCount(), wxstr(name));
        statistics_list_->SetItem(item, 1, wxstr(value));
    }
    // Blocking first, in the order the report gives them.
    for (const bool blocking : {true, false})
        for (const auto& warning : report.warnings) {
            if (warning.blocking != blocking) continue;
            const long item = warnings_list_->InsertItem(warnings_list_->GetItemCount(), blocking ? "Blocking" : "Warning");
            warnings_list_->SetItem(item, 1, wxstr(warning.text));
        }
    if (report.warnings.empty()) warnings_list_->SetItem(warnings_list_->InsertItem(0, "None"), 1, "No warnings");
    statistics_list_->Thaw(); warnings_list_->Thaw();
    const auto blocking = size_t(std::ranges::count_if(report.warnings, [](const TerrainWarning& w) { return w.blocking; }));
    return {blocking, report.warnings.size() - blocking};
}

bool TerrainGeneratorDialog::BuildPreviewBitmap() {
    const auto* selected = &*prepared_;
    const int floor = preview_floor_choice_->GetSelection();
    if (floor == 1 && prepared_->cave) {
        selected = prepared_->cave.get();
    } else {
        const int level = floor - (prepared_->cave ? 1 : 0);
        for (int k = 0; k < level && selected->plateau; ++k) {
            selected = selected->plateau.get();
        }
    }
    const auto& unit = *selected;
    const auto& snapshot = unit.snapshot;
	const auto& region = snapshot.input.config.region;
	const auto& context = snapshot.context;
	const BaseMap& buffer = unit.preview.getBuffer();
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

void TerrainGeneratorDialog::OnValidate(wxCommandEvent&) try {
	if (!prepared_) {
		return;
	}
	if (!CurrentTarget()) {
		ReleasePreview();
		UpdateControlStates();
		return;
	}
	auto request = ReadRequest();
	if (!request) {
		ReleasePreview();
		UpdateControlStates();
		return;
	}
    if (!SeriesCurrent() || !series_request_ || request->config != series_request_->config || request->profile != series_request_->profile) {
        ReleasePreview(); UpdateControlStates(); SetStatus(staleMessage, true); return;
    }
    request->config.seed = prepared_->plan.config.seed;
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
	SetStatus(wxstr(std::format("Preview validated. {} tiles. Similarity: {}.",
		prepared_->preview.getChanges().size(), copyStateName(prepared_->copyAssessment.state))), prepared_->copyAssessment.hardFail());
} catch (const std::bad_alloc&) {
	SetStatus(memoryMessage, true);
}

void TerrainGeneratorDialog::OnPlace(wxCommandEvent&) {
	try {
		if (!prepared_) {
			return;
		}
		if (!CurrentTarget()) {
			ReleasePreview();
			UpdateControlStates();
			return;
		}
		auto request = ReadRequest();
		if (!request) {
			ReleasePreview();
			UpdateControlStates();
			return;
		}
		if (!CandidateAllowed()) {
			SetStatus("Only a validated, admissible candidate can be placed.", true);
			return;
		}
		if (!SeriesCurrent() || !series_request_ || request->config != series_request_->config || request->profile != series_request_->profile) {
			ReleasePreview();
			UpdateControlStates();
			SetStatus(staleMessage, true);
			return;
		}
		request->config.seed = prepared_->plan.config.seed;
		// The commit validates again and copies the owned diff; it never regenerates or reborders.
		const auto receipt = selection_ ?
			DrawOperations::commitSelectedTerrainPreview(*prepared_, *selection_, request->config, request->profile, commitBudget) :
			DrawOperations::commitTerrainPreview(*prepared_, request->config, request->profile, commitBudget);
		if (!receipt) {
			if (const auto* failure = std::get_if<TerrainCommitFailure>(&receipt.error());
				failure && *failure == TerrainCommitFailure::AllocationFailed) {
				SetStatus(describe(*failure), true);
				return;
			}
			const wxString message = describe(receipt.error());
			ReleasePreview();
			UpdateControlStates();
			SetStatus(message, true);
			return;
		}
	} catch (const std::bad_alloc&) {
		SetStatus(memoryMessage, true);
		return;
	}
	EndModal(wxID_OK);
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
