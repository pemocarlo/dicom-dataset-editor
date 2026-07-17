#pragma once

#include "dicom_editor/DicomDocument.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <string>

namespace dicom_editor {

struct EditRequest {
    DicomPath path;
    std::string value;
};

struct AddAttributeRequest {
    DicomPath parentItemPath;
    DcmTagKey tag;
    std::string value;
};

class DicomEditorService {
public:
    void editValue(DicomDocument& document, const EditRequest& request) const;
    void addAttribute(DicomDocument& document, const AddAttributeRequest& request) const;
    void deleteAttribute(DicomDocument& document, const DicomPath& path) const;
};

} // namespace dicom_editor
