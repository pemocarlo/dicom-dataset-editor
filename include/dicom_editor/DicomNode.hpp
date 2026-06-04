#pragma once

#include "dicom_editor/DicomPath.hpp"

#include <string>

namespace dicom_editor {

enum class DicomNodeKind {
    Dataset,
    Element,
    Sequence,
    Item,
};

struct DicomNode {
    DicomNodeKind kind{DicomNodeKind::Element};
    DicomPath path;
    std::string tag;
    std::string keyword;
    std::string vr;
    std::string vm;
    std::string valuePreview;
    unsigned int depth{};
    bool editable{};
};

} // namespace dicom_editor
