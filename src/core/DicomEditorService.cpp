#include "dicom_editor/DicomEditorService.hpp"

#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomError.hpp"

#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dctag.h>
#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/dcmdata/dcvr.h>
#include <dcmtk/ofstd/ofcond.h>

#include <optional>
#include <vector>

namespace dicom_editor {

namespace {

void requireGood(const OFCondition &condition, const std::string &action) {
    if (condition.bad()) {
        throw DicomError(action + ": " + condition.text());
    }
}

void requireEditable(const DcmElement &element) {
    if (element.ident() == EVR_SQ) {
        throw DicomError("Sequences are edited through their items and attributes");
    }
}

} // namespace

void DicomEditorService::editValue(DicomDocument &document, const EditRequest &request) const {
    DcmElement &element = document.elementAt(request.path);
    requireEditable(element);
    requireGood(element.putString(request.value.c_str()), "Edit element " + request.path.toString());
    document.markDirty();
}

void DicomEditorService::addAttribute(DicomDocument &document, const AddAttributeRequest &request) const {
    DcmItem &parent = document.itemAt(request.parentItemPath);
    requireGood(parent.putAndInsertString(request.tag, request.value.c_str(), true), "Add attribute");
    document.markDirty();
}

void DicomEditorService::deleteAttribute(DicomDocument &document, const DicomPath &path) const {
    const auto &tag = path.elementTag();
    if (!tag) {
        throw DicomError("Only attributes can be deleted");
    }
    if (document.elementAt(path).ident() == EVR_SQ) {
        throw DicomError("Deleting sequence attributes is intentionally not exposed yet");
    }

    DicomPath parentPath = DicomPath::item(path.parents());
    DcmItem &parent = document.itemAt(parentPath);
    requireGood(parent.findAndDeleteElement(*tag, true, true), "Delete attribute " + path.toString());
    document.markDirty();
}

} // namespace dicom_editor
