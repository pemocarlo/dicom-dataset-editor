#include "dicom_editor/EditorController.hpp"

#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomEditorService.hpp"
#include "dicom_editor/DicomError.hpp"
#include "dicom_editor/DicomNode.hpp"
#include "dicom_editor/DicomPath.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <algorithm>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace dicom_editor {

EditorController::EditorController(EditorView &view) : view_(view) {}

DicomDocument &EditorController::document() { return workspace_.active(); }

const DicomDocument &EditorController::document() const { return workspace_.active(); }

void EditorController::refreshView() {
    const auto &active = document();
    const std::string name = active.hasFilePath() ? active.filePath().filename().string() : "Untitled";
    const std::string title = std::format("DICOM Dataset Editor - {}{}", name, active.dirty() ? "*" : "");
    const std::string status = active.hasFilePath() ? std::format("File {} of {} | {}", workspace_.activeIndex() + 1, workspace_.size(),
                                                                  active.filePath().string())
                                                    : "New dataset";
    view_.presentDocument(active.nodes(validationEnabled_), title, status);
    view_.presentOpenFiles(workspace_.files(fileSortOrder_), workspace_.hasLoadedFiles());
    refreshPixelData();
}

void EditorController::openDocument() {
    const auto paths = view_.chooseOpenFiles();
    if (paths.empty()) {
        return;
    }
    openPaths(paths);
}

void EditorController::openFolder() {
    const auto folder = view_.chooseOpenFolder();
    if (!folder) {
        return;
    }
    openPaths(DicomWorkspace::discoverFiles(*folder));
}

void EditorController::openDicomDirectory() {
    const auto path = view_.chooseDicomDirectory();
    if (!path) {
        return;
    }
    const auto paths = DicomWorkspace::discoverDicomDirectory(*path);
    if (!paths) {
        view_.showError(paths.error().what());
        return;
    }
    if (paths->empty()) {
        view_.showError("DICOMDIR contains no referenced files.");
        return;
    }
    openPaths(*paths);
}

void EditorController::openPaths(const std::vector<std::filesystem::path> &paths) {
    const auto summary = workspace_.open(paths);
    pixelFrame_ = 0;
    refreshView();
    if (!summary.failures.empty()) {
        std::string message =
            std::format("Opened {} DICOM file(s). Skipped {} non-DICOM or unreadable file(s).", summary.opened, summary.failures.size());
        const auto displayed = std::min<std::size_t>(summary.failures.size(), 8);
        for (std::size_t index = 0; index < displayed; ++index) {
            const auto &failure = summary.failures[index];
            message += std::format("\n{}: {}", failure.path.string(), failure.message);
        }
        if (summary.failures.size() > displayed) {
            message += std::format("\n...and {} more.", summary.failures.size() - displayed);
        }
        view_.showError(message);
    } else if (paths.empty()) {
        view_.showError("The selected folder contains no regular files.");
    }
    if (summary.dicomDirectories > 0) {
        view_.setStatus(std::format("Skipped {} DICOMDIR file(s); opened {} dataset(s).", summary.dicomDirectories, summary.opened));
    }
}

void EditorController::activateDocument(std::size_t index) {
    if (!workspace_.activate(index)) {
        return;
    }
    pixelFrame_ = 0;
    refreshView();
}

void EditorController::showPreviousDocument() {
    if (workspace_.activatePrevious(fileSortOrder_)) {
        pixelFrame_ = 0;
        refreshView();
    }
}

void EditorController::showNextDocument() {
    if (workspace_.activateNext(fileSortOrder_)) {
        pixelFrame_ = 0;
        refreshView();
    }
}

bool EditorController::saveDocument() { return document().hasFilePath() ? saveTo(std::nullopt) : saveDocumentAs(); }

bool EditorController::saveDocumentAs() {
    const auto path = view_.chooseSaveFile();
    return path ? saveTo(path) : false;
}

bool EditorController::saveAllDocuments() {
    const std::size_t original = workspace_.activeIndex();
    std::size_t saved{};
    for (std::size_t index = 0; index < workspace_.size(); ++index) {
        if (!workspace_.at(index).dirty()) {
            continue;
        }
        static_cast<void>(workspace_.activate(index));
        refreshView();
        if (!saveDocument()) {
            return false;
        }
        ++saved;
    }
    static_cast<void>(workspace_.activate(original));
    refreshView();
    view_.setStatus(std::format("Saved {} modified dataset(s).", saved));
    return true;
}

void EditorController::clearWorkspace() {
    if (!confirmClose()) {
        return;
    }
    workspace_.clear();
    pixelFrame_ = 0;
    refreshView();
    view_.setStatus("Workspace cleared.");
}

void EditorController::editSelected(const DicomNode *selected) {
    if (selected == nullptr || !selected->editable) {
        return;
    }
    const auto result = view_.editAttribute(std::format("Edit {}", selected->keyword), selected->value);
    if (result) {
        editValue(selected->path, result->value);
    }
}

void EditorController::editValue(const DicomPath &path, const std::string &value) {
    try {
        DicomEditorService::editValue(document(), {.path = path, .value = value, .validate = validationEnabled_});
        refreshView();
    } catch (const std::exception &error) {
        reportError(error, true);
    }
}

void EditorController::addAttribute(const DicomNode *selected) {
    DicomPath parent = DicomPath::dataset();
    if (selected != nullptr) {
        parent = selected->kind == DicomNodeKind::Item ? selected->path : DicomPath::item(selected->path.parents());
    }

    const auto result = view_.addAttribute();
    if (!result || !result->tag) {
        return;
    }
    try {
        DicomEditorService::addAttribute(
            document(), {.parentItemPath = parent, .tag = *result->tag, .value = result->value, .validate = validationEnabled_});
        refreshView();
    } catch (const std::exception &error) {
        reportError(error, false);
    }
}

