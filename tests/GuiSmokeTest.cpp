#include "DatasetPanel.hpp"
#include "FileTreePanel.hpp"
#include "PixelDataPanel.hpp"
#include "StructuredReportDialog.hpp"

#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomNode.hpp"
#include "dicom_editor/core/DicomPath.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"
#include "dicom_editor/core/StructuredReport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("file tree accepts empty and populated models", "[gui][smoke]") {
    FileTreePanel panel(0, 0, 320, 480);
    const std::vector<dicom_editor::OpenDicomFile> files{{
        .index = 0,
        .path = std::filesystem::path{"smoke-test.dcm"},
        .hierarchy = {.patientLabel = "Smoke Patient",
                      .patientId = "PATIENT",
                      .studyLabel = "Smoke Study",
                      .studyId = "STUDY",
                      .seriesLabel = "Smoke Series",
                      .seriesId = "SERIES",
                      .instanceNumber = 1},
        .dirty = true,
        .active = true,
    }};

    panel.setFiles(files);
    panel.setFiles({});
    panel.setFiles(files);
}

TEST_CASE("pixel preview accepts viewer commands", "[gui][smoke]") {
    PixelDataPanel panel(0, 0, 480, 420);
    dicom_editor::PixelDataPreview preview{
        .pixels = std::vector<std::uint8_t>(std::size_t{16} * 12 * 3, 128),
        .message = {},
        .width = 16,
        .height = 12,
        .channels = 3,
        .frameIndex = 0,
        .frameCount = 1,
        .sourceName = "preview.dcm",
        .sourceIndex = 0,
        .sourceCount = 1,
    };

    panel.setPreview(std::move(preview));
    panel.zoomIn();
    panel.zoomOut();
    panel.showActualSize();
    panel.fitImage();
    panel.resize(0, 0, 640, 480);
}

TEST_CASE("SR form keeps duplicate labels distinct and applies selected node values", "[gui][smoke][sr]") {
    using dicom_editor::DicomPath;
    std::vector<dicom_editor::ReportNode> nodes;
    nodes.push_back(
        {.path = DicomPath::dataset(), .depth = 0, .label = "Report", .valueType = "CONTAINER", .relationship = {}, .fields = {}});
    for (unsigned long index = 0; index < 2; ++index) {
        const auto path = DicomPath::item({{.sequenceTag = DCM_ContentSequence, .itemIndex = index}});
        nodes.push_back({.path = path,
                         .depth = 1,
                         .label = "Finding / same name",
                         .valueType = "TEXT",
                         .relationship = "CONTAINS",
                         .fields = {{.path = DicomPath::element(path.parents(), DCM_TextValue), .label = "Text", .value = "Before"}}});
    }
    bool accept = false;
    std::vector<std::string> submitted;
    DicomPath submittedPath;
    auto dialog = createStructuredReportDialog(nodes, [&](const DicomPath &path, const std::vector<std::string> &values) {
        submittedPath = path;
        submitted = values;
        return accept;
    });
    Fl_Tree *tree = nullptr;
    Fl_Scroll *fields = nullptr;
    Fl_Button *apply = nullptr;
    for (int index = 0; index < dialog->children(); ++index) {
        auto *child = dialog->child(index);
        if (auto *candidate = dynamic_cast<Fl_Tree *>(child)) {
            tree = candidate;
        }
        if (auto *candidate = dynamic_cast<Fl_Scroll *>(child)) {
            fields = candidate;
        }
        if (auto *candidate = dynamic_cast<Fl_Button *>(child); candidate != nullptr && std::string(candidate->label()) == "Apply node") {
            apply = candidate;
        }
    }
    REQUIRE(tree != nullptr);
    REQUIRE(fields != nullptr);
    REQUIRE(apply != nullptr);
    if (tree == nullptr || fields == nullptr || apply == nullptr) {
        return;
    }
    auto *root = tree->root()->child(0);
    REQUIRE(root->children() == 2);
    tree->select_only(root->child(1));
    Fl_Multiline_Input *input = nullptr;
    for (int index = 0; index < fields->children(); ++index) {
        if (auto *candidate = dynamic_cast<Fl_Multiline_Input *>(fields->child(index))) {
            input = candidate;
        }
    }
    REQUIRE(input != nullptr);
    if (input == nullptr) {
        return;
    }
    REQUIRE(std::string(input->value()) == "Before");
    input->value("After\nMore text");
    apply->do_callback();
    REQUIRE(submittedPath.parents() == nodes[2].path.parents());
    REQUIRE(submitted == std::vector<std::string>{"After\nMore text"});
    REQUIRE(std::string(input->value()) == "After\nMore text");
    accept = true;
    apply->do_callback();
    tree->select_only(root->child(0));
    tree->select_only(root->child(1));
    for (int index = 0; index < fields->children(); ++index) {
        if (auto *candidate = dynamic_cast<Fl_Multiline_Input *>(fields->child(index))) {
            REQUIRE(std::string(candidate->value()) == "After\nMore text");
        }
    }
}

