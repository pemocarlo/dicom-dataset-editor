#include "StructuredReportDialog.hpp"

#include "dicom_editor/core/StructuredReport.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Button.H>
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
class ReportTree final : public Fl_Tree {
  public:
    using Fl_Tree::Fl_Tree;

    int handle(int event) override {
        if (event == FL_MOUSEWHEEL && Fl::event_dy() != 0) {
            const auto *item = first_visible_item();
            if (item != nullptr) {
                const int rowHeight = item->h() + linespacing();
                vposition(vposition() + Fl::event_dy() * rowHeight * 4);
                return 1;
            }
        }
        return Fl_Tree::handle(event);
    }
};

class NodeCreationDialog final : public Fl_Window {
  public:
    explicit NodeCreationDialog(ReportCreateHandler create) : Fl_Window(620, 660, "New SR node"), create_(std::move(create)) {
        type_ = new Fl_Choice(190, 15, 410, 30, "Value type");
        type_->add("TEXT|NUM|CODE|CONTAINER|DATE|TIME|DATETIME|PNAME|UIDREF");
        type_->value(0);
        type_->callback([](Fl_Widget *, void *data) { static_cast<NodeCreationDialog *>(data)->updateFields(); }, this);
        relationship_ = new Fl_Choice(190, 55, 410, 30, "Relationship to parent");
        relationship_->add("CONTAINS|HAS PROPERTIES|HAS OBS CONTEXT|HAS ACQ CONTEXT|HAS CONCEPT MOD|INFERRED FROM|SELECTED FROM");
        relationship_->value(0);
        nameMeaning_ = new Fl_Input(190, 115, 410, 30, "Name meaning");
        nameCode_ = new Fl_Input(190, 155, 410, 30, "Name code");
        nameScheme_ = new Fl_Input(190, 195, 410, 30, "Name coding scheme");
        value_ = new Fl_Multiline_Input(190, 265, 410, 120, "Text");
        code_ = new Fl_Input(190, 410, 410, 30, "Value code");
        scheme_ = new Fl_Input(190, 450, 410, 30, "Value coding scheme");
        meaning_ = new Fl_Input(190, 490, 410, 30, "Value meaning");
        note_ = new Fl_Box(20, 545, 580, 50,
                           "Enter the concept name's code, scheme, and meaning.\n"
                           "The relationship must be allowed for the parent and SR type.");
        note_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
        auto *cancel = new Fl_Button(370, 610, 110, 30, "Cancel");
        cancel->callback([](Fl_Widget *, void *data) { static_cast<NodeCreationDialog *>(data)->hide(); }, this);
        auto *createButton = new Fl_Button(490, 610, 110, 30, "Create node");
        createButton->callback([](Fl_Widget *, void *data) { static_cast<NodeCreationDialog *>(data)->accept(); }, this);
        end();
        set_modal();
        updateFields();
    }

  private:
    void updateFields() {
        const std::string type = type_->text();
        const bool coded = type == "CODE" || type == "NUM";
        const bool scalar = type != "CODE" && type != "CONTAINER";
        scalar ? value_->show() : value_->hide();
        for (auto *input : {code_, scheme_, meaning_}) {
            coded ? input->show() : input->hide();
        }
        code_->label(type == "NUM" ? "Units code" : "Value code");
        scheme_->label(type == "NUM" ? "Units coding scheme" : "Value coding scheme");
        meaning_->label(type == "NUM" ? "Units meaning" : "Value meaning");
        const std::string label = type == "TEXT"       ? "Text"
                                  : type == "NUM"      ? "Number"
                                  : type == "DATE"     ? "Date (YYYYMMDD)"
                                  : type == "TIME"     ? "Time (HHMMSS)"
                                  : type == "DATETIME" ? "Date/time"
                                  : type == "PNAME"    ? "Person name"
                                                       : "UID";
        value_->copy_label(label.c_str());
        redraw();
    }

