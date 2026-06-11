#pragma once

#include "dicom_editor/AttributeInput.hpp"

#include <optional>
#include <string>

class AttributeEditDialog final {
  public:
    static std::optional<dicom_editor::AttributeInput> Edit(const std::string &title, const std::string &currentValue);
    static std::optional<dicom_editor::AttributeInput> Add();
};