TEST_CASE("SR hides concept metadata by default and selects newly copied nodes", "[gui][smoke][sr]") {
    using dicom_editor::DicomPath;
    std::vector<dicom_editor::ReportNode> nodes{
        {.path = DicomPath::dataset(), .depth = 0, .label = "Report", .valueType = "CONTAINER", .relationship = {}, .fields = {}},
        {.path = DicomPath::item({{.sequenceTag = DCM_ContentSequence, .itemIndex = 0}}),
         .depth = 1,
         .label = "Finding",
         .valueType = "TEXT",
         .relationship = "HAS PROPERTIES",
         .fields = {{.path = DicomPath::dataset(), .label = "Name meaning", .value = "Finding"},
                    {.path = DicomPath::dataset(), .label = "Text", .value = "Before"}}}};
    auto dialog = createStructuredReportDialog(
        nodes, [](const DicomPath &, const std::vector<std::string> &) { return true; },
        [&](const DicomPath &path, bool remove, DicomPath &selection) {
            REQUIRE_FALSE(remove);
            REQUIRE(path.parents() == nodes[1].path.parents());
            auto copy = nodes[1];
            copy.path = DicomPath::item({{.sequenceTag = DCM_ContentSequence, .itemIndex = 1}});
            selection = copy.path;
            nodes.push_back(std::move(copy));
            return true;
        },
        [&] { return nodes; });
    Fl_Tree *tree = nullptr;
    Fl_Scroll *fields = nullptr;
    Fl_Check_Button *names = nullptr;
    Fl_Button *add = nullptr;
    for (int index = 0; index < dialog->children(); ++index) {
        auto *child = dialog->child(index);
        if (auto *candidate = dynamic_cast<Fl_Tree *>(child)) {
            tree = candidate;
        }
        if (auto *candidate = dynamic_cast<Fl_Scroll *>(child)) {
            fields = candidate;
        }
        if (auto *candidate = dynamic_cast<Fl_Check_Button *>(child)) {
            names = candidate;
        }
        if (auto *candidate = dynamic_cast<Fl_Button *>(child); candidate != nullptr && std::string(candidate->label()) == "Add copy") {
            add = candidate;
        }
    }
    REQUIRE(tree != nullptr);
    REQUIRE(fields != nullptr);
    REQUIRE(names != nullptr);
    REQUIRE(add != nullptr);
    if (tree == nullptr || fields == nullptr || names == nullptr || add == nullptr) {
        return;
    }
    tree->select_only(tree->root()->child(0));
    REQUIRE_FALSE(add->active());
    tree->select_only(tree->root()->child(0)->child(0));
    REQUIRE(std::string(tree->root()->child(0)->child(0)->label()) == "[HAS PROPERTIES] Finding - Before");
    Fl_Multiline_Input *name = nullptr;
    for (int index = 0; index < fields->children(); ++index) {
        auto *input = dynamic_cast<Fl_Multiline_Input *>(fields->child(index));
        if (input != nullptr && std::string(input->label()) == "Name meaning") {
            name = input;
        }
    }
    REQUIRE(name != nullptr);
    if (name == nullptr) {
        return;
    }
    REQUIRE_FALSE(name->visible());
    bool named = false;
    for (int index = 0; index < dialog->children(); ++index) {
        const auto *box = dynamic_cast<Fl_Box *>(dialog->child(index));
        named = named || (box != nullptr && box->label() != nullptr && std::string(box->label()).starts_with("Name: Finding\n"));
    }
    REQUIRE(named);
    REQUIRE(dialog->resizable() != nullptr);
    const int oldWidth = fields->w();
    dialog->resize(dialog->x(), dialog->y(), 1400, 900);
    REQUIRE(fields->w() > oldWidth);
    REQUIRE(fields->h() > 535);
    REQUIRE_FALSE(name->visible());
    names->value(1);
    names->do_callback();
    REQUIRE(name->visible());
    add->do_callback();
    REQUIRE(tree->root()->child(0)->children() == 2);
    REQUIRE(tree->root()->child(0)->child(1)->is_selected());
    REQUIRE(names->value() == 0);
    Fl_Button *collapse = nullptr;
    Fl_Box *divider = nullptr;
    for (int index = 0; index < dialog->children(); ++index) {
        auto *child = dialog->child(index);
        if (auto *button = dynamic_cast<Fl_Button *>(child); button != nullptr && button->label() != nullptr) {
            if (std::string(button->label()) == "Collapse / Expand")
                collapse = button;
        }
        if (auto *box = dynamic_cast<Fl_Box *>(child);
            box != nullptr && box->tooltip() != nullptr && std::string(box->tooltip()) == "Drag to resize the report tree and details")
            divider = box;
    }
    REQUIRE(collapse != nullptr);
    REQUIRE(divider != nullptr);
    if (!collapse || !divider)
        return;
    collapse->do_callback();
    REQUIRE(tree->root()->child(0)->is_close());
    REQUIRE(tree->root()->child(0)->is_selected());
    collapse->do_callback();
    REQUIRE(tree->root()->child(0)->is_open());
    const int treeWidth = tree->w();
    const int oldX = Fl::e_x;
    const int oldY = Fl::e_y;
    const int oldKey = Fl::e_keysym;
    Fl::e_x = divider->x() + 2;
    Fl::e_y = divider->y() + 20;
    Fl::e_keysym = FL_Button + FL_LEFT_MOUSE;
    REQUIRE(dialog->handle(FL_PUSH) == 1);
    Fl::e_x += 100;
    REQUIRE(dialog->handle(FL_DRAG) == 1);
    REQUIRE(dialog->handle(FL_RELEASE) == 1);
    Fl::e_x = oldX;
    Fl::e_y = oldY;
    Fl::e_keysym = oldKey;
    REQUIRE(tree->w() > treeWidth);
}

