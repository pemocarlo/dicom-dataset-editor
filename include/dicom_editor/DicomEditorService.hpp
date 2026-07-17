#pragma once

#include "dicom_editor/DicomPath.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <string>

namespace dicom_editor {

class DicomDocument;

/// Parameters for editing an existing element value.
struct EditRequest {
    /// Path to the element being edited.
    DicomPath path;
    /// Replacement text to write.
    std::string value;
    /// Validate the value before writing it.
    bool validate{true};
};

/// Parameters for adding a new attribute.
struct AddAttributeRequest {
    /// Path to the owning item.
    DicomPath parentItemPath;
    /// Tag of the attribute to create.
    DcmTagKey tag;
    /// Initial value to store.
    std::string value;
    /// Validate the value before writing it.
    bool validate{true};
};

/// Validated add, edit, and delete helpers for the document model.
class DicomEditorService {
  public:
    /// Replaces an existing element value.
    static void editValue(DicomDocument &document, const EditRequest &request);
    /// Inserts or replaces a root-dataset attribute.
    static void setAttribute(DicomDocument &document, const DcmTagKey &tag, const std::string &value, bool validate = true);
    /// Inserts a new attribute into a dataset item.
    static void addAttribute(DicomDocument &document, const AddAttributeRequest &request);
    /// Removes a scalar attribute.
    static void deleteAttribute(DicomDocument &document, const DicomPath &path);
};

} // namespace dicom_editor
