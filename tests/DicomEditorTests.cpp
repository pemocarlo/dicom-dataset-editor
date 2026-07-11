#include "dicom_editor/application/EditorController.hpp"
#include "dicom_editor/core/AttributeInput.hpp"
#include "dicom_editor/core/DatasetViewModel.hpp"
#include "dicom_editor/core/DicomDictionary.hpp"
#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomEditorService.hpp"
#include "dicom_editor/core/DicomError.hpp"
#include "dicom_editor/core/DicomNode.hpp"
#include "dicom_editor/core/DicomPath.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdicdir.h>
#include <dcmtk/dcmdata/dcdirrec.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dctag.h>
#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcxfer.h>
#include <dcmtk/ofstd/ofcond.h>
#include <dcmtk/ofstd/offile.h>
#include <dcmtk/ofstd/ofstring.h>
#include <ofstd/oftypes.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <print>
#include <ranges>
#include <source_location>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
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
    std::optional<dicom_editor::PixelDataPreview> pixelPreview;
    std::optional<dicom_editor::AttributeInput> batchInput;
    dicom_editor::SaveChangesChoice workspaceChoice{dicom_editor::SaveChangesChoice::Discard};
    std::size_t workspaceConfirmations{};
    bool hasLoadedFiles{};
    std::string error;
    std::string status;
    std::string documentStatus;
    std::size_t documentPresentations{};
    std::size_t openFilesPresentations{};
    std::size_t pixelPresentations{};
    std::vector<dicom_editor::SaveAllProgress> saveProgress;
    std::optional<std::size_t> cancelAfterCompleted;

    std::vector<std::filesystem::path> chooseOpenFiles() override { return chosenFiles; }
    std::optional<std::filesystem::path> chooseOpenFolder() override { return std::nullopt; }
    std::optional<std::filesystem::path> chooseDicomDirectory() override { return std::nullopt; }
    std::optional<std::filesystem::path> chooseDataDictionary() override { return std::nullopt; }
    std::optional<std::filesystem::path> chooseSaveFile() override { return std::nullopt; }
    dicom_editor::SaveChangesChoice confirmSaveChanges() override { return dicom_editor::SaveChangesChoice::Discard; }
    dicom_editor::SaveChangesChoice confirmWorkspaceChanges(std::size_t) override {
        ++workspaceConfirmations;
        return workspaceChoice;
    }
    bool confirmDelete() override { return false; }
    std::optional<dicom_editor::AttributeInput> editAttribute(const std::string &, const std::string &) override { return std::nullopt; }
    void viewAttribute(const std::string &, const std::string &) override {}
    std::optional<dicom_editor::AttributeInput> addAttribute() override { return std::nullopt; }
    std::optional<dicom_editor::AttributeInput> batchEditAttribute(const dicom_editor::BatchEditReport &) override { return batchInput; }
    dicom_editor::SaveAllReport runSaveAllJob(dicom_editor::SaveAllTask task) override {
        std::stop_source stop;
        return task(stop.get_token(), [this, &stop](const dicom_editor::SaveAllProgress &value) {
            saveProgress.push_back(value);
            if (cancelAfterCompleted && value.completed >= *cancelAfterCompleted) {
                stop.request_stop();
            }
        });
    }
    void showError(const std::string &message) override { error = message; }
    void presentDocument(std::vector<dicom_editor::DicomNode>, const std::string &, const std::string &value) override {
        ++documentPresentations;
        documentStatus = value;
    }
    void presentOpenFiles(const std::vector<dicom_editor::OpenDicomFile> &files, bool loaded) override {
        ++openFilesPresentations;
        openFiles = files;
        hasLoadedFiles = loaded;
    }
    void presentPixelData(std::optional<dicom_editor::PixelDataPreview> preview) override {
        ++pixelPresentations;
        pixelPreview = std::move(preview);
    }
    void setStatus(const std::string &value) override { status = value; }
};

void require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        throw std::runtime_error(std::format("test requirement failed at {}:{}", location.file_name(), location.line()));
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
    require(node->value == "[Double-click to view Pixel Data]");
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

