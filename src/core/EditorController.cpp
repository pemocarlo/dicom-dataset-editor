#include "dicom_editor/EditorController.hpp"

#include "dicom_editor/DicomEditorService.hpp"
#include "dicom_editor/DicomNode.hpp"
#include "dicom_editor/DicomPath.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <exception>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace dicom_editor {

EditorController::EditorController(EditorView &view) : view_(view) {}

void EditorController::refreshView() {
    const std::string name = document_.hasFilePath() ? document_.filePath().filename().string() : "Untitled";
    const std::string title = std::format("DICOM Dataset Editor - {}{}", name, document_.dirty() ? "*" : "");
    const std::string status = document_.hasFilePath() ? document_.filePath().string() : "New dataset";
    view_.presentDocument(document_.nodes(validationEnabled_), title, status);
}

void EditorController::openDocument() {
    if (!confirmDiscardChanges()) {
        return;
    }
    const auto path = view_.chooseOpenFile();
    if (!path) {
        return;
    }
    try {
        document_.load(*path);
        refreshView();
    } catch (const std::exception &error) {
        reportError(error, false);
    }
}

bool EditorController::saveDocument() { return document_.hasFilePath() ? saveTo(std::nullopt) : saveDocumentAs(); }

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
        DicomEditorService::editValue(document_, {.path = path, .value = value, .validate = validationEnabled_});
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
            document_, {.parentItemPath = parent, .tag = *result->tag, .value = result->value, .validate = validationEnabled_});
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
        DicomEditorService::deleteAttribute(document_, selected->path);
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

bool EditorController::confirmClose() { return confirmDiscardChanges(); }

ActionState EditorController::actionState(const DicomNode *selected) const {
    const bool editable = selected != nullptr && selected->editable;
    return {.saveEnabled = document_.dirty() || !document_.hasFilePath(), .editEnabled = editable, .deleteEnabled = editable};
}

bool EditorController::confirmDiscardChanges() {
    if (!document_.dirty()) {
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
    if (!document_.hasFilePath() && !path) {
        return false;
    }
    if (path && path->empty()) {
        return false;
    }
    try {
        if (path) {
            document_.saveAs(*path);
        } else {
            document_.save();
        }
        DicomDocument verified;
        verified.load(document_.filePath());
        refreshView();
        view_.setStatus(std::format("Saved and reloaded successfully: {}", document_.filePath().string()));
        return true;
    } catch (const std::exception &error) {
        reportError(error, false);
        return false;
    }
}

void EditorController::reportError(const std::exception &error, bool refreshAfter) {
    view_.showError(error.what());
    if (refreshAfter) {
        refreshView();
    }
}

} // namespace dicom_editor
