#pragma once

#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomEditorService.hpp"

#include <wx/frame.h>

#include <string>

class DatasetTreePanel;
class wxCommandEvent;
class wxMenuItem;

namespace dicom_editor {
class DicomPath;
}

class MainFrame final : public wxFrame {
  public:
    MainFrame();

  private:
    void BuildMenus();
    void RefreshDataset();
    bool ConfirmDiscardChanges();
    bool SaveCurrent();
    void EditSelectedValue();
    void EditValue(dicom_editor::DicomPath path, std::string value);
    void ShowError(const std::string &message);
    void UpdateActions();

    void OnOpen(wxCommandEvent &event);
    void OnSave(wxCommandEvent &event);
    void OnSaveAs(wxCommandEvent &event);
    void OnEdit(wxCommandEvent &event);
    void OnAdd(wxCommandEvent &event);
    void OnDelete(wxCommandEvent &event);
    void OnExit(wxCommandEvent &event);

    DatasetTreePanel *datasetPanel_{};
    dicom_editor::DicomDocument document_;
    dicom_editor::DicomEditorService editor_;
    wxMenuItem *saveItem_{};
    wxMenuItem *editItem_{};
    wxMenuItem *deleteItem_{};
};
