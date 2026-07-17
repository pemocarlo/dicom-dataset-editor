#pragma once

#include "dicom_editor/DatasetViewModel.hpp"

#include <FL/Fl_Group.H>

#include <functional>
#include <string>
#include <vector>

class DatasetTable;
class Fl_Input;
class Fl_Widget;

namespace dicom_editor {
class DicomPath;
struct DicomNode;
} // namespace dicom_editor

class DatasetTreePanel final : public Fl_Group {
  public:
    DatasetTreePanel(int x, int y, int width, int height);

    void SetNodes(std::vector<dicom_editor::DicomNode> nodes);
    [[nodiscard]] const dicom_editor::DicomNode *SelectedNode() const;
    void SetSelectionChangedHandler(std::function<void()> handler);
    void SetValueChangedHandler(std::function<void(dicom_editor::DicomPath, std::string)> handler);
    void EditSelectedValue();
    void FocusRows(int offset = 0);
    void resize(int x, int y, int width, int height) override;

  private:
    friend class DatasetTable;

    void Rebuild();
    void SelectionChanged();

    static void filterCallback(Fl_Widget *widget, void *data);

    Fl_Input *filter_{};
    DatasetTable *table_{};
    dicom_editor::DatasetViewModel model_;
    std::function<void()> selectionChanged_;
    std::function<void(dicom_editor::DicomPath, std::string)> valueChanged_;
};
