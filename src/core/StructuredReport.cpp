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
#include <dcmtk/dcmsr/dsriodcc.h>
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
        node.fields.push_back({.path = DicomPath::element(parents, DCM_TextValue), .label = "Text", .value = text(item, DCM_TextValue)});
    } else if (node.valueType == "CODE") {
        codeFields(node, item, parents, DCM_ConceptCodeSequence, "Value");
    } else if (node.valueType == "NUM") {
        DcmSequenceOfItems *measured = nullptr;
        if (item.findAndGetSequence(DCM_MeasuredValueSequence, measured).good() && measured->card() == 1) {
            auto nested = parents;
            nested.push_back({.sequenceTag = DCM_MeasuredValueSequence, .itemIndex = 0});
            auto &measurement = *measured->getItem(0);
            node.fields.push_back(
                {.path = DicomPath::element(nested, DCM_NumericValue),
                 .label = "Number",
                 .value = text(measurement, measurement.tagExists(DCM_NumericValue) ? DCM_NumericValue : DCM_FloatingPointValue)});
            codeFields(node, measurement, nested, DCM_MeasurementUnitsCodeSequence, "Units");
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

DicomPath StructuredReport::insert(DicomDocument &document, const DicomPath &anchor, ReportInsertion placement,
                                   const ReportNodeInput &input) {
    const auto report = nodes(document);
    if (!anchor.pointsToDatasetItem() ||
        std::ranges::none_of(report, [&anchor](const ReportNode &node) { return node.path.parents() == anchor.parents(); })) {
        throw DicomError("Select an SR content item as the insertion point.");
    }
    if (document.attributeValue(DCM_VerificationFlag) == "VERIFIED" || document.dataset().tagExists(DCM_DigitalSignaturesSequence, true)) {
        throw DicomError("Verified or digitally signed reports are read-only in the SR editor.");
    }
    if (document.dataset().tagExists(DCM_ReferencedContentItemIdentifier, true)) {
        throw DicomError("Tree changes are unavailable for reports with content references.");
    }
    auto parents = anchor.parents();
    unsigned long index = 0;
    if (placement != ReportInsertion::Child) {
        if (parents.empty()) {
            throw DicomError("The report root cannot have siblings. Add a child instead.");
        }
        index = parents.back().itemIndex + (placement == ReportInsertion::After ? 1UL : 0UL);
        parents.pop_back();
    }
    if (parents.size() >= 128) {
        throw DicomError("SR content exceeds the supported nesting depth (128).");
    }
    auto &parent = document.itemAt(DicomPath::item(parents));
    const std::unique_ptr<DSRIODConstraintChecker> constraints(
        DSRTypes::createIODConstraintChecker(DSRTypes::sopClassUIDToDocumentType(document.attributeValue(DCM_SOPClassUID).value_or(""))));
    if (!constraints || !constraints->checkContentRelationship(DSRTypes::definedTermToValueType(text(parent, DCM_ValueType)),
                                                               DSRTypes::definedTermToRelationshipType(input.relationship),
                                                               DSRTypes::definedTermToValueType(input.valueType))) {
        throw DicomError("This relationship and value type are not allowed under the selected parent for this SR document type.");
    }
    auto node = std::make_unique<DcmItem>();
    auto put = [](DcmItem &item, const DcmTagKey &tag, const std::string &value) {
        if (value.empty()) {
            throw DicomError("Enter all required node value fields.");
        }
        check(item.putAndInsertString(tag, value.c_str()));
        DcmElement *element = nullptr;
        check(item.findAndGetElement(tag, element));
        check(element->checkValue("1"));
    };
    put(*node, DCM_ValueType, input.valueType);
    put(*node, DCM_RelationshipType, input.relationship);
    DSRCodedEntryValue name(input.nameCode, input.nameScheme, input.nameMeaning);
    check(name.checkCurrentValue());
    check(name.writeSequence(*node, DCM_ConceptNameCodeSequence));
    if (input.valueType == "CONTAINER") {
        put(*node, DCM_ContinuityOfContent, "SEPARATE");
    } else if (input.valueType == "CODE" || input.valueType == "NUM") {
        DSRCodedEntryValue code(input.valueCode, input.valueScheme, input.valueMeaning);
        check(code.checkCurrentValue());
        if (input.valueType == "CODE") {
            check(code.writeSequence(*node, DCM_ConceptCodeSequence));
        } else {
            DcmItem *measurement = nullptr;
            check(node->findOrCreateSequenceItem(DCM_MeasuredValueSequence, measurement, 0));
            put(*measurement, DCM_NumericValue, input.value);
            check(code.writeSequence(*measurement, DCM_MeasurementUnitsCodeSequence));
        }
    } else if (input.valueType == "TEXT") {
        put(*node, DCM_TextValue, input.value);
    } else if (input.valueType == "DATE") {
        put(*node, DCM_Date, input.value);
    } else if (input.valueType == "TIME") {
        put(*node, DCM_Time, input.value);
    } else if (input.valueType == "DATETIME") {
        put(*node, DCM_DateTime, input.value);
    } else if (input.valueType == "PNAME") {
        put(*node, DCM_PersonName, input.value);
    } else if (input.valueType == "UIDREF") {
        put(*node, DCM_UID, input.value);
    } else {
        throw DicomError("This value type is not supported by the node creation form.");
    }
    DcmSequenceOfItems *existing = nullptr;
    parent.findAndGetSequence(DCM_ContentSequence, existing);
    auto sequence = existing == nullptr ? std::make_unique<DcmSequenceOfItems>(DCM_ContentSequence)
                                        : std::unique_ptr<DcmSequenceOfItems>(static_cast<DcmSequenceOfItems *>(existing->clone()));
    if (placement == ReportInsertion::Child) {
        index = sequence->card();
    }
    check(index == sequence->card() ? sequence->append(node.get()) : sequence->insert(node.get(), index, OFTrue));
    node.release(); // NOLINT(bugprone-unused-return-value): ownership transferred to sequence
    check(parent.insert(sequence.get(), true));
    sequence.release(); // NOLINT(bugprone-unused-return-value): ownership transferred to parent
    document.markDirty();
    parents.push_back({.sequenceTag = DCM_ContentSequence, .itemIndex = index});
    return DicomPath::item(std::move(parents));
}

DicomPath StructuredReport::changeStructure(DicomDocument &document, const DicomPath &path, bool remove) {
    const auto report = nodes(document);
    if (path.parents().empty() || !path.pointsToDatasetItem() ||
        std::ranges::none_of(report, [&path](const ReportNode &node) { return node.path.parents() == path.parents(); })) {
        throw DicomError("Select a non-root SR content item.");
    }
    if (document.attributeValue(DCM_VerificationFlag) == "VERIFIED" || document.dataset().tagExists(DCM_DigitalSignaturesSequence, true)) {
        throw DicomError("Verified or digitally signed reports are read-only in the SR editor.");
    }
    if (document.dataset().tagExists(DCM_ReferencedContentItemIdentifier, true)) {
        throw DicomError("Tree changes are unavailable for reports with content references.");
    }
    auto parents = path.parents();
    const auto index = parents.back().itemIndex;
    parents.pop_back();
    auto &parent = document.itemAt(DicomPath::item(parents));
    DcmSequenceOfItems *sequence = nullptr;
    check(parent.findAndGetSequence(DCM_ContentSequence, sequence));
    DicomPath selection = DicomPath::item(parents);
    if (remove) {
        const std::unique_ptr<DcmItem> removed(sequence->remove(index));
        if (sequence->card() == 0) {
            check(parent.findAndDeleteElement(DCM_ContentSequence));
        }
    } else {
        auto copy = std::unique_ptr<DcmItem>(static_cast<DcmItem *>(sequence->getItem(index)->clone()));
        parents.push_back({.sequenceTag = DCM_ContentSequence, .itemIndex = sequence->card()});
        selection = DicomPath::item(std::move(parents));
        check(sequence->append(copy.get()));
        // append succeeded; the sequence now owns the copied item.
        copy.release(); // NOLINT(bugprone-unused-return-value)
    }
    document.markDirty();
    return selection;
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
        check(item.putAndInsertString(*tag, values[index].c_str()));
        check(item.findAndGetElement(*tag, element));
        check(element->checkValue("1"));
        if (*tag == DCM_NumericValue) {
            // Numeric Value becomes authoritative; optional alternate encodings must not retain the old number.
            for (const auto &alternate : {DCM_FloatingPointValue, DCM_RationalNumeratorValue, DCM_RationalDenominatorValue}) {
                if (item.tagExists(alternate)) {
                    check(item.findAndDeleteElement(alternate));
                }
            }
        }
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
