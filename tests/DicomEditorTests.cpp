#include "dicom_editor/AttributeInput.hpp"
#include "dicom_editor/DatasetViewModel.hpp"
#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomEditorService.hpp"
#include "dicom_editor/DicomNode.hpp"
#include "dicom_editor/DicomPath.hpp"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/ofstd/ofcond.h>
#include <dcmtk/ofstd/ofstring.h>
#include <ofstd/oftypes.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using dicom_editor::AddAttributeRequest;
using dicom_editor::DicomDocument;
using dicom_editor::DicomEditorService;
using dicom_editor::DicomPath;
using dicom_editor::EditRequest;
using dicom_editor::SequenceItemRef;

namespace {

void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("test requirement failed");
    }
}

std::string stringValue(DicomDocument &document, const DicomPath &path) {
    OFString value;
    const auto condition = document.elementAt(path).getOFStringArray(value);
    require(condition.good());
    return value;
}

void seedDataset(DicomDocument &document) {
    auto &dataset = document.dataset();
    dataset.putAndInsertString(DCM_SpecificCharacterSet, "ISO_IR 100");
    dataset.putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
    dataset.putAndInsertString(DCM_SOPInstanceUID, "1.2.826.0.1.3680043.10.543.1");
    dataset.putAndInsertString(DCM_StudyInstanceUID, "1.2.826.0.1.3680043.10.543.2");
    dataset.putAndInsertString(DCM_SeriesInstanceUID, "1.2.826.0.1.3680043.10.543.3");
    dataset.putAndInsertString(DCM_Modality, "OT");
    dataset.putAndInsertString(DCM_PatientName, "Before^Patient");

    auto *sequence = new DcmSequenceOfItems(DCM_ReferencedStudySequence);
    auto *item = new DcmItem();
    item->putAndInsertString(DCM_ReferencedSOPClassUID, UID_SecondaryCaptureImageStorage);
    item->putAndInsertString(DCM_ReferencedSOPInstanceUID, "1.2.826.0.1.3680043.10.543.4");
    sequence->append(item);
    dataset.insert(sequence, true);
}

void scalarEdit() {
    DicomDocument document;
    seedDataset(document);

    const DicomPath patientName = DicomPath::element({}, DCM_PatientName);
    DicomEditorService::editValue(document, {.path = patientName, .value = "After^Patient"});

    require(document.dirty());
    require(stringValue(document, patientName) == "After^Patient");
}

void addDeleteElement() {
    DicomDocument document;
    seedDataset(document);

    const auto tag = DCM_PatientID;
    const DicomPath patientId = DicomPath::element({}, tag);
    DicomEditorService::addAttribute(document, AddAttributeRequest{.parentItemPath = DicomPath::dataset(), .tag = tag, .value = "PID-123"});
    require(stringValue(document, patientId) == "PID-123");

    DicomEditorService::deleteAttribute(document, patientId);
    DcmElement *deleted = nullptr;
    require(document.dataset().findAndGetElement(tag, deleted).bad());
}

void nestedSequenceEdit() {
    DicomDocument document;
    seedDataset(document);

    std::vector<SequenceItemRef> parents{{DCM_ReferencedStudySequence, 0}};
    const DicomPath referencedSop = DicomPath::element(parents, DCM_ReferencedSOPInstanceUID);
    DicomEditorService::editValue(document, {.path = referencedSop, .value = "1.2.826.0.1.3680043.10.543.99"});

    require(stringValue(document, referencedSop) == "1.2.826.0.1.3680043.10.543.99");
}

void invalidStandardValuesAreRejectedWithoutMutation() {
    DicomDocument document;
    seedDataset(document);

    const DicomPath sopInstanceUid = DicomPath::element({}, DCM_SOPInstanceUID);
    bool editRejected = false;
    try {
        DicomEditorService::editValue(document, {.path = sopInstanceUid, .value = "not a uid"});
    } catch (const std::exception &) {
        editRejected = true;
    }
    require(editRejected);
    require(stringValue(document, sopInstanceUid) == "1.2.826.0.1.3680043.10.543.1");
    require(!document.dirty());

    bool addRejected = false;
    try {
        DicomEditorService::addAttribute(document,
                                         {.parentItemPath = DicomPath::dataset(), .tag = DCM_PatientBirthDate, .value = "2026-99-99"});
    } catch (const std::exception &) {
        addRejected = true;
    }
    require(addRejected);
    DcmElement *birthDate = nullptr;
    require(document.dataset().findAndGetElement(DCM_PatientBirthDate, birthDate).bad());
    require(!document.dirty());
}

