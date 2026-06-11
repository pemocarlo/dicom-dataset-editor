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

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
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
    DicomEditorService editor;
    seedDataset(document);

    const DicomPath patientName = DicomPath::element({}, DCM_PatientName);
    editor.editValue(document, {.path = patientName, .value = "After^Patient"});

    require(document.dirty());
    require(stringValue(document, patientName) == "After^Patient");
}

void addDeleteElement() {
    DicomDocument document;
    DicomEditorService editor;
    seedDataset(document);

    const auto tag = DCM_PatientID;
    const DicomPath patientId = DicomPath::element({}, tag);
    editor.addAttribute(document, AddAttributeRequest{.parentItemPath = DicomPath::dataset(), .tag = tag, .value = "PID-123"});
    require(stringValue(document, patientId) == "PID-123");

    editor.deleteAttribute(document, patientId);
    DcmElement *deleted = nullptr;
    require(document.dataset().findAndGetElement(tag, deleted).bad());
}

void nestedSequenceEdit() {
    DicomDocument document;
    DicomEditorService editor;
    seedDataset(document);

    std::vector<SequenceItemRef> parents{{DCM_ReferencedStudySequence, 0}};
    const DicomPath referencedSop = DicomPath::element(parents, DCM_ReferencedSOPInstanceUID);
    editor.editValue(document, {.path = referencedSop, .value = "1.2.826.0.1.3680043.10.543.99"});

    require(stringValue(document, referencedSop) == "1.2.826.0.1.3680043.10.543.99");
}

void saveReloadPersistence() {
    DicomDocument document;
    DicomEditorService editor;
    seedDataset(document);

    const DicomPath patientName = DicomPath::element({}, DCM_PatientName);
    editor.editValue(document, EditRequest{.path = patientName, .value = "Persisted^Patient"});

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

    bool sawPatientName = false;
    bool sawSequence = false;
    bool sawNestedValue = false;
    for (const auto &node : nodes) {
        sawPatientName = sawPatientName || node.keyword == "PatientName";
        sawSequence = sawSequence || node.keyword == "ReferencedStudySequence";
        sawNestedValue = sawNestedValue || node.keyword == "ReferencedSOPInstanceUID";
    }

    require(sawPatientName);
    require(sawSequence);
    require(sawNestedValue);
}

void nodeKeepsFullValue() {
    DicomDocument document;
    const std::string longValue(200, 'x');
    document.dataset().putAndInsertString(DCM_PatientComments, longValue.c_str());

    for (const auto &node : document.nodes()) {
        if (node.keyword == "PatientComments") {
            require(node.value == longValue);
            require(node.valuePreview.size() == 160);
            return;
        }
    }
    require(false);
}

void pixelDataIsNotDisplayedOrEditable() {
    DicomDocument document;
    const Uint8 pixelData[]{0x00, 0x7f, 0xff};
    document.dataset().putAndInsertUint8Array(DCM_PixelData, pixelData, 3);

    for (const auto &node : document.nodes()) {
        if (node.keyword == "PixelData") {
            require(node.value == "<not displayed>");
            require(node.valuePreview.empty());
            require(!node.editable);
            return;
        }
    }
    require(false);
}

} // namespace

int main() {
    scalarEdit();
    addDeleteElement();
    nestedSequenceEdit();
    saveReloadPersistence();
    recursiveNodeListing();
    nodeKeepsFullValue();
    pixelDataIsNotDisplayedOrEditable();

    std::cout << "All DICOM editor tests passed\n";
    return 0;
}
