#pragma once

#include "dicom_editor/application/EditorController.hpp"

#include <FL/Fl_Double_Window.H>

#include <cstddef>
#include <string>

class DatasetPanel;
class FileTreePanel;
class Fl_Box;
class Fl_Button;
class Fl_Menu_Bar;
class Fl_Toggle_Button;
class Fl_Widget;
class PixelDataPanel;

/// Main application window and view implementation.
class EditorWindow final : public Fl_Double_Window, private dicom_editor::EditorView {
  public:
    /// Creates the window and child widgets.
    EditorWindow();
    /// Returns focus to the dataset table on Escape.
    int handle(int event) override;
    /// Sets the preview pane size.
    void setPixelDataPanelExtent(int extent);
    /// Sets the file-tree pane width.
    void setFileTreePanelExtent(int extent);
    /// Shows or hides the open-files tree.
    void setFileTreeVisible(bool visible);
    /// Chooses whether the preview pane is shown below or beside the dataset.
    void setPixelDataPreviewVertical(bool vertical);
    /// Returns `true` when the preview pane is on the right.
    [[nodiscard]] bool pixelDataPreviewVertical() const;
    /// Dispatches menu and toolbar actions.
    static void menuCallback(Fl_Widget *widget, void *data);

  private:
    [[nodiscard]] std::vector<std::filesystem::path> chooseOpenFiles() override;
    [[nodiscard]] std::optional<std::filesystem::path> chooseOpenFolder() override;
    [[nodiscard]] std::optional<std::filesystem::path> chooseDicomDirectory() override;
    [[nodiscard]] std::optional<std::filesystem::path> chooseDataDictionary() override;
    [[nodiscard]] std::optional<std::filesystem::path> chooseSaveFile() override;
    [[nodiscard]] dicom_editor::SaveChangesChoice confirmSaveChanges() override;
    [[nodiscard]] dicom_editor::SaveChangesChoice confirmWorkspaceChanges(std::size_t dirtyCount) override;
    [[nodiscard]] bool confirmDelete() override;
    [[nodiscard]] std::optional<dicom_editor::AttributeInput> editAttribute(const std::string &title, const std::string &value) override;
    void viewAttribute(const std::string &title, const std::string &value) override;
    [[nodiscard]] std::optional<dicom_editor::AttributeInput> addAttribute() override;
    [[nodiscard]] std::optional<dicom_editor::AttributeInput> batchEditAttribute(const dicom_editor::BatchEditReport &report) override;
    [[nodiscard]] dicom_editor::SaveAllReport runSaveAllJob(dicom_editor::SaveAllTask task) override;
    void showError(const std::string &message) override;
    void presentDocument(std::vector<dicom_editor::DicomNode> nodes, const std::string &title, const std::string &status) override;
    void presentOpenFiles(const std::vector<dicom_editor::OpenDicomFile> &files, bool hasLoadedFiles) override;
    void presentPixelData(std::optional<dicom_editor::PixelDataPreview> preview) override;
    void setStatus(const std::string &status) override;
    void updateActions();
    void layoutContent();
    void setUiZoom(int size);
    void exit();
    void resize(int x, int y, int width, int height) override;

    static void closeCallback(Fl_Widget *widget, void *data);

    Fl_Menu_Bar *menu_{};
    Fl_Box *toolbar_{};
    Fl_Button *openButton_{};
    Fl_Button *folderButton_{};
    Fl_Button *saveButton_{};
    Fl_Button *addButton_{};
    Fl_Button *editButton_{};
    Fl_Button *deleteButton_{};
    Fl_Button *previousFileButton_{};
    Fl_Button *nextFileButton_{};
    Fl_Toggle_Button *pixelPreviewButton_{};
    FileTreePanel *fileTreePanel_{};
    Fl_Widget *fileTreeSplitter_{};
    DatasetPanel *datasetPanel_{};
    Fl_Widget *pixelSplitter_{};
    PixelDataPanel *pixelDataPanel_{};
    Fl_Box *status_{};
    dicom_editor::EditorController controller_;
    int pixelDataPanelExtent_{400};
    int fileTreePanelExtent_{300};
    bool pixelDataPreviewVertical_{true};
    bool fileTreeVisible_{};
    bool workspaceHadFiles_{};
    int uiFontSize_{14};
};
