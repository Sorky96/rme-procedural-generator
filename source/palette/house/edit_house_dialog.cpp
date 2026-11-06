//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "palette/house/edit_house_dialog.h"

#include "ui/dialog_util.h"
#include "app/settings.h"
#include "ui/gui.h"

#include "map/map.h"
#include "game/house.h"
#include "game/town.h"
#include "editor/action.h"
#include "editor/action_queue.h"
#include "editor/editor.h"

#include <sstream>

EditHouseDialog::EditHouseDialog(wxWindow* parent, Map* map, House* house) :
	wxDialog(parent, wxID_ANY, "House Properties", wxDefaultPosition, wxSize(250, 160)),
	map(map),
	what_house(house) {
	ASSERT(map);
	ASSERT(house);

	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);
	wxSizer* boxsizer = newd wxStaticBoxSizer(wxVERTICAL, this, "House Properties");
	wxFlexGridSizer* housePropContainer = newd wxFlexGridSizer(2, 10, 10);
	housePropContainer->AddGrowableCol(1);

	wxFlexGridSizer* subsizer = newd wxFlexGridSizer(2, 10, 10);
	subsizer->AddGrowableCol(1);

	house_name = wxstr(house->name);
	house_id = i2ws(house->getID());
	house_rent = i2ws(house->rent);

	subsizer->Add(newd wxStaticText(static_cast<wxStaticBoxSizer*>(boxsizer)->GetStaticBox(), wxID_ANY, "Name:"), wxSizerFlags(0).Border(wxLEFT, 5));
	name_field = newd wxTextCtrl(static_cast<wxStaticBoxSizer*>(boxsizer)->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxSize(160, 20), 0, wxTextValidator(wxFILTER_ASCII, &house_name));
	subsizer->Add(name_field, wxSizerFlags(1).Expand());

	subsizer->Add(newd wxStaticText(static_cast<wxStaticBoxSizer*>(boxsizer)->GetStaticBox(), wxID_ANY, "Town:"), wxSizerFlags(0).Border(wxLEFT, 5));

	const Towns& towns = map->towns;
	town_id_field = newd wxChoice(static_cast<wxStaticBoxSizer*>(boxsizer)->GetStaticBox(), wxID_ANY);
	int to_select_index = 0;
	uint32_t houseTownId = house->townid;

	if (towns.count() > 0) {
		bool found = false;
		for (const auto& [id, town_ptr] : towns) {
			if (town_ptr->getID() == houseTownId) {
				found = true;
			}
			town_id_field->Append(wxstr(town_ptr->getName()));
			town_ids_.push_back(town_ptr->getID());
			if (!found) {
				++to_select_index;
			}
		}

		if (!found) {
			if (houseTownId != 0) {
				town_id_field->Append("Undefined Town (id:" + i2ws(houseTownId) + ")");
				town_ids_.push_back(houseTownId);
				++to_select_index;
			}
		}
	}
	town_id_field->SetSelection(to_select_index);
	subsizer->Add(town_id_field, wxSizerFlags(1).Expand());

	subsizer->Add(newd wxStaticText(static_cast<wxStaticBoxSizer*>(boxsizer)->GetStaticBox(), wxID_ANY, "Rent:"), wxSizerFlags(0).Border(wxLEFT, 5));
	rent_field = newd wxTextCtrl(static_cast<wxStaticBoxSizer*>(boxsizer)->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxSize(160, 20), 0, wxTextValidator(wxFILTER_NUMERIC, &house_rent));
	subsizer->Add(rent_field, wxSizerFlags(1).Expand());

	wxFlexGridSizer* subsizerRight = newd wxFlexGridSizer(1, 10, 10);
	wxFlexGridSizer* houseSizer = newd wxFlexGridSizer(2, 10, 10);

	houseSizer->Add(newd wxStaticText(static_cast<wxStaticBoxSizer*>(boxsizer)->GetStaticBox(), wxID_ANY, "ID:"), wxSizerFlags(0).Center());
	id_field = newd wxSpinCtrl(static_cast<wxStaticBoxSizer*>(boxsizer)->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, FROM_DIP(this, wxSize(64, 20)), wxSP_ARROW_KEYS, 1, 0xFFFF, house->getID());
	houseSizer->Add(id_field, wxSizerFlags(1).Expand());
	subsizerRight->Add(houseSizer, wxSizerFlags(1).Expand());

	wxSizer* checkbox_sub_sizer = newd wxBoxSizer(wxVERTICAL);
	checkbox_sub_sizer->AddSpacer(4);
	guildhall_field = newd wxCheckBox(static_cast<wxStaticBoxSizer*>(boxsizer)->GetStaticBox(), wxID_ANY, "Guildhall");
	checkbox_sub_sizer->Add(guildhall_field);
	subsizerRight->Add(checkbox_sub_sizer);
	guildhall_field->SetValue(house->guildhall);

	housePropContainer->Add(subsizer, wxSizerFlags(5).Expand());
	housePropContainer->Add(subsizerRight, wxSizerFlags(5).Expand());
	boxsizer->Add(housePropContainer, wxSizerFlags(5).Expand().Border(wxTOP | wxBOTTOM, 10));
	topsizer->Add(boxsizer, wxSizerFlags(0).Expand().Border(wxRIGHT | wxLEFT, 20));

	wxSizer* buttonsSizer = newd wxBoxSizer(wxHORIZONTAL);
	buttonsSizer->Add(newd wxButton(this, wxID_OK, "OK"), wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	buttonsSizer->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	topsizer->Add(buttonsSizer, wxSizerFlags(0).Center().Border(wxLEFT | wxRIGHT, 20));

	SetSizerAndFit(topsizer);

	Bind(wxEVT_SET_FOCUS, &EditHouseDialog::OnFocusChange, this);
	Bind(wxEVT_BUTTON, &EditHouseDialog::OnClickOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &EditHouseDialog::OnClickCancel, this, wxID_CANCEL);
}

