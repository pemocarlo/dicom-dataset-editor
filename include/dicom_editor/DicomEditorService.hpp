#pragma once

#include "dicom_editor/DicomPath.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <string>

namespace dicom_editor {

class DicomDocument;

struct EditRequest {
    DicomPath path;
    std::string value;
    bool validate{true};
};

struct AddAttributeRequest {
    DicomPath parentItemPath;
    DcmTagKey tag;
    std::string value;
    bool validate{true};
};

class DicomEditorService {
  public:
    static void editValue(DicomDocument &document, const EditRequest &request);
    static void addAttribute(DicomDocument &document, const AddAttributeRequest &request);
    static void deleteAttribute(DicomDocument &document, const DicomPath &path);
};

} // namespace dicom_editor
