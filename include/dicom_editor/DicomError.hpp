#pragma once

#include <stdexcept>

namespace dicom_editor {

class DicomError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

} // namespace dicom_editor
