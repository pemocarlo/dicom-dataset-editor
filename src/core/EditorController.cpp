#include "dicom_editor/EditorController.hpp"

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
#include <system_error>
#include <utility>
#include <vector>

namespace dicom_editor {

namespace {

std::filesystem::path normalizedPath(const std::filesystem::path &path) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);
    return error ? std::filesystem::absolute(path, error).lexically_normal() : normalized;
}

} // namespace

EditorController::EditorController(EditorView &view) : view_(view) { documents_.emplace_back(); }

DicomDocument &EditorController::document() { return documents_[activeDocument_]; }

const DicomDocument &EditorController::document() const { return documents_[activeDocument_]; }

void EditorController::refreshView() {
    const auto &active = document();
    const std::string name = active.hasFilePath() ? active.filePath().filename().string() : "Untitled";
    const std::string title = std::format("DICOM Dataset Editor - {}{}", name, active.dirty() ? "*" : "");
    const std::string status = active.hasFilePath()
                                   ? std::format("File {} of {} | {}", activeDocument_ + 1, documents_.size(), active.filePath().string())
                                   : "New dataset";
    view_.presentDocument(active.nodes(validationEnabled_), title, status);
    view_.presentOpenFiles(openFiles());
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
    std::vector<std::filesystem::path> paths;
    std::error_code error;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iterator(*folder, options, error), end; iterator != end; iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (iterator->is_regular_file(error) && !error) {
            paths.push_back(iterator->path());
        }
        error.clear();
    }
    std::ranges::sort(paths);
    openPaths(paths);
}

void EditorController::openPaths(const std::vector<std::filesystem::path> &paths) {
    std::vector<std::string> errors;
    std::size_t opened{};
    std::size_t failed{};
    for (const auto &path : paths) {
        const auto normalized = normalizedPath(path);
        const auto duplicate = std::ranges::find_if(documents_, [&normalized](const DicomDocument &candidate) {
            return candidate.hasFilePath() && normalizedPath(candidate.filePath()) == normalized;
        });
        if (duplicate != documents_.end()) {
            activeDocument_ = static_cast<std::size_t>(std::distance(documents_.begin(), duplicate));
            continue;
        }

        DicomDocument loaded;
        const auto result = loaded.load(path);
        if (!result) {
            ++failed;
            if (errors.size() < 8) {
                errors.push_back(std::format("{}: {}", path.string(), result.error().what()));
            }
            continue;
        }

        if (documents_.size() == 1 && !document().hasFilePath() && !document().dirty()) {
            documents_.clear();
        }
        documents_.push_back(std::move(loaded));
        activeDocument_ = documents_.size() - 1;
        ++opened;
    }

    pixelFrame_ = 0;
    refreshView();
    if (!errors.empty()) {
        std::string message = std::format("Opened {} DICOM file(s). Skipped {} non-DICOM or unreadable file(s).", opened, failed);
        for (const auto &error : errors) {
            message += "\n" + error;
        }
        if (failed > errors.size()) {
            message += std::format("\n...and {} more.", failed - errors.size());
        }
        view_.showError(message);
    } else if (paths.empty()) {
        view_.showError("The selected folder contains no regular files.");
    }
}

void EditorController::activateDocument(std::size_t index) {
    if (index >= documents_.size() || index == activeDocument_) {
        return;
    }
    activeDocument_ = index;
    pixelFrame_ = 0;
    refreshView();
}

void EditorController::showPreviousDocument() {
    if (activeDocument_ > 0) {
        activateDocument(activeDocument_ - 1);
    }
}

void EditorController::showNextDocument() {
    if (activeDocument_ + 1 < documents_.size()) {
        activateDocument(activeDocument_ + 1);
    }
}

bool EditorController::saveDocument() { return document().hasFilePath() ? saveTo(std::nullopt) : saveDocumentAs(); }

bool EditorController::saveDocumentAs() {
    const auto path = view_.chooseSaveFile();
    return path ? saveTo(path) : false;
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
    preview.sourceIndex = activeDocument_;
    preview.sourceCount = documents_.size();
    view_.presentPixelData(std::move(preview));
}

bool EditorController::confirmClose() {
    const std::size_t original = activeDocument_;
    for (std::size_t index = 0; index < documents_.size(); ++index) {
        if (!documents_[index].dirty()) {
            continue;
        }
        activeDocument_ = index;
        refreshView();
        if (!confirmDiscardChanges()) {
            return false;
        }
    }
    activeDocument_ = original;
    return true;
}

ActionState EditorController::actionState(const DicomNode *selected) const {
    const bool editable = selected != nullptr && selected->editable;
    return {.saveEnabled = document().dirty() || !document().hasFilePath(), .editEnabled = editable, .deleteEnabled = editable};
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

std::vector<OpenDicomFile> EditorController::openFiles() const {
    std::vector<OpenDicomFile> result;
    result.reserve(documents_.size());
    for (std::size_t index = 0; index < documents_.size(); ++index) {
        const auto &entry = documents_[index];
        result.push_back({.index = index,
                          .path = entry.filePath(),
                          .hierarchy = entry.hierarchy(),
                          .dirty = entry.dirty(),
                          .active = index == activeDocument_});
    }
    return result;
}

} // namespace dicom_editor
