#pragma once

#include "dicom_editor/DicomPath.hpp"

#include <string>

namespace dicom_editor {

/// Row kind shown in the dataset tree.
enum class DicomNodeKind {
    /// Root dataset row.
    Dataset,
    /// Scalar attribute row.
    Element,
    /// Sequence attribute row.
    Sequence,
    /// Sequence item row.
    Item,
};

/// One row in the flattened dataset tree.
struct DicomNode {
    /// Kind of row.
    DicomNodeKind kind{DicomNodeKind::Element};
    /// Stable path to the underlying object.
    DicomPath path;
    /// Tag string, for example `(0010,0010)`.
    std::string tag;
    /// DICOM keyword, when available.
    std::string keyword;
    /// Value representation.
    std::string vr;
    /// Value multiplicity.
    std::string vm;
    /// Full value text.
    std::string value;
    /// Truncated value preview shown in the table.
    std::string valuePreview;
    /// Depth used for indentation.
    unsigned int depth{};
    /// `true` when the row can be edited or deleted.
    bool editable{};
    /// `true` when validation marked the value invalid.
    bool invalidValue{};
};

} // namespace dicom_editor
