#pragma once

#include "dicom_editor/DatasetViewModel.hpp"

#include <wx/panel.h>

#include <functional>
#include <string>

class wxCommandEvent;
class wxDataViewEvent;
class wxDataViewListCtrl;
class wxSearchCtrl;
class wxWindow;

namespace dicom_editor {
class DicomPath;
}

class DatasetTreePanel final : public wxPanel {
  public:
    explicit DatasetTreePanel(wxWindow *parent);

    void SetNodes(std::vector<dicom_editor::DicomNode> nodes);
    [[nodiscard]] const dicom_editor::DicomNode *SelectedNode() const;
    void SetSelectionChangedHandler(std::function<void()> handler);
    void SetValueChangedHandler(std::function<void(dicom_editor::DicomPath, std::string)> handler);

  private:
    void Rebuild();
    void OnFilterChanged(wxCommandEvent &event);
    void OnSelectionChanged(wxDataViewEvent &event);
    void OnValueChanged(wxDataViewEvent &event);

    wxSearchCtrl *filter_{};
    wxDataViewListCtrl *list_{};
    dicom_editor::DatasetViewModel model_;
    std::function<void()> selectionChanged_;
    std::function<void(dicom_editor::DicomPath, std::string)> valueChanged_;
};
