#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomEditorService.hpp"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dctk.h>

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

using dicom_editor::AddAttributeRequest;
using dicom_editor::DicomDocument;
using dicom_editor::DicomEditorService;
using dicom_editor::DicomPath;
using dicom_editor::EditRequest;
using dicom_editor::SequenceItemRef;

namespace {

std::string stringValue(DicomDocument& document, const DicomPath& path)
{
    OFString value;
    const auto condition = document.elementAt(path).getOFStringArray(value);
    assert(condition.good());
    return value.c_str();
}

void seedDataset(DicomDocument& document)
{
    auto& dataset = document.dataset();
    dataset.putAndInsertString(DCM_SpecificCharacterSet, "ISO_IR 100");
    dataset.putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
    dataset.putAndInsertString(DCM_SOPInstanceUID, "1.2.826.0.1.3680043.10.543.1");
    dataset.putAndInsertString(DCM_StudyInstanceUID, "1.2.826.0.1.3680043.10.543.2");
    dataset.putAndInsertString(DCM_SeriesInstanceUID, "1.2.826.0.1.3680043.10.543.3");
    dataset.putAndInsertString(DCM_Modality, "OT");
    dataset.putAndInsertString(DCM_PatientName, "Before^Patient");

    auto* sequence = new DcmSequenceOfItems(DCM_ReferencedStudySequence);
    auto* item = new DcmItem();
    item->putAndInsertString(DCM_ReferencedSOPClassUID, UID_SecondaryCaptureImageStorage);
    item->putAndInsertString(DCM_ReferencedSOPInstanceUID, "1.2.826.0.1.3680043.10.543.4");
    sequence->append(item);
    dataset.insert(sequence, true);
}

void scalarEdit()
{
    DicomDocument document;
    DicomEditorService editor;
    seedDataset(document);

    const DicomPath patientName = DicomPath::element({}, DCM_PatientName);
    editor.editValue(document, {.path = patientName, .value = "After^Patient"});

    assert(document.dirty());
    assert(stringValue(document, patientName) == "After^Patient");
}

void addDeleteElement()
{
    DicomDocument document;
    DicomEditorService editor;
    seedDataset(document);

    const auto tag = DCM_PatientID;
    const DicomPath patientId = DicomPath::element({}, tag);
    editor.addAttribute(document, AddAttributeRequest{.parentItemPath = DicomPath::dataset(), .tag = tag, .value = "PID-123"});
    assert(stringValue(document, patientId) == "PID-123");

    editor.deleteAttribute(document, patientId);
    DcmElement* deleted = nullptr;
    assert(document.dataset().findAndGetElement(tag, deleted).bad());
}

void nestedSequenceEdit()
{
    DicomDocument document;
    DicomEditorService editor;
    seedDataset(document);

    std::vector<SequenceItemRef> parents{{DCM_ReferencedStudySequence, 0}};
    const DicomPath referencedSop = DicomPath::element(parents, DCM_ReferencedSOPInstanceUID);
    editor.editValue(document, {.path = referencedSop, .value = "1.2.826.0.1.3680043.10.543.99"});

    assert(stringValue(document, referencedSop) == "1.2.826.0.1.3680043.10.543.99");
}

void saveReloadPersistence()
{
    DicomDocument document;
    DicomEditorService editor;
    seedDataset(document);

    const DicomPath patientName = DicomPath::element({}, DCM_PatientName);
    editor.editValue(document, EditRequest{.path = patientName, .value = "Persisted^Patient"});

    const auto output = std::filesystem::temp_directory_path() / "dicom_editor_persistence_test.dcm";
    document.saveAs(output);

    DicomDocument reloaded;
    reloaded.load(output);
    assert(stringValue(reloaded, patientName) == "Persisted^Patient");

    std::filesystem::remove(output);
}

void recursiveNodeListing()
{
    DicomDocument document;
    seedDataset(document);
    const auto nodes = document.nodes();

    bool sawPatientName = false;
    bool sawSequence = false;
    bool sawNestedValue = false;
    for (const auto& node : nodes) {
        sawPatientName = sawPatientName || node.keyword == "PatientName";
        sawSequence = sawSequence || node.keyword == "ReferencedStudySequence";
        sawNestedValue = sawNestedValue || node.keyword == "ReferencedSOPInstanceUID";
    }

    assert(sawPatientName);
    assert(sawSequence);
    assert(sawNestedValue);
}

} // namespace

int main()
{
    scalarEdit();
    addDeleteElement();
    nestedSequenceEdit();
    saveReloadPersistence();
    recursiveNodeListing();

    std::cout << "All DICOM editor tests passed\n";
    return 0;
}
