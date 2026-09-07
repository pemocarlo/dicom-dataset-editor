#pragma once

#include "dicom_editor/core/DicomPath.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace dicom_editor {
class DicomDocument;

struct ReportField {
    DicomPath path;
    std::string label;
    std::string value;
};

struct ReportNode {
    DicomPath path;
    std::size_t depth{};
    std::string label;
    std::string valueType;
    std::string relationship;
    std::vector<ReportField> fields;
};

/// Projects SR content without rewriting unsupported content or private attributes.
class StructuredReport {
  public:
    [[nodiscard]] static bool supports(const DicomDocument &document);
    [[nodiscard]] static std::vector<ReportNode> nodes(DicomDocument &document);
    /// Validates all fields before committing changes to one existing content item.
    static void edit(DicomDocument &document, const DicomPath &node, const std::vector<std::string> &values);
};
} // namespace dicom_editor
