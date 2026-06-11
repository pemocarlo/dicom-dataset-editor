#include "dicom_editor/EditorController.hpp"

#include "dicom_editor/DicomNode.hpp"
#include "dicom_editor/DicomPath.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <exception>
#include <filesystem>
#include <optional>
#include <string>

namespace dicom_editor {

EditorController::EditorController(EditorControllerHost &host) : host_(host) {}

void EditorController::refresh() {
    const std::string name = document_.hasFilePath() ? document_.filePath().filename().string() : "Untitled";
    const std::string title = "DICOM Dataset Editor - " + name + (document_.dirty() ? "*" : "");
    const std::string status = document_.hasFilePath() ? document_.filePath().string() : "New dataset";
    host_.presentDocument(document_.nodes(), title, status);
}

void EditorController::open() {
    if (!confirmDiscardChanges()) {
        return;
    }
    const auto path = host_.chooseOpenFile();
    if (!path) {
        return;
    }
    try {
        document_.load(*path);
        refresh();
    } catch (const std::exception &error) {
        reportError(error, false);
    }
}

bool EditorController::save() { return document_.hasFilePath() ? saveTo(std::nullopt) : saveAs(); }

bool EditorController::saveAs() {
    const auto path = host_.chooseSaveFile();
    return path ? saveTo(path) : false;
}

void EditorController::editSelected(const DicomNode *selected) {
    if (selected == nullptr || !selected->editable) {
        return;
    }
    const auto result = host_.editAttribute("Edit " + selected->keyword, selected->value);
    if (result) {
        editValue(selected->path, result->value);
    }
}

void EditorController::editValue(const DicomPath &path, const std::string &value) {
    try {
        editor_.editValue(document_, {.path = path, .value = value});
        refresh();
    } catch (const std::exception &error) {
        reportError(error, true);
    }
}

void EditorController::add(const DicomNode *selected) {
    DicomPath parent = DicomPath::dataset();
    if (selected != nullptr) {
        parent = selected->kind == DicomNodeKind::Item ? selected->path : DicomPath::item(selected->path.parents());
    }

    const auto result = host_.addAttribute();
    if (!result || !result->tag) {
        return;
    }
    try {
        editor_.addAttribute(document_, {.parentItemPath = parent, .tag = *result->tag, .value = result->value});
        refresh();
    } catch (const std::exception &error) {
        reportError(error, false);
    }
}

void EditorController::remove(const DicomNode *selected) {
    if (selected == nullptr || !selected->editable || !host_.confirmDelete()) {
        return;
    }
    try {
        editor_.deleteAttribute(document_, selected->path);
        refresh();
    } catch (const std::exception &error) {
        reportError(error, false);
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
    switch (host_.confirmSaveChanges()) {
    case SaveChangesChoice::Cancel:
        return false;
    case SaveChangesChoice::Discard:
        return true;
    case SaveChangesChoice::Save:
        return save();
    }
    return false;
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
        refresh();
        host_.setStatus("Saved and reloaded successfully: " + document_.filePath().string());
        return true;
    } catch (const std::exception &error) {
        reportError(error, false);
        return false;
    }
}

void EditorController::reportError(const std::exception &error, bool refreshAfter) {
    host_.showError(error.what());
    if (refreshAfter) {
        refresh();
    }
}

} // namespace dicom_editor