void EditorController::deleteAttribute(const DicomNode *selected) {
    if (selected == nullptr || !selected->editable || !view_.confirmDelete()) {
        return;
    }
    try {
        DicomEditorService::deleteAttribute(document(), selected->path);
        refreshView();
    } catch (const std::exception &error) {
        reportError(error, false);
    }
}

void EditorController::batchEdit(const BatchEditTarget &target) {
    const auto report = workspace_.batchEditReport(target);
    if (report.documentCount == 0) {
        view_.showError("No open datasets match selected group.");
        return;
    }
    const auto input = view_.batchEditAttribute(report);
    if (!input || !input->tag) {
        return;
    }
    try {
        const auto changed = workspace_.batchEdit(target, *input->tag, input->value, validationEnabled_);
        refreshView();
        view_.setStatus(std::format("Batch edit applied to {} dataset(s). Save modified files individually.", changed));
    } catch (const std::exception &error) {
        reportError(error, true);
    }
}

void EditorController::setFileSortOrder(FileSortOrder order) {
    if (fileSortOrder_ != order) {
        fileSortOrder_ = order;
        view_.presentOpenFiles(workspace_.files(fileSortOrder_), workspace_.hasLoadedFiles());
    }
}

void EditorController::setValidationEnabled(bool enabled) {
    if (validationEnabled_ != enabled) {
        validationEnabled_ = enabled;
        refreshView();
    }
}

void EditorController::setPixelDataVisible(bool visible) {
    pixelDataVisible_ = visible;
    if (visible) {
        pixelFrame_ = 0;
    }
    refreshPixelData();
}

void EditorController::showPreviousPixelFrame() {
    if (pixelDataVisible_ && pixelFrame_ > 0) {
        --pixelFrame_;
        refreshPixelData();
    }
}

void EditorController::showNextPixelFrame() {
    if (pixelDataVisible_ && pixelFrame_ + 1 < pixelFrameCount_) {
        ++pixelFrame_;
        refreshPixelData();
    }
}

void EditorController::refreshPixelData() {
    if (!pixelDataVisible_) {
        pixelFrameCount_ = 0;
        view_.presentPixelData(std::nullopt);
        return;
    }

    auto preview = document().renderPixelData(pixelFrame_);
    pixelFrame_ = preview.frameIndex;
    pixelFrameCount_ = preview.frameCount;
    preview.sourceName = document().hasFilePath() ? document().filePath().filename().string() : "Untitled";
    const auto ordered = workspace_.files(fileSortOrder_);
    const auto active = std::ranges::find_if(ordered, [](const OpenDicomFile &file) { return file.active; });
    preview.sourceIndex = static_cast<std::size_t>(std::distance(ordered.begin(), active));
    preview.sourceCount = ordered.size();
    view_.presentPixelData(std::move(preview));
}

bool EditorController::confirmClose() {
    const std::size_t dirtyCount = workspace_.dirtyDocumentCount();
    if (dirtyCount > 1) {
        using enum SaveChangesChoice;
        switch (view_.confirmWorkspaceChanges(dirtyCount)) {
        case Cancel:
            return false;
        case Discard:
            return true;
        case Save:
            return saveAllDocuments();
        default:
            std::unreachable();
        }
    }
    const std::size_t original = workspace_.activeIndex();
    for (std::size_t index = 0; index < workspace_.size(); ++index) {
        if (!workspace_.at(index).dirty()) {
            continue;
        }
        static_cast<void>(workspace_.activate(index));
        refreshView();
        if (!confirmDiscardChanges()) {
            return false;
        }
    }
    static_cast<void>(workspace_.activate(original));
    return true;
}

ActionState EditorController::actionState(const DicomNode *selected) const {
    const bool editable = selected != nullptr && selected->editable;
    return {.saveEnabled = document().dirty() || !document().hasFilePath(),
            .saveAllEnabled = workspace_.hasDirtyDocuments(),
            .clearWorkspaceEnabled = workspace_.hasLoadedFiles() || workspace_.hasDirtyDocuments(),
            .editEnabled = editable,
            .deleteEnabled = editable};
}

bool EditorController::confirmDiscardChanges() {
    if (!document().dirty()) {
        return true;
    }
    using enum SaveChangesChoice;
    switch (view_.confirmSaveChanges()) {
    case Cancel:
        return false;
    case Discard:
        return true;
    case Save:
        return saveDocument();
    default:
        std::unreachable();
    }
}

bool EditorController::saveTo(const std::optional<std::filesystem::path> &path) {
    if (!document().hasFilePath() && !path) {
        return false;
    }
    if (path && path->empty()) {
        return false;
    }

    const auto saveResult = path ? document().saveAs(*path) : document().save();
    if (!saveResult) {
        reportError(saveResult.error(), false);
        return false;
    }

    DicomDocument verified;
    const auto verifyResult = verified.load(document().filePath());
    if (!verifyResult) {
        reportError(verifyResult.error(), false);
        return false;
    }

    refreshView();
    view_.setStatus(std::format("Saved and reloaded successfully: {}", document().filePath().string()));
    return true;
}

void EditorController::reportError(const std::exception &error, bool refreshAfter) {
    view_.showError(error.what());
    if (refreshAfter) {
        refreshView();
    }
}

} // namespace dicom_editor