void saveReloadPersistence() {
    DicomDocument document;
    seedDataset(document);

    const DicomPath patientName = DicomPath::element({}, DCM_PatientName);
    DicomEditorService::editValue(document, EditRequest{.path = patientName, .value = "Persisted^Patient"});

    const auto output = std::filesystem::temp_directory_path() / "dicom_editor_persistence_test.dcm";
    document.saveAs(output);

    DicomDocument reloaded;
    reloaded.load(output);
    require(stringValue(reloaded, patientName) == "Persisted^Patient");

    std::filesystem::remove(output);
}

void recursiveNodeListing() {
    DicomDocument document;
    seedDataset(document);
    const auto nodes = document.nodes();

    const auto hasKeyword = [&nodes](std::string_view keyword) {
        return std::ranges::any_of(nodes, [keyword](const auto &node) { return node.keyword == keyword; });
    };
    require(hasKeyword("PatientName"));
    require(hasKeyword("ReferencedStudySequence"));
    require(hasKeyword("ReferencedSOPInstanceUID"));
}

void nodeKeepsFullValue() {
    DicomDocument document;
    const std::string longValue(200, 'x');
    document.dataset().putAndInsertString(DCM_PatientComments, longValue.c_str());

    const auto nodes = document.nodes();
    const auto node = std::ranges::find_if(nodes, [](const auto &entry) { return entry.keyword == "PatientComments"; });
    require(node != nodes.end());
    require(node->value == longValue);
    require(node->valuePreview.size() == 160);
}

void pixelDataIsNotDisplayedOrEditable() {
    DicomDocument document;
    const Uint8 pixelData[]{0x00, 0x7f, 0xff};
    document.dataset().putAndInsertUint8Array(DCM_PixelData, pixelData, 3);

    const auto nodes = document.nodes();
    const auto node = std::ranges::find_if(nodes, [](const auto &entry) { return entry.keyword == "PixelData"; });
    require(node != nodes.end());
    require(node->value == "[Pixel Data not displayed]");
    require(node->valuePreview.empty());
    require(!node->editable);
}

void sharedUiModelFiltersAndFormats() {
    dicom_editor::DatasetViewModel model;
    dicom_editor::DicomNode node;
    node.keyword = "PatientName";
    node.tag = "(0010,0010)";
    node.valuePreview = "Example^Patient";
    node.depth = 2;
    model.setNodes({node});

    require(model.visibleIndices().size() == 1);
    require(dicom_editor::DatasetViewModel::attributeLabel(node) == "    PatientName");
    model.setFilter("example^patient");
    require(model.visibleIndices().size() == 1);
    model.setFilter("missing");
    require(model.visibleIndices().empty());
}

void sharedTagParserValidatesHex() {
    const auto tag = dicom_editor::parseTagKey("0010", "0010");
    require(tag && *tag == DCM_PatientName);
    require(!dicom_editor::parseTagKey("nope", "0010"));
    require(!dicom_editor::parseTagKey("10000", "0010"));
}

} // namespace

int main() {
    try {
        scalarEdit();
        addDeleteElement();
        nestedSequenceEdit();
        invalidStandardValuesAreRejectedWithoutMutation();
        saveReloadPersistence();
        recursiveNodeListing();
        nodeKeepsFullValue();
        pixelDataIsNotDisplayedOrEditable();
        sharedUiModelFiltersAndFormats();
        sharedTagParserValidatesHex();

        std::println("All DICOM editor tests passed");
        return 0;
    } catch (const std::exception &error) {
        std::println(stderr, "Test failed: {}", error.what());
        return 1;
    }
}
