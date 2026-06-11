#pragma once

#include <dcmtk/dcmdata/dctagkey.h>

#include <optional>
#include <string>
#include <string_view>

namespace dicom_editor {

struct AttributeInput {
    std::optional<DcmTagKey> tag;
    std::string value;
};

[[nodiscard]] std::optional<DcmTagKey> parseTagKey(std::string_view group, std::string_view element);

} // namespace dicom_editor
