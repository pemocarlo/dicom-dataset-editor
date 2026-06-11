#include "DatasetTreePanel.hpp"

#include "dicom_editor/DatasetViewModel.hpp"
#include "dicom_editor/DicomNode.hpp"
#include "dicom_editor/DicomPath.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Table_Row.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace {

constexpr int Padding = 6;
constexpr int FilterHeight = 28;
constexpr int FilterLabelWidth = 48;
constexpr int ColumnCount = 7;
constexpr std::array<const char *, ColumnCount> Headers{"Attribute", "Tag", "VR", "VM", "Path", "Value", "Kind"};
constexpr std::array<int, ColumnCount> ColumnWidths{220, 105, 55, 55, 260, 420, 90};

} // namespace

class DatasetFilterInput final : public Fl_Input {
  public:
    DatasetFilterInput(int x, int y, int width, int height, const char *label, DatasetTreePanel &owner)
        : Fl_Input(x, y, width, height, label), owner_(owner) {}

    int handle(int event) override {
        if (event == FL_KEYDOWN) {
            const int key = Fl::event_key();
            if (key == FL_Escape) {
                owner_.FocusRows();
                return 1;
            }
            if (key == FL_Up) {
                owner_.FocusRows(-1);
                return 1;
            }
            if (key == FL_Down) {
                owner_.FocusRows(1);
                return 1;
            }
        }
        return Fl_Input::handle(event);
    }

  private:
    DatasetTreePanel &owner_;
};

class DatasetTable final : public Fl_Table_Row {
  public:
    DatasetTable(int x, int y, int width, int height, DatasetTreePanel &owner) : Fl_Table_Row(x, y, width, height), owner_(owner) {
        cols(ColumnCount);
        type(SELECT_SINGLE);
        selection_color(fl_rgb_color(210, 230, 250));
        col_header(true);
        col_resize(true);
        row_header(false);
        row_resize(false);
        when(FL_WHEN_CHANGED);
        callback(tableCallback, this);
        for (int column = 0; column < ColumnCount; ++column) {
            col_width(column, ColumnWidths[static_cast<std::size_t>(column)]);
        }
    }

    void setModel(const dicom_editor::DatasetViewModel *model) {
        model_ = model;
        rows(static_cast<int>(model_->visibleIndices().size()));
        redraw();
    }

    [[nodiscard]] int selectedRow() {
        for (int row = 0; row < rows(); ++row) {
            if (row_selected(row) != 0) {
                return row;
            }
        }
        return -1;
    }

    void moveSelection(int offset) {
        if (rows() == 0) {
            return;
        }

        const int current = selectedRow();
        const int target = std::clamp(current < 0 ? 0 : current + offset, 0, rows() - 1);
        select_all_rows(0);
        select_row(target);
        move_cursor(target, 0);
        redraw();
        owner_.SelectionChanged();
    }

    int handle(int event) override {
        if (event == FL_KEYDOWN) {
            const int key = Fl::event_key();
            if (key == FL_Up || key == 'k') {
                moveSelection(-1);
                return 1;
            }
            if (key == FL_Down || key == 'j') {
                moveSelection(1);
                return 1;
            }
            if (key == FL_Enter) {
                owner_.EditSelectedValue();
                return 1;
            }
        }

        const int handled = Fl_Table_Row::handle(event);
        if (event == FL_PUSH && Fl::event_clicks() != 0) {
            owner_.EditSelectedValue();
            return 1;
        }
        return handled;
    }

  private:
    static void tableCallback(Fl_Widget *, void *data) { static_cast<DatasetTable *>(data)->owner_.SelectionChanged(); }

    void draw_cell(TableContext context, int row, int column, int x, int y, int width, int height) override {
        switch (context) {
        case CONTEXT_COL_HEADER:
            fl_push_clip(x, y, width, height);
            fl_draw_box(FL_THIN_UP_BOX, x, y, width, height, FL_BACKGROUND_COLOR);
            fl_color(FL_FOREGROUND_COLOR);
            fl_draw(Headers[static_cast<std::size_t>(column)], x + 4, y, width - 8, height, FL_ALIGN_LEFT);
            fl_pop_clip();
            break;
        case CONTEXT_CELL:
            drawDataCell(row, column, x, y, width, height);
            break;
        case CONTEXT_NONE:
        case CONTEXT_STARTPAGE:
        case CONTEXT_ENDPAGE:
        case CONTEXT_ROW_HEADER:
        case CONTEXT_TABLE:
        case CONTEXT_RC_RESIZE:
            break;
        }
    }

