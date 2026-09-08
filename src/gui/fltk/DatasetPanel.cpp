#include "DatasetPanel.hpp"

#include "dicom_editor/core/DicomPath.hpp"

#include "dicom_editor/core/DatasetViewModel.hpp"
#include "dicom_editor/core/DicomNode.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Table_Row.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <utility>

namespace {

constexpr int Padding = 10;
constexpr int FilterHeight = 32;
constexpr int FilterLabelWidth = 58;
constexpr int TreeControlsHeight = 36;
constexpr int ColumnCount = 5;
constexpr std::array<const char *, ColumnCount> Headers{"Attribute", "Tag", "VR", "VM", "Value"};
constexpr std::array<int, ColumnCount> ColumnWidths{230, 105, 50, 50, 445};

} // namespace

class DatasetFilterInput final : public Fl_Input {
  public:
    DatasetFilterInput(int x, int y, int width, int height, const char *label, DatasetPanel &owner)
        : Fl_Input(x, y, width, height, label), owner_(owner) {}

    int handle(int event) override {
        if (event == FL_KEYDOWN) {
            const int key = Fl::event_key();
            if (key == FL_Escape) {
                owner_.focusRows();
                return 1;
            }
            if (key == FL_Up) {
                owner_.focusRows(-1);
                return 1;
            }
            if (key == FL_Down) {
                owner_.focusRows(1);
                return 1;
            }
        }
        return Fl_Input::handle(event);
    }

  private:
    DatasetPanel &owner_;
};

class DatasetTable final : public Fl_Table_Row {
  public:
    DatasetTable(int x, int y, int width, int height, DatasetPanel &owner) : Fl_Table_Row(x, y, width, height), owner_(owner) {
        cols(ColumnCount);
        type(SELECT_SINGLE);
        selection_color(fl_rgb_color(207, 228, 245));
        col_header(true);
        col_resize(true);
        row_header(false);
        row_resize(false);
        row_height_all(26);
        col_header_height(30);
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
        owner_.selectionChanged();
    }

    void setFontSize(int size) {
        row_height_all(size + 14);
        col_header_height(size + 18);
        redraw();
    }

    [[nodiscard]] int firstVisibleRow() { return row_position(); }

    void restoreFirstVisibleRow(int row) { row_position(std::clamp(row, 0, std::max(0, rows() - 1))); }

