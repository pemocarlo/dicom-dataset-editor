#pragma once

// Public entry point for the reusable DICOM viewer-operations library.
//
// The library intentionally exposes the existing dicom_editor model namespace
// so the application can evolve without forcing every UI adapter to include a
// long list of individual headers. It has no dependency on FLTK or on the
// application controller.
#include "dicom_editor/core/AttributeInput.hpp"
#include "dicom_editor/core/DatasetViewModel.hpp"
#include "dicom_editor/core/DicomDictionary.hpp"
#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomEditorService.hpp"
#include "dicom_editor/core/DicomError.hpp"
#include "dicom_editor/core/DicomNode.hpp"
#include "dicom_editor/core/DicomPath.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"
#include "dicom_editor/core/StructuredReport.hpp"
