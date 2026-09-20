#include "dicom_editor/core/AttributeInput.hpp"

#include "dicom_editor/core/DicomTag.hpp"

#include <charconv>
#include <cstdint>
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

std::optional<DicomTag> parseTagKey(std::string_view group, std::string_view element) {
    const auto groupValue = parseHex(group);
    const auto elementValue = parseHex(element);
    if (!groupValue || !elementValue) {
        return std::nullopt;
    }
    return DicomTag{.group = static_cast<std::uint16_t>(*groupValue), .element = static_cast<std::uint16_t>(*elementValue)};
}

} // namespace dicom_editor
