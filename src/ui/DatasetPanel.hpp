#pragma once

#include "dicom_editor/DatasetViewModel.hpp"

#include <FL/Fl_Group.H>

#include <functional>
#include <vector>

class DatasetTable;
class Fl_Input;
class Fl_Widget;

namespace dicom_editor {
struct DicomNode;
} // namespace dicom_editor

class DatasetPanel final : public Fl_Group {
  public:
    DatasetPanel(int x, int y, int width, int height);

    void setNodes(std::vector<dicom_editor::DicomNode> nodes);
    [[nodiscard]] const dicom_editor::DicomNode *selectedNode() const;
    void setSelectionChangedHandler(std::function<void()> handler);
    void setEditRequestedHandler(std::function<void()> handler);
    void editSelectedValue();
    void focusRows(int offset = 0);
    void resize(int x, int y, int width, int height) override;

  private:
    friend class DatasetTable;

    void rebuild();
    void selectionChanged();

    static void filterCallback(Fl_Widget *widget, void *data);

    Fl_Input *filter_{};
    DatasetTable *table_{};
    dicom_editor::DatasetViewModel model_;
    std::function<void()> selectionChanged_;
    std::function<void()> editRequested_;
};
