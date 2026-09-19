#include "dicom_viewer/operations.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DICOM viewer operations are usable without an application or UI target") {
    dicom_editor::DicomWorkspace workspace;

    REQUIRE(workspace.size() == 1);
    REQUIRE_FALSE(workspace.hasLoadedFiles());
}
