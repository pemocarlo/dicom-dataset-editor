#include "DatasetTreePanel.hpp"

#include "dicom_editor/DicomPath.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Table_Row.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <utility>

namespace {

constexpr int Padding = 6;
constexpr int FilterHeight = 28;
constexpr int FilterLabelWidth = 48;
constexpr int ColumnCount = 7;
constexpr std::array<const char *, ColumnCount> Headers{"Attribute", "Tag", "VR", "VM", "Path", "Value", "Kind"};
constexpr std::array<int, ColumnCount> ColumnWidths{220, 105, 55, 55, 260, 420, 90};

std::string lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool containsCaseInsensitive(const std::string &haystack, const std::string &needle) {
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

std::string kindLabel(dicom_editor::DicomNodeKind kind) {
    switch (kind) {
    case dicom_editor::DicomNodeKind::Dataset:
        return "Dataset";
    case dicom_editor::DicomNodeKind::Element:
        return "Element";
    case dicom_editor::DicomNodeKind::Sequence:
        return "Sequence";
    case dicom_editor::DicomNodeKind::Item:
        return "Item";
    }
    return "";
}

std::string attributeLabel(const dicom_editor::DicomNode &node) {
    std::string text(static_cast<std::size_t>(node.depth) * 2, ' ');
    text += node.keyword.empty() ? node.tag : node.keyword;
    return text;
}

} // namespace

class DatasetTable final : public Fl_Table_Row {
  public:
    DatasetTable(int x, int y, int width, int height, DatasetTreePanel &owner) : Fl_Table_Row(x, y, width, height), owner_(owner) {
        cols(ColumnCount);
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

    void setRows(const std::vector<dicom_editor::DicomNode> *nodes, const std::vector<std::size_t> *visible) {
        nodes_ = nodes;
        visible_ = visible;
        rows(static_cast<int>(visible_->size()));
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

    int handle(int event) override {
        const int handled = Fl_Table_Row::handle(event);
        if ((event == FL_PUSH && Fl::event_clicks() != 0) || (event == FL_KEYDOWN && Fl::event_key() == FL_Enter)) {
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
        if (nodes_ == nullptr || visible_ == nullptr || row < 0 || static_cast<std::size_t>(row) >= visible_->size()) {
            return;
        }

        const auto &node = (*nodes_)[(*visible_)[static_cast<std::size_t>(row)]];
        const std::array<std::string, ColumnCount> values{
            attributeLabel(node), node.tag, node.vr, node.vm, node.path.toString(), node.value, kindLabel(node.kind)};

        fl_push_clip(x, y, width, height);
        const bool selected = row_selected(row) != 0;
        fl_color(selected ? fl_rgb_color(210, 230, 250) : FL_BACKGROUND2_COLOR);
        fl_rectf(x, y, width, height);
        fl_color(selected ? fl_rgb_color(24, 32, 42) : FL_FOREGROUND_COLOR);
        fl_draw(values[static_cast<std::size_t>(column)].c_str(), x + 4, y, width - 8, height, FL_ALIGN_LEFT);
        fl_color(FL_LIGHT2);
        fl_rect(x, y, width, height);
        fl_pop_clip();
    }

    DatasetTreePanel &owner_;
    const std::vector<dicom_editor::DicomNode> *nodes_{};
    const std::vector<std::size_t> *visible_{};
};

DatasetTreePanel::DatasetTreePanel(int x, int y, int width, int height) : Fl_Group(x, y, width, height) {
    filter_ = new Fl_Input(x + Padding + FilterLabelWidth, y + Padding, width - 2 * Padding - FilterLabelWidth, FilterHeight, "Filter:");
    filter_->align(FL_ALIGN_LEFT);
    filter_->when(FL_WHEN_CHANGED);
    filter_->callback(filterCallback, this);

    const int tableY = y + Padding + FilterHeight + Padding;
    table_ = new DatasetTable(x + Padding, tableY, width - 2 * Padding, height - (tableY - y) - Padding, *this);
    resizable(table_);
    end();
}

void DatasetTreePanel::SetNodes(std::vector<dicom_editor::DicomNode> nodes) {
    allNodes_ = std::move(nodes);
    Rebuild();
}

const dicom_editor::DicomNode *DatasetTreePanel::SelectedNode() const {
    const int row = table_->selectedRow();
    if (row < 0 || static_cast<std::size_t>(row) >= visibleToNode_.size()) {
        return nullptr;
    }
    return &allNodes_[visibleToNode_[static_cast<std::size_t>(row)]];
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

void DatasetTreePanel::resize(int x, int y, int width, int height) {
    Fl_Group::resize(x, y, width, height);
    filter_->resize(x + Padding + FilterLabelWidth, y + Padding, width - 2 * Padding - FilterLabelWidth, FilterHeight);
    const int tableY = y + Padding + FilterHeight + Padding;
    table_->resize(x + Padding, tableY, width - 2 * Padding, height - (tableY - y) - Padding);
}

void DatasetTreePanel::Rebuild() {
    visibleToNode_.clear();
    const std::string query = filter_->value();
    for (std::size_t index = 0; index < allNodes_.size(); ++index) {
        const auto &node = allNodes_[index];
        const std::string searchable = node.tag + " " + node.keyword + " " + node.vr + " " + node.valuePreview + " " + node.path.toString();
        if (query.empty() || containsCaseInsensitive(searchable, query)) {
            visibleToNode_.push_back(index);
        }
    }
    table_->setRows(&allNodes_, &visibleToNode_);
    SelectionChanged();
}

void DatasetTreePanel::SelectionChanged() {
    if (selectionChanged_) {
        selectionChanged_();
    }
}

void DatasetTreePanel::filterCallback(Fl_Widget *, void *data) { static_cast<DatasetTreePanel *>(data)->Rebuild(); }
