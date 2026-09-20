#pragma once

#include "dicom_viewer/export.hpp"

#include <stdexcept>

namespace dicom_editor {

/// DICOM error.
class DICOM_VIEWER_OPERATIONS_EXPORT DicomError : public std::runtime_error {
  public:
    /// Inherits runtime_error constructors.
    using std::runtime_error::runtime_error;
};

} // namespace dicom_editor
