#include "dicom_viewer/operations_c.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("C operations wrapper creates opaque documents and reports errors", "[operations][c-api]") {
    dicom_viewer_document *document = nullptr;
    REQUIRE(dicom_viewer_document_create(&document) == 0);
    REQUIRE(document != nullptr);
    REQUIRE(std::string{dicom_viewer_document_last_error(document)}.empty());

    REQUIRE(dicom_viewer_document_load(document, nullptr) != 0);
    REQUIRE(std::string{dicom_viewer_document_last_error(document)} == "Path must not be null");

    dicom_viewer_document_destroy(document);
}