    void accept() {
        const dicom_editor::ReportNodeInput input{.valueType = type_->text(),
                                                  .relationship = relationship_->text(),
                                                  .nameCode = nameCode_->value(),
                                                  .nameScheme = nameScheme_->value(),
                                                  .nameMeaning = nameMeaning_->value(),
                                                  .value = value_->value(),
                                                  .valueCode = code_->value(),
                                                  .valueScheme = scheme_->value(),
                                                  .valueMeaning = meaning_->value()};
        if (create_(input)) {
            hide();
        }
    }

    ReportCreateHandler create_;
    Fl_Box *note_{};
    Fl_Choice *type_{};
    Fl_Choice *relationship_{};
    Fl_Input *nameMeaning_{};
    Fl_Input *nameCode_{};
    Fl_Input *nameScheme_{};
    Fl_Multiline_Input *value_{};
    Fl_Input *code_{};
    Fl_Input *scheme_{};
    Fl_Input *meaning_{};
};

// Tree scrolling redraws many rows in quick succession. A double-buffered window
// presents each repaint atomically; a plain Fl_Window draws directly to the screen
// and can expose the intermediate erase/draw steps as visible flicker.
class ReportDialog final : public Fl_Double_Window {
  public:
    ReportDialog(std::vector<dicom_editor::ReportNode> nodes, ReportEditHandler edit, ReportStructureHandler structure,
                 ReportReloadHandler reload, ReportInsertHandler insert)
        : Fl_Double_Window(1100, 720, "Structured Report"), edit_(std::move(edit)), structure_(std::move(structure)), reload_(std::move(reload)),
          insert_(std::move(insert)), nodes_(std::move(nodes)) {
        collapseAll_ = new Fl_Button(10, 10, 200, 28, "Collapse all");
        collapseAll_->callback([](Fl_Widget *, void *data) { static_cast<ReportDialog *>(data)->setAllExpanded(false); }, this);
        showAll_ = new Fl_Button(220, 10, 210, 28, "Show all");
        showAll_->callback([](Fl_Widget *, void *data) { static_cast<ReportDialog *>(data)->setAllExpanded(true); }, this);
        tree_ = new ReportTree(10, 45, 420, 560);
        tree_->showroot(0);
        tree_->sortorder(FL_TREE_SORT_NONE);
        tree_->tooltip("Relationships to the parent appear in brackets. Right-click a node to insert a sibling or child.");
        tree_->callback(selectCallback, this);
        rebuildTree();
        add_ = new Fl_Button(10, 615, 200, 30, "Add copy");
        add_->tooltip("Append a copy of the selected node and its children to the same parent.");
        add_->callback([](Fl_Widget *, void *data) { static_cast<ReportDialog *>(data)->changeStructure(false); }, this);
        remove_ = new Fl_Button(220, 615, 210, 30, "Delete subtree...");
        remove_->callback([](Fl_Widget *, void *data) { static_cast<ReportDialog *>(data)->changeStructure(true); }, this);
        add_->deactivate();
        remove_->deactivate();
        divider_ = new Fl_Box(432, 10, 8, 640);
        divider_->box(FL_THIN_UP_BOX);
        divider_->tooltip("Drag to resize the report tree and details");
        detail_ = new Fl_Box(450, 10, 630, 70, "Select a report node to edit its value.");
        detail_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
        names_ = new Fl_Check_Button(450, 85, 300, 25, "Edit concept name (advanced)");
        names_->callback([](Fl_Widget *, void *data) { static_cast<ReportDialog *>(data)->layoutFields(); }, this);
        fields_ = new Fl_Scroll(440, 115, 650, 535);
        fields_->end();
        note_ = new Fl_Box(10, 658, 750, 50,
                           "Apply updates the open dataset. Use Save in the main window to write the file.\n"
                           "Right-click a node to insert a sibling or child.");
        note_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        apply_ = new Fl_Button(800, 674, 130, 30, "Apply node");
        apply_->callback(applyCallback, this);
        apply_->deactivate();
        close_ = new Fl_Button(950, 674, 130, 30, "Close");
        close_->callback(closeCallback, this);
        callback(closeCallback, this);
        end();
        resizable(fields_);
        size_range(900, 600);
        resize(x(), y(), w(), h());
        set_modal();
    }

