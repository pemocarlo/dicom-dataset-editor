#include "dicom_editor/core/AttributeInput.hpp"

#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/ofstd/oftypes.h>

#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

namespace dicom_editor {

namespace {

std::optional<unsigned int> parseHex(std::string_view text) {
    unsigned int value{};
    const auto [ptr, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (error != std::errc() || ptr != text.data() + text.size() || value > 0xffffU) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::optional<DcmTagKey> parseTagKey(std::string_view group, std::string_view element) {
    const auto groupValue = parseHex(group);
    const auto elementValue = parseHex(element);
    if (!groupValue || !elementValue) {
        return std::nullopt;
    }
    return DcmTagKey(static_cast<Uint16>(*groupValue), static_cast<Uint16>(*elementValue));
}

} // namespace dicom_editor
