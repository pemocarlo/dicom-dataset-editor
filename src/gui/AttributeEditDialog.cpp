#include "AttributeEditDialog.hpp"

#include "dicom_editor/AttributeInput.hpp"

#include <wx/defs.h>
#include <wx/gdicmn.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {

void addLabeled(wxWindow *parent, wxSizer *sizer, const wxString &label, wxTextCtrl *control) {
    sizer->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    sizer->Add(control, 1, wxEXPAND);
}

} // namespace

std::optional<dicom_editor::AttributeInput> AttributeEditDialog::Edit(wxWindow *parent, const wxString &title,
                                                                      const wxString &currentValue) {
    AttributeEditDialog dialog(parent, title, false, currentValue);
    if (dialog.ShowModal() != wxID_OK) {
        return std::nullopt;
    }
    return dialog.Result();
}

std::optional<dicom_editor::AttributeInput> AttributeEditDialog::Add(wxWindow *parent) {
    AttributeEditDialog dialog(parent, "Add Attribute", true, "");
    if (dialog.ShowModal() != wxID_OK) {
        return std::nullopt;
    }
    return dialog.Result();
}

AttributeEditDialog::AttributeEditDialog(wxWindow *parent, const wxString &title, bool includeTag, const wxString &currentValue)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(520, 320)), includeTag_(includeTag) {
    auto *outer = new wxBoxSizer(wxVERTICAL);
    auto *form = new wxFlexGridSizer(includeTag ? 3 : 1, 2, 8, 8);
    form->AddGrowableCol(1);

    if (includeTag_) {
        group_ = new wxTextCtrl(this, wxID_ANY);
        element_ = new wxTextCtrl(this, wxID_ANY);
        addLabeled(this, form, "Group (hex)", group_);
        addLabeled(this, form, "Element (hex)", element_);
    }

    value_ = new wxTextCtrl(this, wxID_ANY, currentValue, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    addLabeled(this, form, "Value", value_);

    outer->Add(form, 1, wxEXPAND | wxALL, 12);
    outer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    SetSizerAndFit(outer);
}

std::optional<dicom_editor::AttributeInput> AttributeEditDialog::Result() const {
    dicom_editor::AttributeInput result;
    if (includeTag_) {
        result.tag = dicom_editor::parseTagKey(group_->GetValue().ToStdString(), element_->GetValue().ToStdString());
        if (!result.tag) {
            wxMessageBox("Enter a four-digit hexadecimal group and element.", "Invalid Tag", wxOK | wxICON_ERROR);
            return std::nullopt;
        }
    }
    result.value = value_->GetValue().ToStdString();
    return result;
}
