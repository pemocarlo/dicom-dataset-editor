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

#include <catch2/catch_test_macros.hpp>

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
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <ranges>
#include <span>
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
    void presentDocument(dicom_editor::DocumentPresentation presentation) override {
        ++documentPresentations;
        documentStatus = std::move(presentation.status);
    }
    void presentOpenFiles(dicom_editor::OpenFilesPresentation presentation) override {
        ++openFilesPresentations;
        openFiles = std::move(presentation.files);
        hasLoadedFiles = presentation.hasLoadedFiles;
    }
    void presentPixelData(std::optional<dicom_editor::PixelDataPreview> preview) override {
        ++pixelPresentations;
        pixelPreview = std::move(preview);
    }
    void setStatus(const std::string &value) override { status = value; }
};

std::string stringValue(DicomDocument &document, const DicomPath &path) {
    OFString value;
    const auto condition = document.elementAt(path).getOFStringArray(value);
    REQUIRE(condition.good());
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

std::optional<std::size_t> previewSourceIndex(const ControllerView &view) {
    return view.pixelPreview.transform([](const auto &preview) { return preview.sourceIndex; });
}

TEST_CASE("scalar attributes can be edited", "[core][editing]") {
    DicomDocument document;
    seedDataset(document);

    const DicomPath patientName = DicomPath::element({}, DCM_PatientName);
    DicomEditorService::editValue(document, {.path = patientName, .value = "After^Patient"});

    REQUIRE(document.dirty());
    REQUIRE(stringValue(document, patientName) == "After^Patient");
}

TEST_CASE("attributes can be added and deleted", "[core][editing]") {
    DicomDocument document;
    seedDataset(document);

    const auto tag = DCM_PatientID;
    const DicomPath patientId = DicomPath::element({}, tag);
    DicomEditorService::addAttribute(document, AddAttributeRequest{.parentItemPath = DicomPath::dataset(), .tag = tag, .value = "PID-123"});
    REQUIRE(stringValue(document, patientId) == "PID-123");

    DicomEditorService::deleteAttribute(document, patientId);
    DcmElement *deleted = nullptr;
    REQUIRE(document.dataset().findAndGetElement(tag, deleted).bad());
}

TEST_CASE("nested sequence attributes can be edited", "[core][editing]") {
    DicomDocument document;
    seedDataset(document);

    std::vector<SequenceItemRef> parents{{DCM_ReferencedStudySequence, 0}};
    const DicomPath referencedSop = DicomPath::element(parents, DCM_ReferencedSOPInstanceUID);
    DicomEditorService::editValue(document, {.path = referencedSop, .value = "1.2.826.0.1.3680043.10.543.99"});

    REQUIRE(stringValue(document, referencedSop) == "1.2.826.0.1.3680043.10.543.99");
}

TEST_CASE("invalid standard values do not mutate the document", "[core][validation]") {
    DicomDocument document;
    seedDataset(document);

    const DicomPath sopInstanceUid = DicomPath::element({}, DCM_SOPInstanceUID);
    REQUIRE_THROWS(DicomEditorService::editValue(document, {.path = sopInstanceUid, .value = "not a uid"}));
    REQUIRE(stringValue(document, sopInstanceUid) == "1.2.826.0.1.3680043.10.543.1");
    REQUIRE(!document.dirty());

    REQUIRE_THROWS(DicomEditorService::addAttribute(
        document, {.parentItemPath = DicomPath::dataset(), .tag = DCM_PatientBirthDate, .value = "2026-99-99"}));
    DcmElement *birthDate = nullptr;
    REQUIRE(document.dataset().findAndGetElement(DCM_PatientBirthDate, birthDate).bad());
    REQUIRE(!document.dirty());
}

TEST_CASE("validation can be disabled", "[core][validation]") {
    DicomDocument document;
    seedDataset(document);

    const DicomPath sopInstanceUid = DicomPath::element({}, DCM_SOPInstanceUID);
    DicomEditorService::editValue(document, {.path = sopInstanceUid, .value = "not-a-uid", .validate = false});
    REQUIRE(stringValue(document, sopInstanceUid) == "not-a-uid");

    const auto uncheckedNodes = document.nodes(false);
    REQUIRE(std::ranges::none_of(uncheckedNodes, [](const auto &node) { return node.invalidValue; }));

    const auto checkedNodes = document.nodes(true);
    const auto invalid = std::ranges::find_if(checkedNodes, [](const auto &node) { return node.keyword == "SOPInstanceUID"; });
    REQUIRE(invalid != checkedNodes.end());
    REQUIRE(invalid->invalidValue);
}

TEST_CASE("edits persist after save and reload", "[core][persistence]") {
    DicomDocument document;
    seedDataset(document);

    const DicomPath patientName = DicomPath::element({}, DCM_PatientName);
    DicomEditorService::editValue(document, EditRequest{.path = patientName, .value = "Persisted^Patient"});

    const auto output = std::filesystem::temp_directory_path() / "dicom_editor_persistence_test.dcm";
    REQUIRE(document.saveAs(output).has_value());

    DicomDocument reloaded;
    REQUIRE(reloaded.load(output).has_value());
    REQUIRE(stringValue(reloaded, patientName) == "Persisted^Patient");

    std::filesystem::remove(output);
}

TEST_CASE("dataset nodes include nested sequence attributes", "[core][nodes]") {
    DicomDocument document;
    seedDataset(document);
    const auto nodes = document.nodes();

    const auto hasKeyword = [&nodes](std::string_view keyword) {
        return std::ranges::any_of(nodes, [keyword](const auto &node) { return node.keyword == keyword; });
    };
    REQUIRE(hasKeyword("PatientName"));
    REQUIRE(hasKeyword("ReferencedStudySequence"));
    REQUIRE(hasKeyword("ReferencedSOPInstanceUID"));
}

TEST_CASE("dataset nodes preserve full values", "[core][nodes]") {
    DicomDocument document;
    const std::string longValue(200, 'x');
    document.dataset().putAndInsertString(DCM_PatientComments, longValue.c_str());

    const auto nodes = document.nodes();
    const auto node = std::ranges::find_if(nodes, [](const auto &entry) { return entry.keyword == "PatientComments"; });
    REQUIRE(node != nodes.end());
    REQUIRE(node->value == longValue);
    REQUIRE(node->valuePreview.size() == 160);
}

TEST_CASE("pixel data is not exposed as editable text", "[core][pixel-data]") {
    DicomDocument document;
    const Uint8 pixelData[]{0x00, 0x7f, 0xff};
    document.dataset().putAndInsertUint8Array(DCM_PixelData, pixelData, 3);

    const auto nodes = document.nodes();
    const auto node = std::ranges::find_if(nodes, [](const auto &entry) { return entry.keyword == "PixelData"; });
    REQUIRE(node != nodes.end());
    REQUIRE(node->value == "[Double-click to view Pixel Data]");
    REQUIRE(node->valuePreview.empty());
    REQUIRE(!node->editable);
}

TEST_CASE("pixel data renders as a preview", "[core][pixel-data]") {
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
    REQUIRE(preview.message.empty());
    REQUIRE(preview.width == 2);
    REQUIRE(preview.height == 2);
    REQUIRE(preview.channels == 1);
    REQUIRE(preview.frameIndex == 0);
    REQUIRE(preview.frameCount == 1);
    REQUIRE(preview.pixels.size() == 4);
}

TEST_CASE("dataset view model filters and formats nodes", "[core][view-model]") {
    dicom_editor::DatasetViewModel model;
    dicom_editor::DicomNode node;
    node.keyword = "PatientName";
    node.tag = "(0010,0010)";
    node.valuePreview = "Example^Patient";
    node.depth = 2;
    model.setNodes({node});

    REQUIRE(model.visibleIndices().size() == 1);
    REQUIRE(dicom_editor::DatasetViewModel::attributeLabel(node) == "    PatientName");
    model.setFilter("example^patient");
    REQUIRE(model.visibleIndices().size() == 1);
    model.setFilter("missing");
    REQUIRE(model.visibleIndices().empty());
}

TEST_CASE("dataset view model collapses sequences", "[core][view-model]") {
    DicomDocument document;
    seedDataset(document);
    dicom_editor::DatasetViewModel model;
    model.setNodes(document.nodes());
    const auto sequence = std::ranges::find_if(model.nodes(), [](const auto &node) {
        return node.kind == dicom_editor::DicomNodeKind::Sequence && node.keyword == "ReferencedStudySequence";
    });
    REQUIRE(sequence != model.nodes().end());
    const auto expandedCount = model.visibleIndices().size();

    model.toggleSequence(sequence->path);
    REQUIRE(model.sequenceCollapsed(sequence->path));
    REQUIRE(model.visibleIndices().size() < expandedCount);
    model.setFilter("ReferencedSOPInstanceUID");
    REQUIRE(!model.visibleIndices().empty());
}

TEST_CASE("tag parser accepts only 16-bit hexadecimal components", "[core][validation]") {
    const auto tag = dicom_editor::parseTagKey("0010", "0010");
    REQUIRE(tag.has_value());
    REQUIRE(tag == DCM_PatientName);
    REQUIRE(!dicom_editor::parseTagKey("nope", "0010"));
    REQUIRE(!dicom_editor::parseTagKey("10000", "0010"));
}

TEST_CASE("controller opens and navigates multiple files", "[application][workspace]") {
    const auto directory = std::filesystem::temp_directory_path();
    const auto firstPath = directory / "dicom_editor_workspace_first.dcm";
    const auto secondPath = directory / "dicom_editor_workspace_second.dcm";

    DicomDocument first;
    seedDataset(first);
    first.dataset().putAndInsertString(DCM_PatientID, "PATIENT-1");
    first.dataset().putAndInsertString(DCM_StudyDescription, "Workspace study");
    first.dataset().putAndInsertString(DCM_SeriesDescription, "Series A");
    first.dataset().putAndInsertString(DCM_InstanceNumber, "10");
    REQUIRE(first.saveAs(firstPath).has_value());

    DicomDocument second;
    seedDataset(second);
    second.dataset().putAndInsertString(DCM_PatientID, "PATIENT-1");
    second.dataset().putAndInsertString(DCM_StudyDescription, "Workspace study");
    second.dataset().putAndInsertString(DCM_SeriesDescription, "Series A");
    second.dataset().putAndInsertString(DCM_InstanceNumber, "2");
    REQUIRE(second.saveAs(secondPath).has_value());

    ControllerView view;
    view.chosenFiles = {firstPath, secondPath};
    EditorController controller(view);
    controller.openDocument();
    REQUIRE(view.error.empty());
    REQUIRE(view.openFiles.size() == 2);
    REQUIRE(view.openFiles[0].active);
    REQUIRE(view.documentStatus.starts_with("File 1 of 2"));
    REQUIRE(view.openFiles[0].hierarchy.patientId == "PATIENT-1");
    REQUIRE(view.openFiles[0].hierarchy.studyLabel == "Workspace study");

    controller.setPixelDataVisible(true);
    REQUIRE(view.pixelPreview.has_value());
    REQUIRE(previewSourceIndex(view) == 0);
    controller.showNextDocument();
    REQUIRE(view.openFiles[1].active);
    REQUIRE(view.documentStatus.starts_with("File 2 of 2"));
    REQUIRE(view.pixelPreview.has_value());
    REQUIRE(previewSourceIndex(view) == 1);
    controller.showPreviousDocument();
    REQUIRE(view.openFiles[0].active);
    REQUIRE(view.pixelPreview.has_value());
    REQUIRE(previewSourceIndex(view) == 0);

    std::filesystem::remove(firstPath);
    std::filesystem::remove(secondPath);
}

TEST_CASE("DICOMDIR documents are recognized and skipped", "[core][workspace][dicomdir]") {
    const auto path = std::filesystem::temp_directory_path() / "DICOMDIR";
    DicomDocument directory;
    seedDataset(directory);
    directory.dataset().putAndInsertString(DCM_SOPClassUID, UID_MediaStorageDirectoryStorage);
    REQUIRE(directory.isDicomDirectory());
    REQUIRE(directory.saveAs(path).has_value());

    DicomDocument reloaded;
    REQUIRE(reloaded.load(path).has_value());
    REQUIRE(reloaded.isDicomDirectory());

    DicomWorkspace workspace;
    const auto result = workspace.open({path});
    REQUIRE(result.opened == 0);
    REQUIRE(result.dicomDirectories == 1);
    REQUIRE(result.failures.empty());
    REQUIRE(workspace.size() == 1);
    REQUIRE(!workspace.active().hasFilePath());

    std::filesystem::remove(path);
}

TEST_CASE("workspace sorts by instance number or filename", "[core][workspace]") {
    const auto directory = std::filesystem::temp_directory_path();
    const auto firstPath = directory / "z-last-name.dcm";
    const auto secondPath = directory / "a-first-name.dcm";
    DicomDocument first;
    seedDataset(first);
    first.dataset().putAndInsertString(DCM_InstanceNumber, "2");
    REQUIRE(first.saveAs(firstPath).has_value());
    DicomDocument second;
    seedDataset(second);
    second.dataset().putAndInsertString(DCM_InstanceNumber, "10");
    REQUIRE(second.saveAs(secondPath).has_value());

    DicomWorkspace workspace;
    REQUIRE(workspace.open({secondPath, firstPath}).opened == 2);
    const auto byInstance = workspace.files();
    REQUIRE(byInstance[0].path == firstPath);
    REQUIRE(byInstance[0].active);
    const auto byFilename = workspace.files(dicom_editor::FileSortOrder::Filename);
    REQUIRE(byFilename[0].path == secondPath);
    REQUIRE(workspace.activateNext());
    REQUIRE(workspace.active().filePath() == secondPath);
    REQUIRE(workspace.activatePrevious());
    REQUIRE(workspace.active().filePath() == firstPath);

    std::filesystem::remove(firstPath);
    std::filesystem::remove(secondPath);
}

TEST_CASE("batch editing reports differences and updates its scope", "[core][workspace][editing]") {
    const auto directory = std::filesystem::temp_directory_path();
    const auto firstPath = directory / "dicom_editor_batch_first.dcm";
    const auto secondPath = directory / "dicom_editor_batch_second.dcm";
    DicomDocument first;
    seedDataset(first);
    first.dataset().putAndInsertString(DCM_PatientID, "BATCH-PATIENT");
    REQUIRE(first.saveAs(firstPath).has_value());
    DicomDocument second;
    seedDataset(second);
    second.dataset().putAndInsertString(DCM_PatientID, "BATCH-PATIENT");
    second.dataset().putAndInsertString(DCM_PatientName, "Different^Patient");
    REQUIRE(second.saveAs(secondPath).has_value());

    DicomWorkspace workspace;
    REQUIRE(workspace.open({firstPath, secondPath}).opened == 2);
    const dicom_editor::BatchEditTarget target{
        .level = dicom_editor::BatchEditLevel::Patient, .id = "BATCH-PATIENT", .label = "Before^Patient"};
    const auto report = workspace.batchEditReport(target);
    REQUIRE(report.documentCount == 2);
    REQUIRE(report.attributes.front().values.size() == 2);
    REQUIRE(workspace.batchEdit(target, DCM_PatientName, "Unified^Patient") == 2);
    REQUIRE(workspace.at(0).attributeValue(DCM_PatientName) == "Unified^Patient");
    REQUIRE(workspace.at(1).attributeValue(DCM_PatientName) == "Unified^Patient");
    REQUIRE(workspace.at(0).dirty());
    REQUIRE(workspace.at(1).dirty());

    std::filesystem::remove(firstPath);
    std::filesystem::remove(secondPath);
}

TEST_CASE("hierarchy cache reflects mutations", "[core][document]") {
    DicomDocument document;
    seedDataset(document);
    REQUIRE(document.hierarchy().patientLabel == "Before^Patient");

    document.dataset().putAndInsertString(DCM_PatientName, "Direct^Mutation");
    REQUIRE(document.hierarchy().patientLabel == "Direct^Mutation");

    DicomEditorService::setAttribute(document, DCM_PatientName, "Service^Mutation", true);
    REQUIRE(document.hierarchy().patientLabel == "Service^Mutation");
}

TEST_CASE("controller saves all documents and clears workspace", "[application][workspace]") {
    const auto directory = std::filesystem::temp_directory_path();
    const auto firstPath = directory / "dicom_editor_save_all_first.dcm";
    const auto secondPath = directory / "dicom_editor_save_all_second.dcm";
    DicomDocument first;
    seedDataset(first);
    first.dataset().putAndInsertString(DCM_PatientID, "SAVE-ALL");
    REQUIRE(first.saveAs(firstPath).has_value());
    DicomDocument second;
    seedDataset(second);
    second.dataset().putAndInsertString(DCM_PatientID, "SAVE-ALL");
    REQUIRE(second.saveAs(secondPath).has_value());

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
    REQUIRE(view.documentPresentations == 1);
    REQUIRE(view.openFilesPresentations == 1);
    // cppcheck-suppress knownConditionTrueFalse
    REQUIRE(view.pixelPresentations == 0);
    REQUIRE(controller.actionState(nullptr).saveAllEnabled);
    view.documentPresentations = 0;
    view.openFilesPresentations = 0;
    view.pixelPresentations = 0;
    REQUIRE(controller.saveAllDocuments());
    REQUIRE(view.documentPresentations == 1);
    REQUIRE(view.openFilesPresentations == 1);
    // cppcheck-suppress knownConditionTrueFalse
    REQUIRE(view.pixelPresentations == 1);
    REQUIRE(std::ranges::find_if(view.openFiles, [](const auto &file) { return file.active; })->path == activePath);
    REQUIRE(!controller.actionState(nullptr).saveAllEnabled);

    DicomDocument reloadedFirst;
    DicomDocument reloadedSecond;
    REQUIRE(reloadedFirst.load(firstPath).has_value());
    REQUIRE(reloadedSecond.load(secondPath).has_value());
    REQUIRE(reloadedFirst.attributeValue(DCM_PatientName) == "Saved^Together");
    REQUIRE(reloadedSecond.attributeValue(DCM_PatientName) == "Saved^Together");

    controller.clearWorkspace();
    REQUIRE(!view.hasLoadedFiles);
    REQUIRE(view.openFiles.size() == 1);
    REQUIRE(view.status == "Workspace cleared.");

    std::filesystem::remove(firstPath);
    std::filesystem::remove(secondPath);
}

TEST_CASE("DICOMDIR discovery resolves referenced files", "[core][workspace][dicomdir]") {
    const auto directory = std::filesystem::temp_directory_path() / "dicom_editor_dicomdir_test";
    const auto imagePath = directory / "IMG0001";
    const auto dicomdirPath = directory / "DICOMDIR";
    std::filesystem::create_directories(directory);
    DicomDocument image;
    seedDataset(image);
    REQUIRE(image.saveAs(imagePath).has_value());
    {
        DcmDicomDir dicomdir(dicomdirPath.string().c_str(), "EDITOR_TEST");
        auto *patient = new DcmDirectoryRecord(ERT_Patient, nullptr, OFFilename());
        auto *study = new DcmDirectoryRecord(ERT_Study, nullptr, OFFilename());
        auto *series = new DcmDirectoryRecord(ERT_Series, nullptr, OFFilename());
        auto *imageRecord = new DcmDirectoryRecord(ERT_Image, "IMG0001", imagePath.string().c_str());
        REQUIRE(patient->error().good());
        REQUIRE(study->error().good());
        REQUIRE(series->error().good());
        REQUIRE(imageRecord->error().good());
        REQUIRE(series->insertSub(imageRecord).good());
        REQUIRE(study->insertSub(series).good());
        REQUIRE(patient->insertSub(study).good());
        REQUIRE(dicomdir.getRootRecord().insertSub(patient).good());
        REQUIRE(dicomdir.write().good());
    }

    const auto referenced = DicomWorkspace::discoverDicomDirectory(dicomdirPath);
    REQUIRE(referenced.has_value());
    REQUIRE(referenced->size() == 1);
    REQUIRE(referenced->front() == imagePath);
    DicomWorkspace workspace;
    REQUIRE(workspace.open(*referenced).opened == 1);

    std::filesystem::remove_all(directory);
}

TEST_CASE("dictionary overrides are validated and activated", "[core][dictionary]") {
    REQUIRE(dicom_editor::dicomDictionarySource() == "embedded DCMTK dictionary");

    const auto directory = std::filesystem::temp_directory_path();
    const auto invalidPath = directory / "dicom_editor_invalid_dictionary.dic";
    {
        std::ofstream invalid(invalidPath);
        invalid << "not a DCMTK dictionary\n";
    }
    REQUIRE(!dicom_editor::loadDicomDictionary(invalidPath).has_value());
    REQUIRE(dicom_editor::dicomDictionarySource() == "embedded DCMTK dictionary");

    const auto validPath = directory / "dicom_editor_override_dictionary.dic";
    {
        std::ofstream valid(validPath);
        valid << "(7777,0010)\tLO\tEditorTestAttribute\t1\tTEST\n";
    }
    const auto loaded = dicom_editor::loadDicomDictionary(validPath);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entryCount == 1);
    REQUIRE(loaded->source == validPath.string());
    DcmTag customTag(DcmTagKey(0x7777, 0x0010));
    REQUIRE(std::string_view(customTag.getTagName()) == "EditorTestAttribute");

    std::filesystem::remove(invalidPath);
    std::filesystem::remove(validPath);
}

} // namespace
