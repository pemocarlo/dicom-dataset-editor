#include "DatasetTreePanel.hpp"

#include "dicom_editor/DatasetViewModel.hpp"
#include "dicom_editor/DicomPath.hpp"

#include <wx/dataview.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/sizer.h>
#include <wx/srchctrl.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/variant.h>
#include <wx/vector.h>

#include <utility>

namespace {

constexpr int ValueColumn = 5;

} // namespace

DatasetTreePanel::DatasetTreePanel(wxWindow *parent) : wxPanel(parent) {
    filter_ = new wxSearchCtrl(this, wxID_ANY);
    filter_->ShowCancelButton(true);

    list_ = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES | wxDV_VERT_RULES | wxDV_MULTIPLE);
    list_->AppendTextColumn("Attribute", wxDATAVIEW_CELL_INERT, 220, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    list_->AppendTextColumn("Tag", wxDATAVIEW_CELL_INERT, 105, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    list_->AppendTextColumn("VR", wxDATAVIEW_CELL_INERT, 55, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    list_->AppendTextColumn("VM", wxDATAVIEW_CELL_INERT, 55, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    list_->AppendTextColumn("Path", wxDATAVIEW_CELL_INERT, 260, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    list_->AppendTextColumn("Value", wxDATAVIEW_CELL_EDITABLE, 420, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    list_->AppendTextColumn("Kind", wxDATAVIEW_CELL_INERT, 90, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(filter_, 0, wxEXPAND | wxALL, 6);
    sizer->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    SetSizer(sizer);

    filter_->Bind(wxEVT_TEXT, &DatasetTreePanel::OnFilterChanged, this);
    list_->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &DatasetTreePanel::OnSelectionChanged, this);
    list_->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, &DatasetTreePanel::OnValueChanged, this);
}

void DatasetTreePanel::SetNodes(std::vector<dicom_editor::DicomNode> nodes) {
    model_.setNodes(std::move(nodes));
    Rebuild();
}

const dicom_editor::DicomNode *DatasetTreePanel::SelectedNode() const {
    const int row = list_->GetSelectedRow();
    return row < 0 ? nullptr : model_.nodeAt(static_cast<std::size_t>(row));
}

void DatasetTreePanel::SetSelectionChangedHandler(std::function<void()> handler) { selectionChanged_ = std::move(handler); }

void DatasetTreePanel::SetValueChangedHandler(std::function<void(dicom_editor::DicomPath, std::string)> handler) {
    valueChanged_ = std::move(handler);
}

void DatasetTreePanel::Rebuild() {
    list_->DeleteAllItems();
    model_.setFilter(filter_->GetValue().ToStdString());

    for (const std::size_t index : model_.visibleIndices()) {
        const auto &node = model_.nodes()[index];
        wxVector<wxVariant> row;
        row.push_back(wxVariant(wxString::FromUTF8(dicom_editor::DatasetViewModel::attributeLabel(node))));
        row.push_back(wxVariant(wxString::FromUTF8(node.tag)));
        row.push_back(wxVariant(wxString::FromUTF8(node.vr)));
        row.push_back(wxVariant(wxString::FromUTF8(node.vm)));
        row.push_back(wxVariant(wxString::FromUTF8(node.path.toString())));
        row.push_back(wxVariant(wxString::FromUTF8(node.value)));
        row.push_back(wxVariant(wxString::FromUTF8(dicom_editor::DatasetViewModel::kindLabel(node.kind))));
        list_->AppendItem(row);
    }
}

void DatasetTreePanel::OnFilterChanged(wxCommandEvent &) { Rebuild(); }

void DatasetTreePanel::OnSelectionChanged(wxDataViewEvent &) {
    if (selectionChanged_) {
        selectionChanged_();
    }
}

void DatasetTreePanel::OnValueChanged(wxDataViewEvent &event) {
    const int row = list_->ItemToRow(event.GetItem());
    if (event.GetColumn() != ValueColumn || row < 0) {
        return;
    }

    const auto *node = model_.nodeAt(static_cast<std::size_t>(row));
    if (node != nullptr && node->editable && valueChanged_) {
        valueChanged_(node->path, list_->GetTextValue(static_cast<unsigned int>(row), ValueColumn).ToStdString());
    } else {
        Rebuild();
    }
}
