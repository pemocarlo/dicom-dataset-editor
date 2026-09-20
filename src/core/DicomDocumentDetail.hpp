#pragma once

#include "dicom_editor/core/DicomDocument.hpp"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcitem.h>

namespace dicom_editor::detail {

/// Source-only bridge for operations that need DCMTK's native object graph.
/// It is deliberately not installed with the public operations headers.
struct DocumentAccess {
    static DICOM_VIEWER_OPERATIONS_EXPORT const DcmFileFormat &file(const DicomDocument &document);
    static DICOM_VIEWER_OPERATIONS_EXPORT DcmDataset &dataset(DicomDocument &document);
    static DICOM_VIEWER_OPERATIONS_EXPORT const DcmDataset &dataset(const DicomDocument &document);
    static DICOM_VIEWER_OPERATIONS_EXPORT DcmItem &item(DicomDocument &document, const DicomPath &path);
    static DICOM_VIEWER_OPERATIONS_EXPORT const DcmItem &item(const DicomDocument &document, const DicomPath &path);
    static DICOM_VIEWER_OPERATIONS_EXPORT DcmElement &element(DicomDocument &document, const DicomPath &path);
    static DICOM_VIEWER_OPERATIONS_EXPORT const DcmElement &element(const DicomDocument &document, const DicomPath &path);
};

} // namespace dicom_editor::detail
