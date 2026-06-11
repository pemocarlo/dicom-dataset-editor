#pragma once

#include "dicom_editor/EditorController.hpp"

#include <FL/Fl_Double_Window.H>

#include <string>

class DatasetTreePanel;
class Fl_Box;
class Fl_Menu_Bar;
class Fl_Widget;

class MainFrame final : public Fl_Double_Window, private dicom_editor::EditorControllerHost {
  public:
    MainFrame();
    int handle(int event) override;

  private:
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
    void exit();

    static void menuCallback(Fl_Widget *widget, void *data);
    static void closeCallback(Fl_Widget *widget, void *data);

    Fl_Menu_Bar *menu_{};
    DatasetTreePanel *datasetPanel_{};
    Fl_Box *status_{};
    dicom_editor::EditorController controller_;
};
