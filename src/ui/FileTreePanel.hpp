#pragma once

#include <FL/Fl_Group.H>

#include <cstddef>
#include <functional>
#include <vector>

class Fl_Tree;
class Fl_Widget;

namespace dicom_editor {
struct OpenDicomFile;
}

/// Collapsible patient/study/series/file browser for the open workspace.
class FileTreePanel final : public Fl_Group {
  public:
    FileTreePanel(int x, int y, int width, int height);

    void setFiles(const std::vector<dicom_editor::OpenDicomFile> &files);
    void setActivationHandler(std::function<void(std::size_t)> handler);
    void resize(int x, int y, int width, int height) override;

  private:
    static void treeCallback(Fl_Widget *widget, void *data);

    Fl_Tree *tree_{};
    std::vector<std::size_t> fileIndices_;
    std::function<void(std::size_t)> activationHandler_;
};
