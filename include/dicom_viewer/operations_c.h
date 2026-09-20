#pragma once

#include "dicom_viewer/operations_c_export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dicom_viewer_document dicom_viewer_document;

/// Creates an empty document handle. Returns zero on success.
DICOM_VIEWER_OPERATIONS_C_EXPORT int dicom_viewer_document_create(dicom_viewer_document **document);
/// Destroys a document handle; NULL is accepted.
DICOM_VIEWER_OPERATIONS_C_EXPORT void dicom_viewer_document_destroy(dicom_viewer_document *document);
/// Loads a DICOM file into the handle. Returns zero on success.
DICOM_VIEWER_OPERATIONS_C_EXPORT int dicom_viewer_document_load(dicom_viewer_document *document, const char *path);
/// Saves a file-backed document. Returns zero on success.
DICOM_VIEWER_OPERATIONS_C_EXPORT int dicom_viewer_document_save(dicom_viewer_document *document);
/// Saves a document to a new path and adopts that path. Returns zero on success.
DICOM_VIEWER_OPERATIONS_C_EXPORT int dicom_viewer_document_save_as(dicom_viewer_document *document, const char *path);
/// Returns the last error for this handle, or an empty string when there is none.
DICOM_VIEWER_OPERATIONS_C_EXPORT const char *dicom_viewer_document_last_error(const dicom_viewer_document *document);

#ifdef __cplusplus
}
#endif