    void resize(int x, int y, int width, int height) override {
        Fl_Double_Window::resize(x, y, width, height);
        const int split = std::clamp(static_cast<int>(static_cast<double>(width) * splitRatio_), 260, width - 460);
        collapseAll_->resize(10, 10, (split - 30) / 2, 28);
        showAll_->resize(split / 2, 10, split / 2 - 10, 28);
        tree_->resize(10, 45, split - 20, height - 160);
        divider_->resize(split - 8, 10, 8, height - 80);
        add_->resize(10, height - 105, (split - 30) / 2, 30);
        remove_->resize(split / 2, height - 105, split / 2 - 10, 30);
        detail_->resize(split + 10, 10, width - split - 30, 70);
        names_->resize(split + 10, 85, width - split - 30, 25);
        fields_->resize(split, 115, width - split - 10, height - 185);
        note_->resize(10, height - 62, width - 330, 50);
        apply_->resize(width - 300, height - 46, 130, 30);
        close_->resize(width - 150, height - 46, 130, 30);
        layoutFields();
    }

    int handle(int event) override {
        if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE && Fl::event_inside(divider_)) {
            draggingDivider_ = true;
            cursor(FL_CURSOR_WE);
            return 1;
        }
        if (event == FL_DRAG && draggingDivider_) {
            splitRatio_ = static_cast<double>(std::clamp(Fl::event_x(), 260, w() - 460)) / static_cast<double>(w());
            resize(x(), y(), w(), h());
            redraw();
            return 1;
        }
        if (event == FL_RELEASE && draggingDivider_) {
            draggingDivider_ = false;
            cursor(FL_CURSOR_DEFAULT);
            return 1;
        }
        if (event == FL_MOVE) {
            cursor(Fl::event_inside(divider_) ? FL_CURSOR_WE : FL_CURSOR_DEFAULT);
        }
        if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE && Fl::event_inside(tree_)) {
            auto *item = tree_->find_clicked();
            if (item == nullptr || item->user_data() == nullptr) {
                return 1;
            }
            tree_->select_only(item);
            if (selectedItem_ != item) {
                return 1;
            }
            Fl_Menu_Button menu(Fl::event_x(), Fl::event_y(), 0, 0);
            menu.type(Fl_Menu_Button::POPUP3);
            const int unavailable = insert_ && reload_ ? 0 : FL_MENU_INACTIVE;
            const int siblingUnavailable = selected_->path.parents().empty() ? FL_MENU_INACTIVE : unavailable;
            menu.add("Insert before...", 0, nullptr, nullptr, siblingUnavailable);
            menu.add("Insert after...", 0, nullptr, nullptr, siblingUnavailable);
            menu.add("Add child...", 0, nullptr, nullptr, unavailable);
            menu.add("Add copy", 0, nullptr, nullptr, add_->active() ? 0 : FL_MENU_INACTIVE);
            menu.add("Delete subtree...", 0, nullptr, nullptr, remove_->active() ? 0 : FL_MENU_INACTIVE);
            if (menu.popup() != nullptr) {
                switch (menu.value()) {
                case 0:
                    insertNode(dicom_editor::ReportInsertion::Before);
                    break;
                case 1:
                    insertNode(dicom_editor::ReportInsertion::After);
                    break;
                case 2:
                    insertNode(dicom_editor::ReportInsertion::Child);
                    break;
                case 3:
                    changeStructure(false);
                    break;
                case 4:
                    changeStructure(true);
                    break;
                default:
                    break;
                }
            }
            return 1;
        }
        return Fl_Double_Window::handle(event);
    }

  private:
    void setAllExpanded(bool expanded) {
        if (!expanded && tree_->root()->children() > 0) {
            auto *root = tree_->root()->child(0);
            tree_->select_only(root);
            if (selectedItem_ != root) {
                return;
            }
        }
        for (auto *item = tree_->first(); item != nullptr; item = tree_->next(item)) {
            if (item->user_data() != nullptr) {
                expanded ? tree_->open(item, 0) : tree_->close(item, 0);
            }
        }
        if (selectedItem_ != nullptr) {
            tree_->show_item(selectedItem_);
        }
        tree_->redraw();
    }

    static std::string nodeLabel(const dicom_editor::ReportNode &node) {
        const auto &relationship = node.relationship;
        std::string label = (relationship.empty() ? "" : "[" + relationship + "] ") + node.label;
        for (const auto &field : node.fields) {
            if (!field.label.starts_with("Name ") && !field.label.starts_with("Units ") &&
                (!field.label.starts_with("Value ") || field.label == "Value meaning")) {
                auto preview = field.value.substr(0, 60);
                std::ranges::replace(preview, '\n', ' ');
                label += " - " + preview + (field.value.size() > 60 ? "..." : "");
                if (field.label == "Number") {
                    const auto units = std::ranges::find_if(node.fields, [](const dicom_editor::ReportField &entry) {
                        return entry.label == "Units code" || entry.label == "Units meaning";
                    });
                    if (units != node.fields.end()) {
                        label += " " + units->value;
                    }
                }
                break;
            }
        }
        return label;
    }

    void rebuildTree() {
        tree_->clear_children(tree_->root());
        std::vector<Fl_Tree_Item *> parents;
        for (auto &node : nodes_) {
            if (node.depth > parents.size()) {
                throw std::invalid_argument("SR node has no parent.");
            }
            auto *parent = node.depth == 0 ? tree_->root() : parents.at(node.depth - 1);
            const std::string label = nodeLabel(node);
            auto *item = tree_->add(parent, label.c_str());
            item->user_data(&node);
            parents.resize(node.depth + 1);
            parents[node.depth] = item;
        }
    }

    void changeStructure(bool remove) {
        if (selected_ == nullptr || !structure_ || !reload_ || !resolvePending()) {
            return;
        }
        if (remove && fl_choice("Delete '%s' and all its children?", "Cancel", "Delete", nullptr, selected_->label.c_str()) != 1) {
            return;
        }
        dicom_editor::DicomPath selection;
        if (!structure_(selected_->path, remove, selection)) {
            return;
        }
        reloadTree(selection);
    }

    void insertNode(dicom_editor::ReportInsertion placement) {
        if (!selected_ || !insert_ || !reload_ || !resolvePending()) {
            return;
        }
        const auto anchor = selected_->path;
        auto dialog = createReportNodeDialog([&](const dicom_editor::ReportNodeInput &input) {
            dicom_editor::DicomPath selection;
            if (!insert_(anchor, placement, input, selection)) {
                return false;
            }
            reloadTree(selection);
            return true;
        });
        dialog->show();
        while (dialog->shown()) {
            Fl::wait();
        }
    }

    void reloadTree(const dicom_editor::DicomPath &selection) {
        selected_ = nullptr;
        selectedItem_ = nullptr;
        nodes_ = reload_();
        rebuildTree();
        fields_->clear();
        valueHeading_ = nullptr;
        nameHeading_ = nullptr;
        inputs_.clear();
        apply_->deactivate();
        add_->deactivate();
        remove_->deactivate();
        for (auto *item = tree_->first(); item != nullptr; item = tree_->next(item)) {
            const auto *node = static_cast<dicom_editor::ReportNode *>(item->user_data());
            if (node != nullptr && node->path.parents() == selection.parents()) {
                tree_->select_only(item);
                tree_->show_item(item);
                break;
            }
        }
        redraw();
    }

    void layoutFields() {
        if (selected_ == nullptr) {
            return;
        }
        fields_->scroll_to(0, 0);
        int top = fields_->y() + 10;
        for (const bool nameSection : {false, true}) {
            auto *heading = nameSection ? nameHeading_ : valueHeading_;
            if (heading == nullptr) {
                continue;
            }
            const bool visible = !nameSection || names_->value() != 0;
            heading->resize(fields_->x() + 10, top, fields_->w() - 40, 30);
            visible ? heading->show() : heading->hide();
            if (visible) {
                top += 40;
            }
            for (std::size_t index = 0; index < inputs_.size(); ++index) {
                if (selected_->fields[index].label.starts_with("Name ") != nameSection) {
                    continue;
                }
                auto *input = inputs_[index];
                if (!visible) {
                    input->hide();
                } else {
                    input->resize(fields_->x() + 175, top, fields_->w() - 205, input->h());
                    input->show();
                    top += input->h() + 12;
                }
            }
        }
        redraw();
    }

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
        const std::string label = nodeLabel(*selected_);
        selectedItem_->label(label.c_str());
        tree_->redraw();
        updateDetail();
        return true;
    }

    void updateDetail() {
        const std::string detail = "Name: " + selected_->label + "\nType: " + selected_->valueType +
                                   "   |   Relationship: " + (selected_->relationship.empty() ? "Report root" : selected_->relationship);
        detail_->copy_label(detail.c_str());
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
        names_->value(0);
        const bool structural = structure_ && reload_ && !selected_->path.parents().empty();
        structural ? add_->activate() : add_->deactivate();
        structural ? remove_->activate() : remove_->deactivate();
        fields_->clear();
        inputs_.clear();
        fields_->scroll_to(0, 0);
        fields_->begin();
        const bool hasValue = std::ranges::any_of(selected_->fields,
                                                  [](const dicom_editor::ReportField &field) { return !field.label.starts_with("Name "); });
        valueHeading_ = new Fl_Box(0, 0, 100, 30, hasValue ? "Value" : "Value: no editable value on this node");
        valueHeading_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        nameHeading_ = new Fl_Box(0, 0, 100, 30, "Name codes (advanced)");
        nameHeading_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
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
        updateDetail();
        inputs_.empty() ? apply_->deactivate() : apply_->activate();
        layoutFields();
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
    ReportStructureHandler structure_;
    ReportReloadHandler reload_;
    ReportInsertHandler insert_;
    std::vector<dicom_editor::ReportNode> nodes_;
    dicom_editor::ReportNode *selected_{};
    Fl_Tree_Item *selectedItem_{};
    Fl_Tree *tree_{};
    Fl_Box *detail_{};
    Fl_Box *note_{};
    Fl_Scroll *fields_{};
    Fl_Button *apply_{};
    Fl_Button *close_{};
    Fl_Box *valueHeading_{};
    Fl_Box *nameHeading_{};
    Fl_Button *add_{};
    Fl_Button *remove_{};
    Fl_Button *collapseAll_{};
    Fl_Button *showAll_{};
    Fl_Box *divider_{};
    double splitRatio_{0.38};
    bool draggingDivider_{};
    Fl_Check_Button *names_{};
    std::vector<Fl_Multiline_Input *> inputs_;
};
} // namespace

std::unique_ptr<Fl_Window> createStructuredReportDialog(std::vector<dicom_editor::ReportNode> nodes, ReportEditHandler edit,
                                                        ReportStructureHandler structure, ReportReloadHandler reload,
                                                        ReportInsertHandler insert) {
    return std::make_unique<ReportDialog>(std::move(nodes), std::move(edit), std::move(structure), std::move(reload), std::move(insert));
}

std::unique_ptr<Fl_Window> createReportNodeDialog(ReportCreateHandler create) {
    return std::make_unique<NodeCreationDialog>(std::move(create));
}

void showStructuredReportDialog(std::vector<dicom_editor::ReportNode> nodes, ReportEditHandler edit, ReportStructureHandler structure,
                                ReportReloadHandler reload, ReportInsertHandler insert) {
    auto dialog =
        createStructuredReportDialog(std::move(nodes), std::move(edit), std::move(structure), std::move(reload), std::move(insert));
    dialog->show();
    while (dialog->shown()) {
        Fl::wait();
    }
}