    void drawDataCell(int row, int column, int x, int y, int width, int height) {
        if (model_ == nullptr || row < 0) {
            return;
        }

        const auto *node = model_->nodeAt(static_cast<std::size_t>(row));
        if (node == nullptr) {
            return;
        }
        const std::array<std::string, ColumnCount> values{
            dicom_editor::DatasetViewModel::attributeLabel(*node), node->tag, node->vr, node->vm, node->path.toString(), node->value,
            dicom_editor::DatasetViewModel::kindLabel(node->kind)};

        fl_push_clip(x, y, width, height);
        const bool selected = row_selected(row) != 0;
        fl_color(selected ? selection_color() : FL_BACKGROUND2_COLOR);
        fl_rectf(x, y, width, height);
        fl_color(selected ? fl_rgb_color(24, 32, 42) : FL_FOREGROUND_COLOR);
        fl_draw(values[static_cast<std::size_t>(column)].c_str(), x + 4, y, width - 8, height, FL_ALIGN_LEFT);
        fl_color(FL_LIGHT2);
        fl_rect(x, y, width, height);
        fl_pop_clip();
    }

    DatasetTreePanel &owner_;
    const dicom_editor::DatasetViewModel *model_{};
};

DatasetTreePanel::DatasetTreePanel(int x, int y, int width, int height) : Fl_Group(x, y, width, height) {
    filter_ = new DatasetFilterInput(x + Padding + FilterLabelWidth, y + Padding, width - 2 * Padding - FilterLabelWidth, FilterHeight,
                                     "Filter:", *this);
    filter_->align(FL_ALIGN_LEFT);
    filter_->when(FL_WHEN_CHANGED);
    filter_->callback(filterCallback, this);

    const int tableY = y + Padding + FilterHeight + Padding;
    table_ = new DatasetTable(x + Padding, tableY, width - 2 * Padding, height - (tableY - y) - Padding, *this);
    resizable(table_);
    end();
}

void DatasetTreePanel::SetNodes(std::vector<dicom_editor::DicomNode> nodes) {
    model_.setNodes(std::move(nodes));
    Rebuild();
}

const dicom_editor::DicomNode *DatasetTreePanel::SelectedNode() const {
    const int row = table_->selectedRow();
    return row < 0 ? nullptr : model_.nodeAt(static_cast<std::size_t>(row));
}

void DatasetTreePanel::SetSelectionChangedHandler(std::function<void()> handler) { selectionChanged_ = std::move(handler); }

void DatasetTreePanel::SetValueChangedHandler(std::function<void(dicom_editor::DicomPath, std::string)> handler) {
    valueChanged_ = std::move(handler);
}

void DatasetTreePanel::EditSelectedValue() {
    const auto *selected = SelectedNode();
    if (selected != nullptr && selected->editable && valueChanged_) {
        valueChanged_(selected->path, selected->value);
    }
}

void DatasetTreePanel::FocusRows(int offset) {
    Fl::focus(table_);
    table_->moveSelection(offset);
}

void DatasetTreePanel::resize(int x, int y, int width, int height) {
    Fl_Group::resize(x, y, width, height);
    filter_->resize(x + Padding + FilterLabelWidth, y + Padding, width - 2 * Padding - FilterLabelWidth, FilterHeight);
    const int tableY = y + Padding + FilterHeight + Padding;
    table_->resize(x + Padding, tableY, width - 2 * Padding, height - (tableY - y) - Padding);
}

void DatasetTreePanel::Rebuild() {
    model_.setFilter(filter_->value());
    table_->setModel(&model_);
    SelectionChanged();
}

void DatasetTreePanel::SelectionChanged() {
    if (selectionChanged_) {
        selectionChanged_();
    }
}

void DatasetTreePanel::filterCallback(Fl_Widget *, void *data) { static_cast<DatasetTreePanel *>(data)->Rebuild(); }