TEST_CASE("dataset collapse controls retain a visible selection", "[gui][smoke]") {
    dicom_editor::DicomNode root;
    root.kind = dicom_editor::DicomNodeKind::Dataset;
    dicom_editor::DicomNode item;
    item.kind = dicom_editor::DicomNodeKind::Item;
    item.path = dicom_editor::DicomPath::item({{.sequenceTag = DCM_ContentSequence, .itemIndex = 0}});
    item.depth = 1;
    dicom_editor::DicomNode value;
    value.path = dicom_editor::DicomPath::element(item.path.parents(), DCM_TextValue);
    value.depth = 2;
    DatasetPanel panel(0, 0, 900, 600);
    panel.setNodes({root, item, value});
    panel.focusRows();
    panel.focusRows(2);
    REQUIRE(panel.selectedNode() != nullptr);
    REQUIRE(panel.selectedNode()->path.toString() == value.path.toString());
    Fl_Button *collapse = nullptr;
    for (int index = 0; index < panel.children(); ++index) {
        if (auto *button = dynamic_cast<Fl_Button *>(panel.child(index)); button != nullptr && button->label() != nullptr &&
            std::string(button->label()) == "Collapse / Expand") {
            collapse = button;
        }
    }
    REQUIRE(collapse != nullptr);
    if (collapse == nullptr) {
        return;
    }
    collapse->do_callback();
    REQUIRE(panel.selectedNode() != nullptr);
    REQUIRE(panel.selectedNode()->path.toString() == item.path.toString());
    collapse->do_callback();
    REQUIRE(panel.selectedNode() != nullptr);
    REQUIRE(panel.selectedNode()->path.toString() == item.path.toString());

    DatasetPanel noSelectionPanel(0, 0, 900, 600);
    noSelectionPanel.setNodes({root, item, value});
    Fl_Button *collapseWithoutSelection = nullptr;
    for (int index = 0; index < noSelectionPanel.children(); ++index) {
        if (auto *button = dynamic_cast<Fl_Button *>(noSelectionPanel.child(index)); button != nullptr && button->label() != nullptr &&
            std::string(button->label()) == "Collapse / Expand") {
            collapseWithoutSelection = button;
        }
    }
    REQUIRE(collapseWithoutSelection != nullptr);
    if (collapseWithoutSelection == nullptr) {
        return;
    }
    REQUIRE(noSelectionPanel.selectedNode() == nullptr);
    collapseWithoutSelection->do_callback();
    REQUIRE(noSelectionPanel.selectedNode() == nullptr);
    collapseWithoutSelection->do_callback();
    REQUIRE(noSelectionPanel.selectedNode() == nullptr);
    noSelectionPanel.focusRows();
    REQUIRE(noSelectionPanel.selectedNode() != nullptr);
    REQUIRE(noSelectionPanel.selectedNode()->kind == dicom_editor::DicomNodeKind::Dataset);
    noSelectionPanel.focusRows(1);
    REQUIRE(noSelectionPanel.selectedNode() != nullptr);
    REQUIRE(noSelectionPanel.selectedNode()->kind == dicom_editor::DicomNodeKind::Item);
    panel.resize(0, 0, 1100, 750);
}

