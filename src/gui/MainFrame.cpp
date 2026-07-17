#include "MainFrame.hpp"

#include "AttributeEditDialog.hpp"
#include "DatasetTreePanel.hpp"
#include "dicom_editor/DicomNode.hpp"
#include "dicom_editor/DicomPath.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <wx/defs.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/gdicmn.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/string.h>

#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr int IdEdit = wxID_HIGHEST + 1;
constexpr int IdAdd = IdEdit + 1;
constexpr int IdDelete = IdAdd + 1;

wxString documentTitle(const dicom_editor::DicomDocument &document) {
    const wxString name = document.hasFilePath() ? wxString::FromUTF8(document.filePath().filename().string()) : "Untitled";
    return document.dirty() ? name + "*" : name;
}

} // namespace

MainFrame::MainFrame() : wxFrame(nullptr, wxID_ANY, "DICOM Dataset Editor", wxDefaultPosition, wxSize(1180, 760)) {
    BuildMenus();
    CreateStatusBar();

    datasetPanel_ = new DatasetTreePanel(this);
    datasetPanel_->SetSelectionChangedHandler([this] { UpdateActions(); });
    datasetPanel_->SetValueChangedHandler(
        [this](dicom_editor::DicomPath path, std::string value) { EditValue(std::move(path), std::move(value)); });
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(datasetPanel_, 1, wxEXPAND);
    SetSizer(sizer);

    RefreshDataset();
}

void MainFrame::BuildMenus() {
    auto *file = new wxMenu();
    file->Append(wxID_OPEN, "&Open...\tCtrl+O");
    saveItem_ = file->Append(wxID_SAVE, "&Save\tCtrl+S");
    file->Append(wxID_SAVEAS, "Save &As...\tCtrl+Shift+S");
    file->AppendSeparator();
    file->Append(wxID_EXIT, "E&xit");

    auto *edit = new wxMenu();
    editItem_ = edit->Append(IdEdit, "&Edit Value...\tEnter");
    edit->Append(IdAdd, "&Add Attribute...\tCtrl+N");
    deleteItem_ = edit->Append(IdDelete, "&Delete Attribute\tDel");

    auto *bar = new wxMenuBar();
    bar->Append(file, "&File");
    bar->Append(edit, "&Edit");
    SetMenuBar(bar);

    Bind(wxEVT_MENU, &MainFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_MENU, &MainFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_MENU, &MainFrame::OnSaveAs, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &MainFrame::OnEdit, this, IdEdit);
    Bind(wxEVT_MENU, &MainFrame::OnAdd, this, IdAdd);
    Bind(wxEVT_MENU, &MainFrame::OnDelete, this, IdDelete);
    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
}

void MainFrame::RefreshDataset() {
    datasetPanel_->SetNodes(document_.nodes());
    SetTitle("DICOM Dataset Editor - " + documentTitle(document_));
    SetStatusText(document_.hasFilePath() ? wxString::FromUTF8(document_.filePath().string()) : "New dataset");
    UpdateActions();
}

bool MainFrame::ConfirmDiscardChanges() {
    if (!document_.dirty()) {
        return true;
    }
    const int answer = wxMessageBox("Save changes before continuing?", "Unsaved Changes", wxYES_NO | wxCANCEL | wxICON_WARNING);
    if (answer == wxCANCEL) {
        return false;
    }
    if (answer == wxYES) {
        return SaveCurrent();
    }
    return true;
}

