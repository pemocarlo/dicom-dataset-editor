#pragma once

#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomEditorService.hpp"

#include <FL/Fl_Double_Window.H>

#include <string>

class DatasetTreePanel;
class Fl_Box;
class Fl_Menu_Bar;
class Fl_Widget;

namespace dicom_editor {
class DicomPath;
}

class MainFrame final : public Fl_Double_Window {
  public:
    MainFrame();
    int handle(int event) override;

  private:
    void RefreshDataset();
    bool ConfirmDiscardChanges();
    bool SaveCurrent();
    bool SaveAs();
    void EditSelectedValue();
    void EditValue(const dicom_editor::DicomPath &path, const std::string &value);
    void AddAttribute();
    void DeleteAttribute();
    void ShowError(const std::string &message);
    void UpdateActions();
    void Exit();

    static void menuCallback(Fl_Widget *widget, void *data);
    static void closeCallback(Fl_Widget *widget, void *data);

    Fl_Menu_Bar *menu_{};
    DatasetTreePanel *datasetPanel_{};
    Fl_Box *status_{};
    dicom_editor::DicomDocument document_;
    dicom_editor::DicomEditorService editor_;
};
