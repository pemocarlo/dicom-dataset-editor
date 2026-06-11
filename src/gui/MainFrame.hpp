#pragma once

#include "dicom_editor/EditorController.hpp"

#include <wx/frame.h>

#include <string>

class DatasetTreePanel;
class wxCommandEvent;
class wxMenuItem;

namespace dicom_editor {
class DicomPath;
}

class MainFrame final : public wxFrame, private dicom_editor::EditorControllerHost {
  public:
    MainFrame();

  private:
    void BuildMenus();
    [[nodiscard]] std::optional<std::filesystem::path> chooseOpenFile() override;
    [[nodiscard]] std::optional<std::filesystem::path> chooseSaveFile() override;
    [[nodiscard]] dicom_editor::SaveChangesChoice confirmSaveChanges() override;
    [[nodiscard]] bool confirmDelete() override;
    [[nodiscard]] std::optional<dicom_editor::AttributeInput> editAttribute(const std::string &title, const std::string &value) override;
    [[nodiscard]] std::optional<dicom_editor::AttributeInput> addAttribute() override;
    void showError(const std::string &message) override;
    void presentDocument(std::vector<dicom_editor::DicomNode> nodes, const std::string &title, const std::string &status) override;
    void setStatus(const std::string &status) override;
    void updateActions();

    void OnOpen(wxCommandEvent &event);
    void OnSave(wxCommandEvent &event);
    void OnSaveAs(wxCommandEvent &event);
    void OnEdit(wxCommandEvent &event);
    void OnAdd(wxCommandEvent &event);
    void OnDelete(wxCommandEvent &event);
    void OnExit(wxCommandEvent &event);

    DatasetTreePanel *datasetPanel_{};
    dicom_editor::EditorController controller_;
    wxMenuItem *saveItem_{};
    wxMenuItem *editItem_{};
    wxMenuItem *deleteItem_{};
};
