#pragma once

#include "dicom_editor/DicomNode.hpp"

#include <wx/dataview.h>
#include <wx/panel.h>
#include <wx/srchctrl.h>

#include <functional>
#include <vector>

class DatasetTreePanel final : public wxPanel {
public:
    explicit DatasetTreePanel(wxWindow* parent);

    void SetNodes(std::vector<dicom_editor::DicomNode> nodes);
    [[nodiscard]] const dicom_editor::DicomNode* SelectedNode() const;
    void SetSelectionChangedHandler(std::function<void()> handler);

private:
    void Rebuild();
    void OnFilterChanged(wxCommandEvent& event);
    void OnSelectionChanged(wxDataViewEvent& event);

    wxSearchCtrl* filter_{};
    wxDataViewListCtrl* list_{};
    std::vector<dicom_editor::DicomNode> allNodes_;
    std::vector<std::size_t> visibleToNode_;
    std::function<void()> selectionChanged_;
};
