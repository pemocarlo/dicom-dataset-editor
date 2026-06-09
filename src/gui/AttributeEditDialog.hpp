#pragma once

#include <dcmtk/dcmdata/dctagkey.h>

#include <wx/dialog.h>
#include <wx/string.h>

#include <optional>
#include <string>

class wxTextCtrl;
class wxWindow;

struct AttributeDialogResult {
    std::optional<DcmTagKey> tag;
    std::string value;
};

class AttributeEditDialog final : public wxDialog {
  public:
    static std::optional<AttributeDialogResult> Edit(wxWindow *parent, const wxString &title, const wxString &currentValue);
    static std::optional<AttributeDialogResult> Add(wxWindow *parent);

  private:
    AttributeEditDialog(wxWindow *parent, const wxString &title, bool includeTag, const wxString &currentValue);

    [[nodiscard]] std::optional<AttributeDialogResult> Result() const;

    bool includeTag_{};
    wxTextCtrl *group_{};
    wxTextCtrl *element_{};
    wxTextCtrl *value_{};
};
