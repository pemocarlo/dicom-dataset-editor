#include "AttributeEditDialog.hpp"

#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <charconv>

namespace {

std::optional<unsigned int> parseHex(const std::string &text) {
    unsigned int value{};
    const auto *first = text.data();
    const auto *last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value, 16);
    if (ec != std::errc() || ptr != last || value > 0xffffU) {
        return std::nullopt;
    }
    return value;
}

void addLabeled(wxWindow *parent, wxSizer *sizer, const wxString &label, wxTextCtrl *control) {
    sizer->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    sizer->Add(control, 1, wxEXPAND);
}

} // namespace

std::optional<AttributeDialogResult> AttributeEditDialog::Edit(wxWindow *parent, const wxString &title, const wxString &currentValue) {
    AttributeEditDialog dialog(parent, title, false, currentValue);
    if (dialog.ShowModal() != wxID_OK) {
        return std::nullopt;
    }
    return dialog.Result();
}

std::optional<AttributeDialogResult> AttributeEditDialog::Add(wxWindow *parent) {
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

std::optional<AttributeDialogResult> AttributeEditDialog::Result() const {
    AttributeDialogResult result;
    if (includeTag_) {
        const auto group = parseHex(group_->GetValue().ToStdString());
        const auto element = parseHex(element_->GetValue().ToStdString());
        if (!group || !element) {
            wxMessageBox("Enter a four-digit hexadecimal group and element.", "Invalid Tag", wxOK | wxICON_ERROR);
            return std::nullopt;
        }
        result.tag = DcmTagKey(static_cast<Uint16>(*group), static_cast<Uint16>(*element));
    }
    result.value = value_->GetValue().ToStdString();
    return result;
}
