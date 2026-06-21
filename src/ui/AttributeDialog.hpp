#pragma once

#include "dicom_editor/AttributeInput.hpp"

#include <optional>
#include <string>

namespace dicom_editor {
struct BatchEditReport;
}

class AttributeDialog final {
  public:
    /// Edits an attribute value.
    static std::optional<dicom_editor::AttributeInput> edit(const std::string &title, const std::string &currentValue);
    /// Adds an attribute.
    static std::optional<dicom_editor::AttributeInput> add();
    /// Selects one allowed attribute and replacement value for a batch edit.
    static std::optional<dicom_editor::AttributeInput> batch(const dicom_editor::BatchEditReport &report);
};
