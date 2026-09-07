#include "dicom_editor/core/StructuredReport.hpp"

#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomError.hpp"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/dcmsr/dsrcodvl.h>
#include <dcmtk/dcmsr/dsrtypes.h>
#include <dcmtk/ofstd/ofcond.h>
#include <dcmtk/ofstd/ofstring.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dicom_editor {
namespace {
std::string text(DcmItem &item, const DcmTagKey &tag) {
    OFString value;
    item.findAndGetOFStringArray(tag, value);
    return value;
}

void check(const OFCondition &result) {
    if (result.bad()) {
        throw DicomError(result.text());
    }
}

void field(ReportNode &node, DcmItem &item, const std::vector<SequenceItemRef> &parents, const DcmTagKey &tag, const std::string &label) {
    if (item.tagExists(tag)) {
        node.fields.push_back({.path = DicomPath::element(parents, tag), .label = label, .value = text(item, tag)});
    }
}

void codeFields(ReportNode &node, DcmItem &item, std::vector<SequenceItemRef> parents, const DcmTagKey &tag, const std::string &label) {
    DcmSequenceOfItems *sequence = nullptr;
    if (item.findAndGetSequence(tag, sequence).bad() || sequence->card() != 1) {
        return;
    }
    auto &code = *sequence->getItem(0);
    parents.push_back({.sequenceTag = tag, .itemIndex = 0});
    field(node, code, parents, DCM_CodeValue, label + " code");
    field(node, code, parents, DCM_LongCodeValue, label + " code");
    field(node, code, parents, DCM_URNCodeValue, label + " URI");
    field(node, code, parents, DCM_CodingSchemeDesignator, label + " scheme");
    field(node, code, parents, DCM_CodingSchemeVersion, label + " scheme version");
    field(node, code, parents, DCM_CodeMeaning, label + " meaning");
}

ReportNode project(DcmItem &item, const DicomPath &path, std::size_t depth) {
    ReportNode node{.path = path,
                    .depth = depth,
                    .label = {},
                    .valueType = text(item, DCM_ValueType),
                    .relationship = text(item, DCM_RelationshipType),
                    .fields = {}};
    DSRCodedEntryValue conceptName;
    if (conceptName.readSequence(item, DCM_ConceptNameCodeSequence, "3").good()) {
        node.label = conceptName.getCodeMeaning();
    }
    if (node.label.empty()) {
        node.label = item.tagExists(DCM_ReferencedContentItemIdentifier) ? "Content reference" : "Unnamed content";
    }
    const auto &parents = path.parents();
    codeFields(node, item, parents, DCM_ConceptNameCodeSequence, "Name");
    if (node.valueType == "TEXT") {
        field(node, item, parents, DCM_TextValue, "Text");
    } else if (node.valueType == "CODE") {
        codeFields(node, item, parents, DCM_ConceptCodeSequence, "Value");
    } else if (node.valueType == "NUM") {
        DcmSequenceOfItems *measured = nullptr;
        if (item.findAndGetSequence(DCM_MeasuredValueSequence, measured).good() && measured->card() == 1) {
            auto nested = parents;
            nested.push_back({.sequenceTag = DCM_MeasuredValueSequence, .itemIndex = 0});
            auto &measurement = *measured->getItem(0);
            // Alternate numeric representations must not become inconsistent with Numeric Value.
            if (!measurement.tagExists(DCM_FloatingPointValue) && !measurement.tagExists(DCM_RationalNumeratorValue) &&
                !measurement.tagExists(DCM_RationalDenominatorValue)) {
                field(node, measurement, nested, DCM_NumericValue, "Number");
                codeFields(node, measurement, nested, DCM_MeasurementUnitsCodeSequence, "Units");
            }
        }
    } else if (node.valueType == "DATE") {
        field(node, item, parents, DCM_Date, "Date (YYYYMMDD)");
    } else if (node.valueType == "TIME") {
        field(node, item, parents, DCM_Time, "Time (HHMMSS)");
    } else if (node.valueType == "DATETIME") {
        field(node, item, parents, DCM_DateTime, "Date/time");
    } else if (node.valueType == "PNAME") {
        field(node, item, parents, DCM_PersonName, "Person name");
    } else if (node.valueType == "UIDREF") {
        field(node, item, parents, DCM_UID, "UID");
    }
    return node;
}

void visit(DcmItem &item, const DicomPath &path, std::size_t depth, std::vector<ReportNode> &result) {
    if (depth > 128) {
        throw DicomError("SR content exceeds the supported nesting depth (128).");
    }
    result.push_back(project(item, path, depth));
    DcmSequenceOfItems *children = nullptr;
    if (item.findAndGetSequence(DCM_ContentSequence, children).bad()) {
        return;
    }
    for (unsigned long index = 0; index < children->card(); ++index) {
        auto parents = path.parents();
        parents.push_back({.sequenceTag = DCM_ContentSequence, .itemIndex = index});
        visit(*children->getItem(index), DicomPath::item(std::move(parents)), depth + 1, result);
    }
}

DcmItem &relativeItem(DcmItem &candidate, const ReportField &entry, std::size_t prefix) {
    auto *item = &candidate;
    for (std::size_t index = prefix; index < entry.path.parents().size(); ++index) {
        const auto &parent = entry.path.parents()[index];
        DcmItem *nested = nullptr;
        check(item->findAndGetSequenceItem(parent.sequenceTag, nested, static_cast<signed long>(parent.itemIndex)));
        item = nested;
    }
    return *item;
}
} // namespace

