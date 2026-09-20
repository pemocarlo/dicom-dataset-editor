#include "dicom_viewer/operations_c.h"

#include "dicom_viewer/operations.hpp"

#include <exception>
#include <filesystem>
#include <string>

struct dicom_viewer_document {
    dicom_editor::DicomDocument document;
    std::string error;
};

namespace {

int fail(dicom_viewer_document *handle, const std::exception &error) {
    if (handle != nullptr) {
        handle->error = error.what();
    }
    return -1;
}

int invalid(dicom_viewer_document *handle, const char *message) {
    if (handle != nullptr) {
        handle->error = message;
    }
    return -1;
}

template <typename Operation> int run(dicom_viewer_document *handle, Operation &&operation) {
    if (handle == nullptr) {
        return -1;
    }
    try {
        const auto result = operation();
        if (!result) {
            handle->error = result.error().what();
            return -1;
        }
        handle->error.clear();
        return 0;
    } catch (const std::exception &error) {
        return fail(handle, error);
    } catch (...) {
        return invalid(handle, "Unknown exception");
    }
}

} // namespace

extern "C" int dicom_viewer_document_create(dicom_viewer_document **document) {
    if (document == nullptr) {
        return -1;
    }
    try {
        *document = new dicom_viewer_document;
        return 0;
    } catch (...) {
        *document = nullptr;
        return -1;
    }
}

extern "C" void dicom_viewer_document_destroy(dicom_viewer_document *document) { delete document; }

extern "C" int dicom_viewer_document_load(dicom_viewer_document *document, const char *path) {
    if (path == nullptr) {
        return invalid(document, "Path must not be null");
    }
    return run(document, [document, path] { return document->document.load(std::filesystem::path(path)); });
}

extern "C" int dicom_viewer_document_save(dicom_viewer_document *document) {
    return run(document, [document] { return document->document.save(); });
}

extern "C" int dicom_viewer_document_save_as(dicom_viewer_document *document, const char *path) {
    if (path == nullptr) {
        return invalid(document, "Path must not be null");
    }
    return run(document, [document, path] { return document->document.saveAs(std::filesystem::path(path)); });
}

extern "C" const char *dicom_viewer_document_last_error(const dicom_viewer_document *document) {
    return document == nullptr ? "Invalid document handle" : document->error.c_str();
}
