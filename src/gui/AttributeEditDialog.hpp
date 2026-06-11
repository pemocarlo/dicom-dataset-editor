#pragma once

#include "dicom_editor/AttributeInput.hpp"

#include <wx/dialog.h>
#include <wx/string.h>

#include <optional>
#include <string>

class wxTextCtrl;
class wxWindow;

class AttributeEditDialog final : public wxDialog {
  public:
    static std::optional<dicom_editor::AttributeInput> Edit(wxWindow *parent, const wxString &title, const wxString &currentValue);
    static std::optional<dicom_editor::AttributeInput> Add(wxWindow *parent);

  private:
    AttributeEditDialog(wxWindow *parent, const wxString &title, bool includeTag, const wxString &currentValue);

    [[nodiscard]] std::optional<dicom_editor::AttributeInput> Result() const;

    bool includeTag_{};
    wxTextCtrl *group_{};
    wxTextCtrl *element_{};
    wxTextCtrl *value_{};
};
