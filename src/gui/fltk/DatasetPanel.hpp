#pragma once

#include "dicom_editor/core/DatasetViewModel.hpp"

#include <FL/Fl_Group.H>

#include <functional>
#include <string>
#include <vector>

class DatasetTable;
class Fl_Input;
class Fl_Widget;

namespace dicom_editor {
struct DicomNode;
} // namespace dicom_editor

/// Dataset browser panel used by the main window.
class DatasetPanel final : public Fl_Group {
  public:
    /// Creates the panel and its filter and table.
    DatasetPanel(int x, int y, int width, int height);

    /// Updates the displayed rows.
    void setNodes(std::vector<dicom_editor::DicomNode> nodes);
    /// Returns the selected node, or `nullptr` when nothing is selected.
    [[nodiscard]] const dicom_editor::DicomNode *selectedNode() const;
    /// Sets the callback for selection changes.
    void setSelectionChangedHandler(std::function<void()> handler);
    /// Sets the callback for edit requests.
    void setEditRequestedHandler(std::function<void()> handler);
    /// Requests editing for the selected row when possible.
    void editSelectedValue();
    /// Focuses the table and optionally moves the selection.
    void focusRows(int offset = 0);
    /// Applies the current application font size.
    void setFontSize(int size);
    /// Repositions child widgets after resize.
    void resize(int x, int y, int width, int height) override;

  private:
    friend class DatasetTable;

    void rebuild();
    void restoreSelection(const std::string &path);
    void selectionChanged();
    void toggleSelectedSequence();

    static void filterCallback(Fl_Widget *widget, void *data);

    Fl_Input *filter_{};
    DatasetTable *table_{};
    dicom_editor::DatasetViewModel model_;
    std::function<void()> selectionChanged_;
    std::function<void()> editRequested_;
};
