#include "FileTreePanel.hpp"
#include "PixelDataPanel.hpp"
#include "StructuredReportDialog.hpp"

#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomPath.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"
#include "dicom_editor/core/StructuredReport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>

#include <FL/Fl_Button.H>
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
