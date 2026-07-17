#include "MainFrame.hpp"

#include "AttributeEditDialog.hpp"
#include "DatasetTreePanel.hpp"
#include "dicom_editor/AttributeInput.hpp"
#include "dicom_editor/EditorController.hpp"

#include <wx/defs.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/gdicmn.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/string.h>

#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr int IdEdit = wxID_HIGHEST + 1;
constexpr int IdAdd = IdEdit + 1;
constexpr int IdDelete = IdAdd + 1;

} // namespace

MainFrame::MainFrame() : wxFrame(nullptr, wxID_ANY, "DICOM Dataset Editor", wxDefaultPosition, wxSize(1180, 760)), controller_(*this) {
    BuildMenus();
    CreateStatusBar();

    datasetPanel_ = new DatasetTreePanel(this);
    datasetPanel_->SetSelectionChangedHandler([this] { updateActions(); });
    datasetPanel_->SetValueChangedHandler([this](dicom_editor::DicomPath path, std::string value) { controller_.editValue(path, value); });
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(datasetPanel_, 1, wxEXPAND);
    SetSizer(sizer);

    controller_.refresh();
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

std::optional<std::filesystem::path> MainFrame::chooseOpenFile() {
    wxFileDialog dialog(this, "Open DICOM File", "", "", "DICOM files (*.dcm)|*.dcm|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    return dialog.ShowModal() == wxID_OK ? std::optional<std::filesystem::path>(dialog.GetPath().ToStdString()) : std::nullopt;
}

std::optional<std::filesystem::path> MainFrame::chooseSaveFile() {
    wxFileDialog dialog(this, "Save DICOM File", "", "", "DICOM files (*.dcm)|*.dcm|All files (*.*)|*.*",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    return dialog.ShowModal() == wxID_OK ? std::optional<std::filesystem::path>(dialog.GetPath().ToStdString()) : std::nullopt;
}

dicom_editor::SaveChangesChoice MainFrame::confirmSaveChanges() {
    const int answer = wxMessageBox("Save changes before continuing?", "Unsaved Changes", wxYES_NO | wxCANCEL | wxICON_WARNING);
    if (answer == wxYES) {
        return dicom_editor::SaveChangesChoice::Save;
    }
    return answer == wxNO ? dicom_editor::SaveChangesChoice::Discard : dicom_editor::SaveChangesChoice::Cancel;
}

bool MainFrame::confirmDelete() {
    return wxMessageBox("Delete selected attribute?", "Delete Attribute", wxYES_NO | wxICON_WARNING) == wxYES;
}

std::optional<dicom_editor::AttributeInput> MainFrame::editAttribute(const std::string &title, const std::string &value) {
    return AttributeEditDialog::Edit(this, wxString::FromUTF8(title), wxString::FromUTF8(value));
}

std::optional<dicom_editor::AttributeInput> MainFrame::addAttribute() { return AttributeEditDialog::Add(this); }

void MainFrame::showError(const std::string &message) {
    wxMessageBox(wxString::FromUTF8(message), "DICOM Dataset Editor", wxOK | wxICON_ERROR);
}

void MainFrame::presentDocument(std::vector<dicom_editor::DicomNode> nodes, const std::string &title, const std::string &status) {
    datasetPanel_->SetNodes(std::move(nodes));
    SetTitle(wxString::FromUTF8(title));
    setStatus(status);
    updateActions();
}

void MainFrame::setStatus(const std::string &status) { SetStatusText(wxString::FromUTF8(status)); }

void MainFrame::updateActions() {
    const auto actions = controller_.actionState(datasetPanel_->SelectedNode());
    saveItem_->Enable(actions.saveEnabled);
    editItem_->Enable(actions.editEnabled);
    deleteItem_->Enable(actions.deleteEnabled);
}

void MainFrame::OnOpen(wxCommandEvent &) { controller_.open(); }
void MainFrame::OnSave(wxCommandEvent &) { controller_.save(); }
void MainFrame::OnSaveAs(wxCommandEvent &) { controller_.saveAs(); }
void MainFrame::OnEdit(wxCommandEvent &) { controller_.editSelected(datasetPanel_->SelectedNode()); }
void MainFrame::OnAdd(wxCommandEvent &) { controller_.add(datasetPanel_->SelectedNode()); }
void MainFrame::OnDelete(wxCommandEvent &) { controller_.remove(datasetPanel_->SelectedNode()); }
void MainFrame::OnExit(wxCommandEvent &) {
    if (controller_.confirmClose()) {
        Close(true);
    }
}
