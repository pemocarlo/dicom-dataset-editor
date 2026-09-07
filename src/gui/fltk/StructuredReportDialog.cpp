#include "StructuredReportDialog.hpp"

#include "dicom_editor/core/StructuredReport.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class Fl_Widget;

namespace {
class ReportDialog final : public Fl_Window {
  public:
    ReportDialog(std::vector<dicom_editor::ReportNode> nodes, ReportEditHandler edit)
        : Fl_Window(1100, 720, "Structured Report"), edit_(std::move(edit)), nodes_(std::move(nodes)) {
        tree_ = new Fl_Tree(10, 10, 420, 640);
        tree_->showroot(0);
        tree_->sortorder(FL_TREE_SORT_NONE);
        tree_->callback(selectCallback, this);
        std::vector<Fl_Tree_Item *> parents;
        for (auto &node : nodes_) {
            if (node.depth > parents.size()) {
                throw std::invalid_argument("SR node has no parent.");
            }
            auto *parent = node.depth == 0 ? tree_->root() : parents.at(node.depth - 1);
            const std::string label = node.label + " [" + node.valueType + "]";
            auto *item = tree_->add(parent, label.c_str());
            item->user_data(&node);
            parents.resize(node.depth + 1);
            parents[node.depth] = item;
        }
        detail_ = new Fl_Box(450, 10, 630, 40);
        detail_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
        fields_ = new Fl_Scroll(440, 60, 650, 590);
        fields_->end();
        note_ = new Fl_Box(10, 658, 750, 50,
                           "Apply updates the open dataset. Use Save in the main window to write the file.\n"
                           "Unsupported values remain available in the raw dataset editor.");
        note_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        apply_ = new Fl_Button(800, 674, 130, 30, "Apply node");
        apply_->callback(applyCallback, this);
        apply_->deactivate();
        auto *close = new Fl_Button(950, 674, 130, 30, "Close");
        close->callback(closeCallback, this);
        callback(closeCallback, this);
        end();
        set_modal();
    }

  private:
    bool pending() const {
        if (selected_ == nullptr) {
            return false;
        }
        for (std::size_t index = 0; index < inputs_.size(); ++index) {
            if (selected_->fields[index].value != inputs_[index]->value()) {
                return true;
            }
        }
        return false;
    }

    bool resolvePending() {
        if (!pending()) {
            return true;
        }
        const int choice = fl_choice("Apply changes to the selected SR node?", "Cancel", "Discard", "Apply");
        return choice == 1 || (choice == 2 && apply());
    }

    bool apply() {
        if (selected_ == nullptr) {
            return true;
        }
        std::vector<std::string> values;
        values.reserve(inputs_.size());
        std::ranges::transform(inputs_, std::back_inserter(values),
                               [](const Fl_Multiline_Input *input) { return std::string(input->value()); });
        if (!edit_(selected_->path, values)) {
            return false;
        }
        for (std::size_t index = 0; index < values.size(); ++index) {
            selected_->fields[index].value = values[index];
            if (selected_->fields[index].label == "Name meaning") {
                selected_->label = values[index];
            }
        }
        const std::string label = selected_->label + " [" + selected_->valueType + "]";
        selectedItem_->label(label.c_str());
        tree_->redraw();
        return true;
    }

    void select() {
        auto *item = tree_->callback_item();
        if (tree_->callback_reason() != FL_TREE_REASON_SELECTED || item == nullptr || item->user_data() == nullptr ||
            item == selectedItem_) {
            return;
        }
        if (!resolvePending()) {
            tree_->deselect_all(nullptr, 0);
            if (selectedItem_ != nullptr) {
                tree_->select(selectedItem_, 0);
            }
            return;
        }
        selectedItem_ = item;
        selected_ = static_cast<dicom_editor::ReportNode *>(item->user_data());
        fields_->clear();
        inputs_.clear();
        fields_->scroll_to(0, 0);
        fields_->begin();
        int top = fields_->y() + 10;
        for (const auto &field : selected_->fields) {
            auto *input = new Fl_Multiline_Input(fields_->x() + 175, top, 440, field.label == "Text" ? 140 : 42);
            input->copy_label(field.label.c_str());
            input->value(field.value.c_str());
            inputs_.push_back(input);
            top += input->h() + 12;
        }
        fields_->end();
        Fl_Group::current(nullptr);
        const std::string detail =
            selected_->valueType + " | " + selected_->relationship + (inputs_.empty() ? " | No supported editable fields" : "");
        detail_->copy_label(detail.c_str());
        inputs_.empty() ? apply_->deactivate() : apply_->activate();
        redraw();
    }

    static void selectCallback(Fl_Widget *, void *data) { static_cast<ReportDialog *>(data)->select(); }
    static void applyCallback(Fl_Widget *, void *data) { static_cast<void>(static_cast<ReportDialog *>(data)->apply()); }
    static void closeCallback(Fl_Widget *, void *data) {
        auto &dialog = *static_cast<ReportDialog *>(data);
        if (dialog.resolvePending()) {
            dialog.hide();
        }
    }

    ReportEditHandler edit_;
    std::vector<dicom_editor::ReportNode> nodes_;
    dicom_editor::ReportNode *selected_{};
    Fl_Tree_Item *selectedItem_{};
    Fl_Tree *tree_{};
    Fl_Box *detail_{};
    Fl_Box *note_{};
    Fl_Scroll *fields_{};
    Fl_Button *apply_{};
    std::vector<Fl_Multiline_Input *> inputs_;
};
} // namespace

std::unique_ptr<Fl_Window> createStructuredReportDialog(std::vector<dicom_editor::ReportNode> nodes, ReportEditHandler edit) {
    return std::make_unique<ReportDialog>(std::move(nodes), std::move(edit));
}

void showStructuredReportDialog(std::vector<dicom_editor::ReportNode> nodes, ReportEditHandler edit) {
    auto dialog = createStructuredReportDialog(std::move(nodes), std::move(edit));
    dialog->show();
    while (dialog->shown()) {
        Fl::wait();
    }
}
