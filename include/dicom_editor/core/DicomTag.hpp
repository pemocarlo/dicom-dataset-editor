#pragma once

#include "dicom_viewer/export.hpp"

#include <cstdint>

namespace dicom_editor {

/// A DICOM tag represented without exposing the storage library's types.
struct DICOM_VIEWER_OPERATIONS_EXPORT DicomTag {
    std::uint16_t group{};
    std::uint16_t element{};

    [[nodiscard]] constexpr bool operator==(const DicomTag &) const = default;
};

} // namespace dicom_editor