bool MainFrame::SaveCurrent() {
    try {
        if (!document_.hasFilePath()) {
            wxFileDialog dialog(this, "Save DICOM File", "", "", "DICOM files (*.dcm)|*.dcm|All files (*.*)|*.*",
                                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
            if (dialog.ShowModal() != wxID_OK) {
                return false;
            }
            document_.saveAs(dialog.GetPath().ToStdString());
        } else {
            document_.save();
        }

        dicom_editor::DicomDocument verified;
        verified.load(document_.filePath());
        RefreshDataset();
        SetStatusText("Saved and reloaded successfully: " + wxString::FromUTF8(document_.filePath().string()));
        return true;
    } catch (const std::exception &error) {
        ShowError(error.what());
        return false;
    }
}

void MainFrame::ShowError(const std::string &message) {
    wxMessageBox(wxString::FromUTF8(message), "DICOM Dataset Editor", wxOK | wxICON_ERROR);
}

void MainFrame::UpdateActions() {
    const auto *selected = datasetPanel_->SelectedNode();
    saveItem_->Enable(document_.dirty() || !document_.hasFilePath());
    editItem_->Enable(selected != nullptr && selected->editable);
    deleteItem_->Enable(selected != nullptr && selected->editable);
}

void MainFrame::OnOpen(wxCommandEvent &) {
    if (!ConfirmDiscardChanges()) {
        return;
    }

    wxFileDialog dialog(this, "Open DICOM File", "", "", "DICOM files (*.dcm)|*.dcm|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    try {
        document_.load(dialog.GetPath().ToStdString());
        RefreshDataset();
    } catch (const std::exception &error) {
        ShowError(error.what());
    }
}

void MainFrame::OnSave(wxCommandEvent &) { SaveCurrent(); }

void MainFrame::OnSaveAs(wxCommandEvent &) {
    wxFileDialog dialog(this, "Save DICOM File", "", "", "DICOM files (*.dcm)|*.dcm|All files (*.*)|*.*",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    try {
        document_.saveAs(dialog.GetPath().ToStdString());
        dicom_editor::DicomDocument verified;
        verified.load(document_.filePath());
        RefreshDataset();
        SetStatusText("Saved and reloaded successfully: " + wxString::FromUTF8(document_.filePath().string()));
    } catch (const std::exception &error) {
        ShowError(error.what());
    }
}

void MainFrame::OnEdit(wxCommandEvent &) { EditSelectedValue(); }

void MainFrame::EditSelectedValue() {
    const auto *selected = datasetPanel_->SelectedNode();
    if (selected == nullptr || !selected->editable) {
        return;
    }

    auto result = AttributeEditDialog::Edit(this, "Edit " + wxString::FromUTF8(selected->keyword), wxString::FromUTF8(selected->value));
    if (!result) {
        return;
    }

    EditValue(selected->path, result->value);
}

void MainFrame::EditValue(dicom_editor::DicomPath path, std::string value) {
    try {
        editor_.editValue(document_, {.path = std::move(path), .value = std::move(value)});
        RefreshDataset();
    } catch (const std::exception &error) {
        ShowError(error.what());
        RefreshDataset();
    }
}

void MainFrame::OnAdd(wxCommandEvent &) {
    const auto *selected = datasetPanel_->SelectedNode();
    dicom_editor::DicomPath parent = dicom_editor::DicomPath::dataset();
    if (selected != nullptr) {
        parent =
            selected->kind == dicom_editor::DicomNodeKind::Item ? selected->path : dicom_editor::DicomPath::item(selected->path.parents());
    }

    auto result = AttributeEditDialog::Add(this);
    if (!result || !result->tag) {
        return;
    }

    try {
        editor_.addAttribute(document_, {.parentItemPath = parent, .tag = *result->tag, .value = result->value});
        RefreshDataset();
    } catch (const std::exception &error) {
        ShowError(error.what());
    }
}

void MainFrame::OnDelete(wxCommandEvent &) {
    const auto *selected = datasetPanel_->SelectedNode();
    if (selected == nullptr || !selected->editable) {
        return;
    }
    if (wxMessageBox("Delete selected attribute?", "Delete Attribute", wxYES_NO | wxICON_WARNING) != wxYES) {
        return;
    }

    try {
        editor_.deleteAttribute(document_, selected->path);
        RefreshDataset();
    } catch (const std::exception &error) {
        ShowError(error.what());
    }
}

void MainFrame::OnExit(wxCommandEvent &) {
    if (ConfirmDiscardChanges()) {
        Close(true);
    }
}