TEST_CASE("SR creation form switches value fields and submits a new measurement", "[gui][smoke][sr]") {
    dicom_editor::ReportNodeInput submitted;
    bool called = false;
    auto dialog = createReportNodeDialog([&](const dicom_editor::ReportNodeInput &input) {
        submitted = input;
        called = true;
        return false;
    });
    Fl_Choice *type = nullptr;
    Fl_Input *value = nullptr;
    Fl_Input *units = nullptr;
    Fl_Button *create = nullptr;
    for (int index = 0; index < dialog->children(); ++index) {
        auto *widget = dialog->child(index);
        const std::string label = widget->label() == nullptr ? "" : widget->label();
        if (label == "Value type") {
            type = dynamic_cast<Fl_Choice *>(widget);
        }
        if (auto *input = dynamic_cast<Fl_Input *>(widget)) {
            if (label == "Name meaning")
                input->value("Dose");
            if (label == "Name code")
                input->value("DOSE");
            if (label == "Name coding scheme")
                input->value("99TEST");
            if (label == "Text")
                value = input;
            if (label == "Value code") {
                units = input;
                input->value("mGy");
            }
            if (label == "Value coding scheme")
                input->value("UCUM");
            if (label == "Value meaning")
                input->value("milligray");
        }
        if (label == "Create node")
            create = dynamic_cast<Fl_Button *>(widget);
    }
    REQUIRE(type != nullptr);
    REQUIRE(value != nullptr);
    REQUIRE(units != nullptr);
    REQUIRE(create != nullptr);
    if (!type || !value || !units || !create)
        return;
    REQUIRE_FALSE(units->visible());
    type->value(1);
    type->do_callback();
    REQUIRE(units->visible());
    REQUIRE(std::string(units->label()) == "Units code");
    value->value("2.5");
    create->do_callback();
    REQUIRE(called);
    REQUIRE(submitted.valueType == "NUM");
    REQUIRE(submitted.relationship == "CONTAINS");
    REQUIRE(submitted.nameMeaning == "Dose");
    REQUIRE(submitted.value == "2.5");
    REQUIRE(submitted.valueCode == "mGy");
    type->value(3);
    type->do_callback();
    REQUIRE_FALSE(value->visible());
    REQUIRE_FALSE(units->visible());
}