    int handle(int event) override {
        if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE && model_ != nullptr) {
            int row = -1;
            int column = -1;
            ResizeFlag resizeFlag = RESIZE_NONE;
            if (cursor2rowcol(row, column, resizeFlag) == CONTEXT_CELL && row >= 0 && column == 0 && resizeFlag == RESIZE_NONE) {
                const auto *node = model_->nodeAt(static_cast<std::size_t>(row));
                if (node != nullptr && node->kind != dicom_editor::DicomNodeKind::Element) {
                    int cellX = 0, cellY = 0, cellWidth = 0, cellHeight = 0;
                    find_cell(CONTEXT_CELL, row, column, cellX, cellY, cellWidth, cellHeight);
                    fl_font(FL_HELVETICA_BOLD, FL_NORMAL_SIZE);
                    const std::string indent(static_cast<std::size_t>(node->depth) * 2, ' ');
                    const int left = cellX + 8 + static_cast<int>(fl_width(indent.c_str()));
                    if (Fl::event_x() >= left && Fl::event_x() <= left + static_cast<int>(fl_width("[+] "))) {
                        select_all_rows(0);
                        select_row(row);
                        move_cursor(row, 0);
                        owner_.toggleSelectedSequence();
                        return 1;
                    }
                }
            }
        }
        if (event == FL_MOUSEWHEEL && Fl::event_dy() != 0) {
            restoreFirstVisibleRow(row_position() + Fl::event_dy() * 4);
            return 1;
        }
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
                const auto *node = owner_.selectedNode();
                if (node != nullptr && node->kind != dicom_editor::DicomNodeKind::Element) {
                    owner_.toggleSelectedSequence();
                } else {
                    owner_.editSelectedValue();
                }
                return 1;
            }
            if (key == FL_Left || key == FL_Right) {
                const auto *node = owner_.selectedNode();
                if (node != nullptr && node->kind != dicom_editor::DicomNodeKind::Element &&
                    owner_.model_.sequenceCollapsed(node->path) == (key == FL_Right)) {
                    owner_.toggleSelectedSequence();
                    return 1;
                }
            }
        }

        const int handled = Fl_Table_Row::handle(event);
        if (event == FL_PUSH && Fl::event_clicks() != 0) {
            const auto *node = owner_.selectedNode();
            if (node != nullptr && node->kind != dicom_editor::DicomNodeKind::Element) {
                owner_.toggleSelectedSequence();
            } else {
                owner_.editSelectedValue();
            }
            return 1;
        }
        return handled;
    }

  private:
    static void tableCallback(Fl_Widget *, void *data) { static_cast<DatasetTable *>(data)->owner_.selectionChanged(); }

    void draw_cell(TableContext context, int row, int column, int x, int y, int width, int height) override {
        switch (context) {
        case CONTEXT_COL_HEADER:
            fl_push_clip(x, y, width, height);
            fl_color(fl_rgb_color(227, 234, 240));
            fl_rectf(x, y, width, height);
            fl_color(fl_rgb_color(42, 62, 78));
            fl_font(FL_HELVETICA_BOLD, FL_NORMAL_SIZE);
            fl_draw(Headers[static_cast<std::size_t>(column)], x + 8, y, width - 12, height, FL_ALIGN_LEFT);
            fl_color(fl_rgb_color(190, 202, 212));
            fl_line(x + width - 1, y, x + width - 1, y + height);
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
        std::string attribute = dicom_editor::DatasetViewModel::attributeLabel(*node);
        if (node->kind != dicom_editor::DicomNodeKind::Element) {
            attribute.insert(static_cast<std::size_t>(node->depth) * 2, model_->sequenceCollapsed(node->path) ? "[+] " : "[-] ");
        }
        const std::array<std::string, ColumnCount> values{std::move(attribute), node->tag, node->vr, node->vm,
                                                          node->valuePreview.empty() ? node->value : node->valuePreview};

        fl_push_clip(x, y, width, height);
        const bool selected = row_selected(row) != 0;
        const Fl_Color normalBackground =
            selected ? selection_color() : (row % 2 == 0 ? fl_rgb_color(252, 253, 254) : fl_rgb_color(244, 248, 251));
        const Fl_Color invalidBackground = selected ? fl_rgb_color(245, 170, 170) : fl_rgb_color(255, 226, 226);
        fl_color(node->invalidValue ? invalidBackground : normalBackground);
        fl_rectf(x, y, width, height);
        fl_color(node->invalidValue ? fl_rgb_color(125, 18, 18) : (selected ? fl_rgb_color(24, 32, 42) : FL_FOREGROUND_COLOR));
        fl_font(column == 0 ? FL_HELVETICA_BOLD : FL_HELVETICA, FL_NORMAL_SIZE);
        fl_draw(values[static_cast<std::size_t>(column)].c_str(), x + 8, y, width - 12, height, FL_ALIGN_LEFT);
        fl_color(fl_rgb_color(224, 230, 235));
        fl_line(x, y + height - 1, x + width, y + height - 1);
        fl_pop_clip();
    }

    DatasetPanel &owner_;
    const dicom_editor::DatasetViewModel *model_{};
};

DatasetPanel::DatasetPanel(int x, int y, int width, int height) : Fl_Group(x, y, width, height) {
    box(FL_FLAT_BOX);
    color(fl_rgb_color(238, 243, 247));
    filter_ = new DatasetFilterInput(x + Padding + FilterLabelWidth, y + Padding, width - 2 * Padding - FilterLabelWidth, FilterHeight,
                                     "Filter:", *this);
    filter_->align(FL_ALIGN_LEFT);
    filter_->box(FL_BORDER_BOX);
    filter_->color(FL_WHITE);
    filter_->textcolor(fl_rgb_color(31, 46, 58));
    filter_->tooltip("Filter by attribute name, tag, VR, VM, or value");
    filter_->when(FL_WHEN_CHANGED);
    filter_->callback(filterCallback, this);

    collapseAll_ = new Fl_Button(x + Padding, y + Padding + FilterHeight + 4, 120, 28, "Collapse all");
    collapseAll_->tooltip("Clear the filter and collapse all sequences and items");
    collapseAll_->callback([](Fl_Widget *, void *data) { static_cast<DatasetPanel *>(data)->setAllExpanded(false); }, this);
    showAll_ = new Fl_Button(x + Padding + 130, y + Padding + FilterHeight + 4, 120, 28, "Show all");
    showAll_->tooltip("Clear the filter and expand all sequences and items");
    showAll_->callback([](Fl_Widget *, void *data) { static_cast<DatasetPanel *>(data)->setAllExpanded(true); }, this);
    const int tableY = y + Padding + FilterHeight + Padding + TreeControlsHeight;
    table_ = new DatasetTable(x + Padding, tableY, width - 2 * Padding, height - (tableY - y) - Padding, *this);
    table_->tooltip("Click [+]/[-] to expand/collapse a branch. Left/Right or Enter also toggle branches.");
    resizable(table_);
    end();
}

