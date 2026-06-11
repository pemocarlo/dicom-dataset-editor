#pragma once

#include "dicom_editor/AttributeInput.hpp"

#include <optional>
#include <string>

class AttributeDialog final {
  public:
    static std::optional<dicom_editor::AttributeInput> edit(const std::string &title, const std::string &currentValue);
    static std::optional<dicom_editor::AttributeInput> add();
};
