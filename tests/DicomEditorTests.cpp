#include "dicom_editor/AttributeInput.hpp"
#include "dicom_editor/DatasetViewModel.hpp"
#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomEditorService.hpp"
#include "dicom_editor/DicomError.hpp"
#include "dicom_editor/DicomNode.hpp"
#include "dicom_editor/DicomPath.hpp"
#include "dicom_editor/DicomWorkspace.hpp"
#include "dicom_editor/EditorController.hpp"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcxfer.h>
#include <dcmtk/ofstd/ofcond.h>
#include <dcmtk/ofstd/ofstring.h>
#include <ofstd/oftypes.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <expected>
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
using dicom_editor::DicomWorkspace;
using dicom_editor::EditorController;
using dicom_editor::EditRequest;
using dicom_editor::SequenceItemRef;

namespace {

class ControllerView final : public dicom_editor::EditorView {
  public:
    std::vector<std::filesystem::path> chosenFiles;
    std::vector<dicom_editor::OpenDicomFile> openFiles;
    std::string error;

    std::vector<std::filesystem::path> chooseOpenFiles() override { return chosenFiles; }
    std::optional<std::filesystem::path> chooseOpenFolder() override { return std::nullopt; }
    std::optional<std::filesystem::path> chooseSaveFile() override { return std::nullopt; }
    dicom_editor::SaveChangesChoice confirmSaveChanges() override { return dicom_editor::SaveChangesChoice::Discard; }
    bool confirmDelete() override { return false; }
    std::optional<dicom_editor::AttributeInput> editAttribute(const std::string &, const std::string &) override { return std::nullopt; }
    std::optional<dicom_editor::AttributeInput> addAttribute() override { return std::nullopt; }
    void showError(const std::string &message) override { error = message; }
    void presentDocument(std::vector<dicom_editor::DicomNode>, const std::string &, const std::string &) override {}
    void presentOpenFiles(const std::vector<dicom_editor::OpenDicomFile> &files, bool) override { openFiles = files; }
    void presentPixelData(std::optional<dicom_editor::PixelDataPreview>) override {}
    void setStatus(const std::string &) override {}
};

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

void validationCanBeDisabled() {
    DicomDocument document;
    seedDataset(document);

    const DicomPath sopInstanceUid = DicomPath::element({}, DCM_SOPInstanceUID);
    DicomEditorService::editValue(document, {.path = sopInstanceUid, .value = "not-a-uid", .validate = false});
    require(stringValue(document, sopInstanceUid) == "not-a-uid");

    const auto uncheckedNodes = document.nodes(false);
    require(std::ranges::none_of(uncheckedNodes, [](const auto &node) { return node.invalidValue; }));

    const auto checkedNodes = document.nodes(true);
    const auto invalid = std::ranges::find_if(checkedNodes, [](const auto &node) { return node.keyword == "SOPInstanceUID"; });
    require(invalid != checkedNodes.end());
    require(invalid->invalidValue);
}

void saveReloadPersistence() {
    DicomDocument document;
    seedDataset(document);

    const DicomPath patientName = DicomPath::element({}, DCM_PatientName);
    DicomEditorService::editValue(document, EditRequest{.path = patientName, .value = "Persisted^Patient"});

    const auto output = std::filesystem::temp_directory_path() / "dicom_editor_persistence_test.dcm";
    require(document.saveAs(output).has_value());

    DicomDocument reloaded;
    require(reloaded.load(output).has_value());
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

void pixelDataRendersForPreview() {
    DicomDocument document;
    auto &dataset = document.dataset();
    dataset.putAndInsertUint16(DCM_Rows, 2);
    dataset.putAndInsertUint16(DCM_Columns, 2);
    dataset.putAndInsertUint16(DCM_SamplesPerPixel, 1);
    dataset.putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
    dataset.putAndInsertUint16(DCM_BitsAllocated, 8);
    dataset.putAndInsertUint16(DCM_BitsStored, 8);
    dataset.putAndInsertUint16(DCM_HighBit, 7);
    dataset.putAndInsertUint16(DCM_PixelRepresentation, 0);
    const Uint8 pixels[]{0, 64, 128, 255};
    dataset.putAndInsertUint8Array(DCM_PixelData, pixels, 4);
    dataset.initializeXfer(EXS_JPEGProcess1);

    const auto preview = document.renderPixelData(0);
    require(preview.message.empty());
    require(preview.width == 2);
    require(preview.height == 2);
    require(preview.channels == 1);
    require(preview.frameIndex == 0);
    require(preview.frameCount == 1);
    require(preview.pixels.size() == 4);
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

void controllerOpensAndNavigatesMultipleFiles() {
    const auto directory = std::filesystem::temp_directory_path();
    const auto firstPath = directory / "dicom_editor_workspace_first.dcm";
    const auto secondPath = directory / "dicom_editor_workspace_second.dcm";

    DicomDocument first;
    seedDataset(first);
    first.dataset().putAndInsertString(DCM_PatientID, "PATIENT-1");
    first.dataset().putAndInsertString(DCM_StudyDescription, "Workspace study");
    first.dataset().putAndInsertString(DCM_SeriesDescription, "Series A");
    require(first.saveAs(firstPath).has_value());

    DicomDocument second;
    seedDataset(second);
    second.dataset().putAndInsertString(DCM_PatientID, "PATIENT-1");
    second.dataset().putAndInsertString(DCM_StudyDescription, "Workspace study");
    second.dataset().putAndInsertString(DCM_SeriesDescription, "Series B");
    require(second.saveAs(secondPath).has_value());

    ControllerView view;
    view.chosenFiles = {firstPath, secondPath};
    EditorController controller(view);
    controller.openDocument();
    require(view.error.empty());
    require(view.openFiles.size() == 2);
    require(view.openFiles[1].active);
    require(view.openFiles[0].hierarchy.patientId == "PATIENT-1");
    require(view.openFiles[0].hierarchy.studyLabel == "Workspace study");

    controller.showPreviousDocument();
    require(view.openFiles[0].active);
    controller.showNextDocument();
    require(view.openFiles[1].active);

    std::filesystem::remove(firstPath);
    std::filesystem::remove(secondPath);
}

void dicomDirectoryIsRecognizedAndSkipped() {
    const auto path = std::filesystem::temp_directory_path() / "DICOMDIR";
    DicomDocument directory;
    seedDataset(directory);
    directory.dataset().putAndInsertString(DCM_SOPClassUID, UID_MediaStorageDirectoryStorage);
    require(directory.isDicomDirectory());
    require(directory.saveAs(path).has_value());

    DicomDocument reloaded;
    require(reloaded.load(path).has_value());
    require(reloaded.isDicomDirectory());

    DicomWorkspace workspace;
    const auto result = workspace.open({path});
    require(result.opened == 0);
    require(result.dicomDirectories == 1);
    require(result.failures.empty());
    require(workspace.size() == 1);
    require(!workspace.active().hasFilePath());

    std::filesystem::remove(path);
}

} // namespace

int main() {
    try {
        scalarEdit();
        addDeleteElement();
        nestedSequenceEdit();
        invalidStandardValuesAreRejectedWithoutMutation();
        validationCanBeDisabled();
        saveReloadPersistence();
        recursiveNodeListing();
        nodeKeepsFullValue();
        pixelDataIsNotDisplayedOrEditable();
        pixelDataRendersForPreview();
        sharedUiModelFiltersAndFormats();
        sharedTagParserValidatesHex();
        controllerOpensAndNavigatesMultipleFiles();
        dicomDirectoryIsRecognizedAndSkipped();

        std::println("All DICOM editor tests passed");
        return 0;
    } catch (const std::exception &error) {
        std::println(stderr, "Test failed: {}", error.what());
        return 1;
    }
}