void sharedUiModelCollapsesSequences() {
    DicomDocument document;
    seedDataset(document);
    dicom_editor::DatasetViewModel model;
    model.setNodes(document.nodes());
    const auto sequence = std::ranges::find_if(model.nodes(), [](const auto &node) {
        return node.kind == dicom_editor::DicomNodeKind::Sequence && node.keyword == "ReferencedStudySequence";
    });
    require(sequence != model.nodes().end());
    const auto expandedCount = model.visibleIndices().size();

    model.toggleSequence(sequence->path);
    require(model.sequenceCollapsed(sequence->path));
    require(model.visibleIndices().size() < expandedCount);
    model.setFilter("ReferencedSOPInstanceUID");
    require(!model.visibleIndices().empty());
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
    first.dataset().putAndInsertString(DCM_InstanceNumber, "10");
    require(first.saveAs(firstPath).has_value());

    DicomDocument second;
    seedDataset(second);
    second.dataset().putAndInsertString(DCM_PatientID, "PATIENT-1");
    second.dataset().putAndInsertString(DCM_StudyDescription, "Workspace study");
    second.dataset().putAndInsertString(DCM_SeriesDescription, "Series A");
    second.dataset().putAndInsertString(DCM_InstanceNumber, "2");
    require(second.saveAs(secondPath).has_value());

    ControllerView view;
    view.chosenFiles = {firstPath, secondPath};
    EditorController controller(view);
    controller.openDocument();
    require(view.error.empty());
    require(view.openFiles.size() == 2);
    require(view.openFiles[0].active);
    require(view.documentStatus.starts_with("File 1 of 2"));
    require(view.openFiles[0].hierarchy.patientId == "PATIENT-1");
    require(view.openFiles[0].hierarchy.studyLabel == "Workspace study");

    controller.setPixelDataVisible(true);
    require(view.pixelPreview && view.pixelPreview->sourceIndex == 0);
    controller.showNextDocument();
    require(view.openFiles[1].active);
    require(view.documentStatus.starts_with("File 2 of 2"));
    require(view.pixelPreview && view.pixelPreview->sourceIndex == 1);
    controller.showPreviousDocument();
    require(view.openFiles[0].active);
    require(view.pixelPreview && view.pixelPreview->sourceIndex == 0);

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

void workspaceSortsByInstanceOrFilename() {
    const auto directory = std::filesystem::temp_directory_path();
    const auto firstPath = directory / "z-last-name.dcm";
    const auto secondPath = directory / "a-first-name.dcm";
    DicomDocument first;
    seedDataset(first);
    first.dataset().putAndInsertString(DCM_InstanceNumber, "2");
    require(first.saveAs(firstPath).has_value());
    DicomDocument second;
    seedDataset(second);
    second.dataset().putAndInsertString(DCM_InstanceNumber, "10");
    require(second.saveAs(secondPath).has_value());

    DicomWorkspace workspace;
    require(workspace.open({secondPath, firstPath}).opened == 2);
    const auto byInstance = workspace.files();
    require(byInstance[0].path == firstPath);
    require(byInstance[0].active);
    const auto byFilename = workspace.files(dicom_editor::FileSortOrder::Filename);
    require(byFilename[0].path == secondPath);
    require(workspace.activateNext());
    require(workspace.active().filePath() == secondPath);
    require(workspace.activatePrevious());
    require(workspace.active().filePath() == firstPath);

    std::filesystem::remove(firstPath);
    std::filesystem::remove(secondPath);
}

void batchEditReportsDifferencesAndUpdatesScope() {
    const auto directory = std::filesystem::temp_directory_path();
    const auto firstPath = directory / "dicom_editor_batch_first.dcm";
    const auto secondPath = directory / "dicom_editor_batch_second.dcm";
    DicomDocument first;
    seedDataset(first);
    first.dataset().putAndInsertString(DCM_PatientID, "BATCH-PATIENT");
    require(first.saveAs(firstPath).has_value());
    DicomDocument second;
    seedDataset(second);
    second.dataset().putAndInsertString(DCM_PatientID, "BATCH-PATIENT");
    second.dataset().putAndInsertString(DCM_PatientName, "Different^Patient");
    require(second.saveAs(secondPath).has_value());

    DicomWorkspace workspace;
    require(workspace.open({firstPath, secondPath}).opened == 2);
    const dicom_editor::BatchEditTarget target{
        .level = dicom_editor::BatchEditLevel::Patient, .id = "BATCH-PATIENT", .label = "Before^Patient"};
    const auto report = workspace.batchEditReport(target);
    require(report.documentCount == 2);
    require(report.attributes.front().values.size() == 2);
    require(workspace.batchEdit(target, DCM_PatientName, "Unified^Patient") == 2);
    require(workspace.at(0).attributeValue(DCM_PatientName) == "Unified^Patient");
    require(workspace.at(1).attributeValue(DCM_PatientName) == "Unified^Patient");
    require(workspace.at(0).dirty() && workspace.at(1).dirty());

    std::filesystem::remove(firstPath);
    std::filesystem::remove(secondPath);
}

void controllerSavesAllAndClearsWorkspace() {
    const auto directory = std::filesystem::temp_directory_path();
    const auto firstPath = directory / "dicom_editor_save_all_first.dcm";
    const auto secondPath = directory / "dicom_editor_save_all_second.dcm";
    DicomDocument first;
    seedDataset(first);
    first.dataset().putAndInsertString(DCM_PatientID, "SAVE-ALL");
    require(first.saveAs(firstPath).has_value());
    DicomDocument second;
    seedDataset(second);
    second.dataset().putAndInsertString(DCM_PatientID, "SAVE-ALL");
    require(second.saveAs(secondPath).has_value());

    ControllerView view;
    view.chosenFiles = {firstPath, secondPath};
    view.batchInput = dicom_editor::AttributeInput{.tag = DCM_PatientName, .value = "Saved^Together"};
    EditorController controller(view);
    controller.openDocument();
    const auto activePath = std::ranges::find_if(view.openFiles, [](const auto &file) { return file.active; })->path;
    view.documentPresentations = 0;
    view.openFilesPresentations = 0;
    view.pixelPresentations = 0;
    controller.batchEdit({.level = dicom_editor::BatchEditLevel::Patient, .id = "SAVE-ALL", .label = "Before^Patient"});
    require(view.documentPresentations == 1);
    require(view.openFilesPresentations == 1);
    // cppcheck-suppress knownConditionTrueFalse
    require(view.pixelPresentations == 0);
    require(controller.actionState(nullptr).saveAllEnabled);
    view.documentPresentations = 0;
    view.openFilesPresentations = 0;
    view.pixelPresentations = 0;
    require(controller.saveAllDocuments());
    require(view.documentPresentations == 1);
    require(view.openFilesPresentations == 1);
    // cppcheck-suppress knownConditionTrueFalse
    require(view.pixelPresentations == 1);
    require(std::ranges::find_if(view.openFiles, [](const auto &file) { return file.active; })->path == activePath);
    require(!controller.actionState(nullptr).saveAllEnabled);

    DicomDocument reloadedFirst;
    DicomDocument reloadedSecond;
    require(reloadedFirst.load(firstPath).has_value());
    require(reloadedSecond.load(secondPath).has_value());
    require(reloadedFirst.attributeValue(DCM_PatientName) == "Saved^Together");
    require(reloadedSecond.attributeValue(DCM_PatientName) == "Saved^Together");

    controller.clearWorkspace();
    require(!view.hasLoadedFiles);
    require(view.openFiles.size() == 1);
    require(view.status == "Workspace cleared.");

    std::filesystem::remove(firstPath);
    std::filesystem::remove(secondPath);
}

void dicomDirectoryResolvesReferencedFiles() {
    const auto directory = std::filesystem::temp_directory_path() / "dicom_editor_dicomdir_test";
    const auto imagePath = directory / "IMG0001";
    const auto dicomdirPath = directory / "DICOMDIR";
    std::filesystem::create_directories(directory);
    DicomDocument image;
    seedDataset(image);
    require(image.saveAs(imagePath).has_value());
    {
        DcmDicomDir dicomdir(dicomdirPath.string().c_str(), "EDITOR_TEST");
        auto *patient = new DcmDirectoryRecord(ERT_Patient, nullptr, OFFilename());
        auto *study = new DcmDirectoryRecord(ERT_Study, nullptr, OFFilename());
        auto *series = new DcmDirectoryRecord(ERT_Series, nullptr, OFFilename());
        auto *imageRecord = new DcmDirectoryRecord(ERT_Image, "IMG0001", imagePath.string().c_str());
        require(patient->error().good() && study->error().good() && series->error().good() && imageRecord->error().good());
        require(series->insertSub(imageRecord).good());
        require(study->insertSub(series).good());
        require(patient->insertSub(study).good());
        require(dicomdir.getRootRecord().insertSub(patient).good());
        require(dicomdir.write().good());
    }

    const auto referenced = DicomWorkspace::discoverDicomDirectory(dicomdirPath);
    require(referenced.has_value());
    require(referenced->size() == 1);
    require(referenced->front() == imagePath);
    DicomWorkspace workspace;
    require(workspace.open(*referenced).opened == 1);

    std::filesystem::remove_all(directory);
}

void dictionaryOverrideIsValidatedAndActivated() {
    require(dicom_editor::dicomDictionarySource() == "embedded DCMTK dictionary");

    const auto directory = std::filesystem::temp_directory_path();
    const auto invalidPath = directory / "dicom_editor_invalid_dictionary.dic";
    {
        std::ofstream invalid(invalidPath);
        invalid << "not a DCMTK dictionary\n";
    }
    require(!dicom_editor::loadDicomDictionary(invalidPath).has_value());
    require(dicom_editor::dicomDictionarySource() == "embedded DCMTK dictionary");

    const auto validPath = directory / "dicom_editor_override_dictionary.dic";
    {
        std::ofstream valid(validPath);
        valid << "(7777,0010)\tLO\tEditorTestAttribute\t1\tTEST\n";
    }
    const auto loaded = dicom_editor::loadDicomDictionary(validPath);
    require(loaded.has_value());
    require(loaded->entryCount == 1);
    require(loaded->source == validPath.string());
    DcmTag customTag(DcmTagKey(0x7777, 0x0010));
    require(std::string_view(customTag.getTagName()) == "EditorTestAttribute");

    std::filesystem::remove(invalidPath);
    std::filesystem::remove(validPath);
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
        sharedUiModelCollapsesSequences();
        sharedTagParserValidatesHex();
        controllerOpensAndNavigatesMultipleFiles();
        dicomDirectoryIsRecognizedAndSkipped();
        workspaceSortsByInstanceOrFilename();
        batchEditReportsDifferencesAndUpdatesScope();
        controllerSavesAllAndClearsWorkspace();
        dicomDirectoryResolvesReferencedFiles();
        dictionaryOverrideIsValidatedAndActivated();

        std::println("All DICOM editor tests passed");
        return 0;
    } catch (const std::exception &error) {
        std::println(stderr, "Test failed: {}", error.what());
        return 1;
    }
}