EditHouseDialog::~EditHouseDialog() {
}

void EditHouseDialog::OnFocusChange(wxFocusEvent& event) {
	wxWindow* win = event.GetWindow();
	if (wxSpinCtrl* spin = dynamic_cast<wxSpinCtrl*>(win)) {
		spin->SetSelection(-1, -1);
	} else if (wxTextCtrl* text = dynamic_cast<wxTextCtrl*>(win)) {
		text->SetSelection(-1, -1);
	}
}

void EditHouseDialog::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	if (Validate() && TransferDataFromWindow()) {
		long new_house_rent = 0;
		if (!house_rent.ToLong(&new_house_rent) || new_house_rent < 0) {
			DialogUtil::PopupDialog(this, "Error", "House rent must be a non-negative number.", wxOK);
			return;
		}

		uint32_t new_house_id = id_field->GetValue();
		if (new_house_id < 1) {
			DialogUtil::PopupDialog(this, "Error", "House id cannot be less than 1.", wxOK);
			return;
		}

		if (house_name.length() == 0) {
			DialogUtil::PopupDialog(this, "Error", "House name cannot be empty.", wxOK);
			return;
		}

		const int selection = town_id_field->GetSelection();
		if (selection == wxNOT_FOUND || selection < 0 || static_cast<size_t>(selection) >= town_ids_.size()) {
			DialogUtil::PopupDialog(this, "Error", "Invalid town selected.", wxOK);
			return;
		}
		const uint32_t new_town_id = town_ids_[selection];

		if (new_house_id != what_house->getID() && map->houses.getHouse(new_house_id)) {
			DialogUtil::PopupDialog(this, "Error", "This house id is already in use.", wxOK);
			return;
		}

		if (g_settings.getInteger(Config::WARN_FOR_DUPLICATE_ID)) {
			Houses& houses = map->houses;
			for (const auto& [house_id, house_ptr] : houses) {
				House* house = house_ptr.get();
				ASSERT(house);

				if (wxstr(house->name) == house_name && house->getID() != what_house->getID()) {
					int ret = DialogUtil::PopupDialog(this, "Warning", "This house name is already in use, are you sure you want to continue?", wxYES | wxNO);
					if (ret == wxID_NO) {
						return;
					}
				}
			}
		}

		if (new_house_id != what_house->getID()) {
			int ret = DialogUtil::PopupDialog(this, "Warning", "Changing existing house ids on a production server WILL HAVE DATABASE CONSEQUENCES such as potential item loss, house owner change or invalidating guest lists.\nYou are doing it at own risk!\n\nAre you ABSOLUTELY sure you want to continue?", wxYES | wxNO);
			if (ret == wxID_NO) {
				return;
			}
		}

		const std::string new_name = nstr(house_name);
		const bool new_guildhall = guildhall_field->GetValue();
		if (new_house_id != what_house->getID() || new_name != what_house->name ||
			new_house_rent != what_house->rent || new_guildhall != what_house->guildhall ||
			new_town_id != what_house->townid) {
			Editor* editor = g_gui.GetCurrentEditor();
			if (!editor || editor->getMap() != map) {
				DialogUtil::PopupDialog(this, "Error", "The map is no longer active.", wxOK);
				return;
			}
			if (editor->live_manager.IsLive()) {
				DialogUtil::PopupDialog(this, "Error", "House metadata changes are not supported in live sessions.", wxOK);
				return;
			}
			HousePropertiesChangeData properties {
				.currentId = what_house->getID(),
				.generation = what_house->getGeneration(),
				.targetId = new_house_id,
				.name = new_name,
				.rent = static_cast<int>(new_house_rent),
				.townId = new_town_id,
				.guildhall = new_guildhall,
			};
			auto action = editor->actionQueue->createAction(ACTION_HOUSE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(properties)));
			editor->addAction(std::move(action));
		}

		EndModal(1);
	}
}

void EditHouseDialog::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(0);
}