void DatasetPanel::setNodes(std::vector<dicom_editor::DicomNode> nodes) {
    const auto *selected = selectedNode();
    const std::string selectedPath = selected == nullptr ? std::string{} : selected->path.toString();
    model_.setNodes(std::move(nodes));
    model_.setFilter(filter_->value());
    table_->setModel(&model_);
    restoreSelection(selectedPath);
    selectionChanged();
}

const dicom_editor::DicomNode *DatasetPanel::selectedNode() const {
    const int row = table_->selectedRow();
    return row < 0 ? nullptr : model_.nodeAt(static_cast<std::size_t>(row));
}

void DatasetPanel::setSelectionChangedHandler(std::function<void()> handler) { selectionChanged_ = std::move(handler); }

void DatasetPanel::setEditRequestedHandler(std::function<void()> handler) { editRequested_ = std::move(handler); }

void DatasetPanel::editSelectedValue() {
    const auto *selected = selectedNode();
    if (selected != nullptr && (selected->editable || !selected->readOnlyValue.empty()) && editRequested_) {
        editRequested_();
    }
}

void DatasetPanel::focusRows(int offset) {
    Fl::focus(table_);
    table_->moveSelection(offset);
}

void DatasetPanel::setFontSize(int size) {
    labelsize(size);
    filter_->labelsize(size);
    filter_->textsize(size);
    collapseAll_->labelsize(size);
    showAll_->labelsize(size);
    table_->setFontSize(size);
}

void DatasetPanel::resize(int x, int y, int width, int height) {
    Fl_Group::resize(x, y, width, height);
    filter_->resize(x + Padding + FilterLabelWidth, y + Padding, width - 2 * Padding - FilterLabelWidth, FilterHeight);
    collapseAll_->resize(x + Padding, y + Padding + FilterHeight + 4, 120, 28);
    showAll_->resize(x + Padding + 130, y + Padding + FilterHeight + 4, 120, 28);
    const int tableY = y + Padding + FilterHeight + Padding + TreeControlsHeight;
    table_->resize(x + Padding, tableY, width - 2 * Padding, height - (tableY - y) - Padding);
}

void DatasetPanel::rebuild() {
    const auto *selected = selectedNode();
    const std::string selectedPath = selected == nullptr ? std::string{} : selected->path.toString();
    model_.setFilter(filter_->value());
    table_->setModel(&model_);
    restoreSelection(selectedPath);
    selectionChanged();
}

void DatasetPanel::restoreSelection(const std::string &path) {
    table_->select_all_rows(0);
    if (path.empty()) {
        return;
    }
    int bestRow = -1;
    std::size_t bestLength = 0;
    for (int row = 0; row < table_->rows(); ++row) {
        const auto *node = model_.nodeAt(static_cast<std::size_t>(row));
        if (node != nullptr) {
            const auto candidate = node->path.toString();
            if (path.starts_with(candidate) && candidate.size() > bestLength) {
                bestRow = row;
                bestLength = candidate.size();
            }
        }
    }
    if (bestRow >= 0) {
        table_->select_row(bestRow);
        table_->move_cursor(bestRow, 0);
    }
}

void DatasetPanel::selectionChanged() {
    if (selectionChanged_) {
        selectionChanged_();
    }
}

void DatasetPanel::toggleSelectedSequence() {
    const auto *selected = selectedNode();
    if (selected == nullptr || selected->kind == dicom_editor::DicomNodeKind::Element) {
        return;
    }
    const int firstVisibleRow = table_->firstVisibleRow();
    const std::string path = selected->path.toString();
    model_.toggleSequence(selected->path);
    table_->setModel(&model_);
    restoreSelection(path);
    table_->restoreFirstVisibleRow(firstVisibleRow);
    selectionChanged();
}

void DatasetPanel::setAllExpanded(bool expanded) {
    const auto *selected = selectedNode();
    const std::string path = selected == nullptr ? std::string{} : selected->path.toString();
    filter_->value("");
    model_.setFilter("");
    expanded ? model_.showAll() : model_.collapseAll();
    table_->setModel(&model_);
    restoreSelection(path);
    selectionChanged();
}

void DatasetPanel::filterCallback(Fl_Widget *, void *data) { static_cast<DatasetPanel *>(data)->rebuild(); }