bool StructuredReport::supports(const DicomDocument &document) {
    return DSRTypes::sopClassUIDToDocumentType(document.attributeValue(DCM_SOPClassUID).value_or("")) != DSRTypes::DT_invalid;
}

std::vector<ReportNode> StructuredReport::nodes(DicomDocument &document) {
    if (!supports(document)) {
        return {};
    }
    std::vector<ReportNode> result;
    visit(document.dataset(), DicomPath::dataset(), 0, result);
    return result;
}

void StructuredReport::edit(DicomDocument &document, const DicomPath &path, const std::vector<std::string> &values) {
    const auto report = nodes(document);
    const auto found = std::ranges::find_if(
        report, [&path](const ReportNode &node) { return node.path.parents() == path.parents() && path.pointsToDatasetItem(); });
    if (found == report.end() || values.size() != found->fields.size()) {
        throw DicomError("The SR node or its fields have changed. Reopen the report editor.");
    }
    auto &target = document.itemAt(path);
    const std::unique_ptr<DcmItem> storage(static_cast<DcmItem *>(target.clone()));
    auto &candidate = *storage;
    bool changed = false;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto &entry = found->fields[index];
        if (values[index] == entry.value) {
            continue;
        }
        if (values[index].empty()) {
            throw DicomError(entry.label + " must not be empty.");
        }
        auto &item = relativeItem(candidate, entry, path.parents().size());
        DcmElement *element = nullptr;
        const auto &tag = entry.path.elementTag();
        if (!tag) {
            throw DicomError("SR field has no attribute tag.");
        }
        check(item.findAndGetElement(*tag, element));
        check(element->putString(values[index].c_str()));
        check(element->checkValue("1"));
        changed = true;
    }
    if (!changed) {
        return;
    }
    // Validate the complete edited coded entries with dcmsr, including code/scheme pairing.
    for (const auto &entry : found->fields) {
        if (entry.path.elementTag() == DCM_CodeMeaning) {
            DSRCodedEntryValue code;
            check(code.readSequenceItem(relativeItem(candidate, entry, path.parents().size()), entry.path.parents().back().sequenceTag));
            check(code.checkCurrentValue());
        }
    }
    if (document.attributeValue(DCM_VerificationFlag) == "VERIFIED" || document.dataset().tagExists(DCM_DigitalSignaturesSequence, true)) {
        throw DicomError("Verified or digitally signed reports are read-only in the SR editor.");
    }
    check(target.copyFrom(candidate));
    document.markDirty();
}
} // namespace dicom_editor
