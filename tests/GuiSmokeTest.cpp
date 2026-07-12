#include "FileTreePanel.hpp"
#include "PixelDataPanel.hpp"

#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
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
