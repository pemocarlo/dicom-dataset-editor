#pragma once

#include "dicom_editor/core/DicomTag.hpp"
#include "dicom_viewer/export.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace dicom_editor {

/// Result returned by the attribute dialogs.
struct AttributeInput {
    /// Parsed tag, when the dialog collects one.
    std::optional<DicomTag> tag;
    /// Entered value text.
    std::string value;
};

/// Parses a hexadecimal DICOM tag from group and element fields.
[[nodiscard]] DICOM_VIEWER_OPERATIONS_EXPORT std::optional<DicomTag> parseTagKey(std::string_view group, std::string_view element);

} // namespace dicom_editor
