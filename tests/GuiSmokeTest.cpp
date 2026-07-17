#include "FileTreePanel.hpp"

#include "dicom_editor/core/DicomWorkspace.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

int main() {
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
