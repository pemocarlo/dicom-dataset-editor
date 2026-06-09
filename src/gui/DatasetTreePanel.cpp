#include "DatasetTreePanel.hpp"

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

#include <algorithm>
#include <cctype>
#include <utility>

namespace {

constexpr int ValueColumn = 5;

std::string lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool containsCaseInsensitive(const std::string &haystack, const std::string &needle) {
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

std::string kindLabel(dicom_editor::DicomNodeKind kind) {
    switch (kind) {
    case dicom_editor::DicomNodeKind::Dataset:
        return "Dataset";
    case dicom_editor::DicomNodeKind::Element:
        return "Element";
    case dicom_editor::DicomNodeKind::Sequence:
        return "Sequence";
    case dicom_editor::DicomNodeKind::Item:
        return "Item";
    }
    return "";
}

wxString indented(const dicom_editor::DicomNode &node) {
    std::string text(static_cast<std::size_t>(node.depth) * 2, ' ');
    text += node.keyword.empty() ? node.tag : node.keyword;
    return wxString::FromUTF8(text);
}

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
    allNodes_ = std::move(nodes);
    Rebuild();
}

const dicom_editor::DicomNode *DatasetTreePanel::SelectedNode() const {
    const int row = list_->GetSelectedRow();
    if (row < 0 || static_cast<std::size_t>(row) >= visibleToNode_.size()) {
        return nullptr;
    }
    return &allNodes_[visibleToNode_[static_cast<std::size_t>(row)]];
}

void DatasetTreePanel::SetSelectionChangedHandler(std::function<void()> handler) { selectionChanged_ = std::move(handler); }

void DatasetTreePanel::SetValueChangedHandler(std::function<void(dicom_editor::DicomPath, std::string)> handler) {
    valueChanged_ = std::move(handler);
}

void DatasetTreePanel::Rebuild() {
    list_->DeleteAllItems();
    visibleToNode_.clear();

    const std::string query = filter_->GetValue().ToStdString();
    for (std::size_t index = 0; index < allNodes_.size(); ++index) {
        const auto &node = allNodes_[index];
        const std::string searchable = node.tag + " " + node.keyword + " " + node.vr + " " + node.valuePreview + " " + node.path.toString();
        if (!query.empty() && !containsCaseInsensitive(searchable, query)) {
            continue;
        }

        wxVector<wxVariant> row;
        row.push_back(wxVariant(indented(node)));
        row.push_back(wxVariant(wxString::FromUTF8(node.tag)));
        row.push_back(wxVariant(wxString::FromUTF8(node.vr)));
        row.push_back(wxVariant(wxString::FromUTF8(node.vm)));
        row.push_back(wxVariant(wxString::FromUTF8(node.path.toString())));
        row.push_back(wxVariant(wxString::FromUTF8(node.value)));
        row.push_back(wxVariant(wxString::FromUTF8(kindLabel(node.kind))));
        list_->AppendItem(row);
        visibleToNode_.push_back(index);
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
    if (event.GetColumn() != ValueColumn || row < 0 || static_cast<std::size_t>(row) >= visibleToNode_.size()) {
        return;
    }

    const auto &node = allNodes_[visibleToNode_[static_cast<std::size_t>(row)]];
    if (node.editable && valueChanged_) {
        valueChanged_(node.path, list_->GetTextValue(static_cast<unsigned int>(row), ValueColumn).ToStdString());
    } else {
        Rebuild();
    }
}
