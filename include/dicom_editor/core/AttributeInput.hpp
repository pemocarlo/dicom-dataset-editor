#pragma once

#include "dicom_viewer/export.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <optional>
#include <string>
#include <string_view>

namespace dicom_editor {

/// Result returned by the attribute dialogs.
struct AttributeInput {
    /// Parsed tag, when the dialog collects one.
    std::optional<DcmTagKey> tag;
    /// Entered value text.
    std::string value;
};

/// Parses a hexadecimal DICOM tag from group and element fields.
[[nodiscard]] DICOM_VIEWER_OPERATIONS_EXPORT std::optional<DcmTagKey> parseTagKey(std::string_view group, std::string_view element);

} // namespace dicom_editor
