#pragma once

#include <dcmtk/dcmdata/dctagkey.h>

#include <optional>
#include <string>

struct AttributeDialogResult {
    std::optional<DcmTagKey> tag;
    std::string value;
};

class AttributeEditDialog final {
  public:
    static std::optional<AttributeDialogResult> Edit(const std::string &title, const std::string &currentValue);
    static std::optional<AttributeDialogResult> Add();
};
