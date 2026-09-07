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

enum class ReportInsertion { Before, After, Child };

struct ReportNodeInput {
    std::string valueType{"TEXT"};
    std::string relationship{"CONTAINS"};
    std::string nameCode;
    std::string nameScheme;
    std::string nameMeaning;
    std::string value;
    // Coded value for CODE, measurement units for NUM.
    std::string valueCode;
    std::string valueScheme;
    std::string valueMeaning;
};

/// Projects SR content without rewriting unsupported content or private attributes.
class StructuredReport {
  public:
    [[nodiscard]] static bool supports(const DicomDocument &document);
    [[nodiscard]] static std::vector<ReportNode> nodes(DicomDocument &document);
    /// Validates all fields before committing changes to one existing content item.
    static void edit(DicomDocument &document, const DicomPath &node, const std::vector<std::string> &values);
    /// Copies a subtree to the end of its sibling list, or removes it. Root is protected.
    [[nodiscard]] static DicomPath changeStructure(DicomDocument &document, const DicomPath &node, bool remove);
    [[nodiscard]] static DicomPath insert(DicomDocument &document, const DicomPath &anchor, ReportInsertion placement,
                                          const ReportNodeInput &input);
};
} // namespace dicom_editor
