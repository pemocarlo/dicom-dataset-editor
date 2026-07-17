#include "dicom_editor/DicomEditorService.hpp"

#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomError.hpp"

#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dctag.h>
#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/dcmdata/dcvr.h>
#include <dcmtk/ofstd/ofcond.h>

#include <format>
#include <optional>
#include <vector>

namespace dicom_editor {

namespace {

void requireGood(const OFCondition &condition, const std::string &action) {
    if (condition.bad()) {
        throw DicomError(std::format("{}: {}", action, condition.text()));
    }
}

void requireEditable(const DcmElement &element) {
    if (element.ident() == EVR_SQ) {
        throw DicomError("Sequences are edited through their items and attributes");
    }
}

void validateValue(const DcmTag &tag, const std::string &value) {
    DcmItem candidate;
    requireGood(candidate.putAndInsertString(tag, value.c_str()), std::format("Set candidate {} value", tag.getVRName()));

    DcmElement *element = nullptr;
    requireGood(candidate.findAndGetElement(tag, element), "Read candidate value");
    requireGood(element->checkValue("1-n"), std::format("Invalid DICOM {} value", tag.getVRName()));
}

} // namespace

void DicomEditorService::editValue(DicomDocument &document, const EditRequest &request) {
    DcmElement &element = document.elementAt(request.path);
    requireEditable(element);
    validateValue(element.getTag(), request.value);
    requireGood(element.putString(request.value.c_str()), std::format("Edit element {}", request.path.toString()));
    document.markDirty();
}

void DicomEditorService::addAttribute(DicomDocument &document, const AddAttributeRequest &request) {
    DcmItem &parent = document.itemAt(request.parentItemPath);
    validateValue(DcmTag(request.tag), request.value);
    requireGood(parent.putAndInsertString(request.tag, request.value.c_str(), true), "Add attribute");
    document.markDirty();
}

void DicomEditorService::deleteAttribute(DicomDocument &document, const DicomPath &path) {
    const auto &tag = path.elementTag();
    if (!tag) {
        throw DicomError("Only attributes can be deleted");
    }
    if (document.elementAt(path).ident() == EVR_SQ) {
        throw DicomError("Deleting sequence attributes is intentionally not exposed yet");
    }

    DicomPath parentPath = DicomPath::item(path.parents());
    DcmItem &parent = document.itemAt(parentPath);
    requireGood(parent.findAndDeleteElement(*tag, true, true), std::format("Delete attribute {}", path.toString()));
    document.markDirty();
}

} // namespace dicom_editor
