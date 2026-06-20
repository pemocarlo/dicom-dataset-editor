#pragma once

#include "dicom_editor/EditorController.hpp"

#include <FL/Fl_Double_Window.H>

#include <string>

class DatasetPanel;
class Fl_Box;
class Fl_Menu_Bar;
class Fl_Widget;
class PixelDataPanel;

class EditorWindow final : public Fl_Double_Window, private dicom_editor::EditorView {
  public:
    EditorWindow();
    int handle(int event) override;
    void setPixelDataPanelExtent(int extent);
    void setPixelDataPreviewVertical(bool vertical);
    [[nodiscard]] bool pixelDataPreviewVertical() const;

  private:
    [[nodiscard]] std::optional<std::filesystem::path> chooseOpenFile() override;
    [[nodiscard]] std::optional<std::filesystem::path> chooseSaveFile() override;
    [[nodiscard]] dicom_editor::SaveChangesChoice confirmSaveChanges() override;
    [[nodiscard]] bool confirmDelete() override;
    [[nodiscard]] std::optional<dicom_editor::AttributeInput> editAttribute(const std::string &title, const std::string &value) override;
    [[nodiscard]] std::optional<dicom_editor::AttributeInput> addAttribute() override;
    void showError(const std::string &message) override;
    void presentDocument(std::vector<dicom_editor::DicomNode> nodes, const std::string &title, const std::string &status) override;
    void presentPixelData(std::optional<dicom_editor::PixelDataPreview> preview) override;
    void setStatus(const std::string &status) override;
    void updateActions();
    void layoutContent();
    void exit();
    void resize(int x, int y, int width, int height) override;

    static void menuCallback(Fl_Widget *widget, void *data);
    static void closeCallback(Fl_Widget *widget, void *data);

    Fl_Menu_Bar *menu_{};
    DatasetPanel *datasetPanel_{};
    Fl_Widget *pixelSplitter_{};
    PixelDataPanel *pixelDataPanel_{};
    Fl_Box *status_{};
    dicom_editor::EditorController controller_;
    int pixelDataPanelExtent_{280};
    bool pixelDataPreviewVertical_{false};
};
