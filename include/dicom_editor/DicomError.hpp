#pragma once

#include <stdexcept>
#include <string>

namespace dicom_editor {

class DicomError : public std::runtime_error {
  public:
    explicit DicomError(const std::string &message) : std::runtime_error(message) {}
};

} // namespace dicom_editor
