#pragma once

#include <FL/Fl_Group.H>

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

class Fl_Tree;
class Fl_Box;
class Fl_Widget;

namespace dicom_editor {
struct BatchEditTarget;
struct OpenDicomFile;
} // namespace dicom_editor

/// Collapsible patient/study/series/file browser for the open workspace.
class FileTreePanel final : public Fl_Group {
  public:
    FileTreePanel(int x, int y, int width, int height);
    ~FileTreePanel() override;

    void setFiles(const std::vector<dicom_editor::OpenDicomFile> &files);
    void setActivationHandler(std::function<void(std::size_t)> handler);
    void setBatchEditHandler(std::function<void(const dicom_editor::BatchEditTarget &)> handler);
    void setFontSize(int size);
    int handle(int event) override;
    void resize(int x, int y, int width, int height) override;

  private:
    static void treeCallback(Fl_Widget *widget, void *data);

    struct TreeItemData;

    Fl_Box *heading_{};
    Fl_Tree *tree_{};
    std::vector<std::unique_ptr<TreeItemData>> itemData_;
    std::function<void(std::size_t)> activationHandler_;
    std::function<void(const dicom_editor::BatchEditTarget &)> batchEditHandler_;
};
