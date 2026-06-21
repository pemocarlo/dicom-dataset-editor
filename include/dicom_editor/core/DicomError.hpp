#pragma once

#include <stdexcept>

namespace dicom_editor {

/// DICOM error.
class DicomError : public std::runtime_error {
  public:
    /// Inherits runtime_error constructors.
    using std::runtime_error::runtime_error;
};

} // namespace dicom_editor
