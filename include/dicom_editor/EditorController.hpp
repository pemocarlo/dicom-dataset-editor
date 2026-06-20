#pragma once

#include "dicom_editor/AttributeInput.hpp"
#include "dicom_editor/DicomWorkspace.hpp"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dicom_editor {

class DicomDocument;
class DicomPath;
struct DicomNode;
struct PixelDataPreview;

/// User choice when closing or reopening with unsaved changes.
enum class SaveChangesChoice {
    /// Abort the action.
    Cancel,
    /// Continue without saving.
    Discard,
    /// Save first.
    Save,
};

/// Enablement state for document actions.
struct ActionState {
    /// Save action enabled.
    bool saveEnabled{};
    /// Edit action enabled.
    bool editEnabled{};
    /// Delete action enabled.
    bool deleteEnabled{};
};

/// View interface used by the controller.
class EditorView {
  public:
    /// Destroys the view.
    virtual ~EditorView() = default;

    /// Prompts for a file to open.
    [[nodiscard]] virtual std::vector<std::filesystem::path> chooseOpenFiles() = 0;
    /// Prompts for a folder to scan recursively.
    [[nodiscard]] virtual std::optional<std::filesystem::path> chooseOpenFolder() = 0;
    /// Prompts for a file to save.
    [[nodiscard]] virtual std::optional<std::filesystem::path> chooseSaveFile() = 0;
    /// Resolves unsaved changes.
    [[nodiscard]] virtual SaveChangesChoice confirmSaveChanges() = 0;
    /// Confirms deletion of the current selection.
    [[nodiscard]] virtual bool confirmDelete() = 0;
    /// Collects a replacement value for an existing attribute.
    [[nodiscard]] virtual std::optional<AttributeInput> editAttribute(const std::string &title, const std::string &value) = 0;
    /// Collects the fields for a new attribute.
    [[nodiscard]] virtual std::optional<AttributeInput> addAttribute() = 0;
    /// Shows an error message.
    virtual void showError(const std::string &message) = 0;
    /// Updates the document tree and window title.
    virtual void presentDocument(std::vector<DicomNode> nodes, const std::string &title, const std::string &status) = 0;
    /// Updates the patient/study/series/file hierarchy.
    virtual void presentOpenFiles(const std::vector<OpenDicomFile> &files, bool hasLoadedFiles) = 0;
    /// Shows or hides the pixel preview.
    virtual void presentPixelData(std::optional<PixelDataPreview> preview) = 0;
    /// Updates the status bar text.
    virtual void setStatus(const std::string &status) = 0;
};

/// Coordinates document loading, editing, and preview state.
class EditorController {
  public:
    /// Binds the controller to its view.
    explicit EditorController(EditorView &view);

    /// Pushes the current document state into the view.
    void refreshView();
    /// Opens a document chosen by the view.
    void openDocument();
    /// Recursively opens all valid DICOM files from a chosen folder.
    void openFolder();
    /// Makes an open file active.
    void activateDocument(std::size_t index);
    /// Moves to the previous open file.
    void showPreviousDocument();
    /// Moves to the next open file.
    void showNextDocument();
    /// Saves the document.
    bool saveDocument();
    /// Saves the document to a new file.
    bool saveDocumentAs();
    /// Edits the selected attribute when it is editable.
    void editSelected(const DicomNode *selected);
    /// Adds an attribute near the selected row.
    void addAttribute(const DicomNode *selected);
    /// Deletes the selected attribute.
    void deleteAttribute(const DicomNode *selected);
    /// Enables or disables value validation.
    void setValidationEnabled(bool enabled);
    /// Shows or hides pixel data.
    void setPixelDataVisible(bool visible);
    /// Moves to the previous pixel frame.
    void showPreviousPixelFrame();
    /// Moves to the next pixel frame.
    void showNextPixelFrame();
    /// Returns `false` if closing is canceled due to unsaved changes.
    [[nodiscard]] bool confirmClose();
    /// Computes the current action enablement state.
    [[nodiscard]] ActionState actionState(const DicomNode *selected) const;

  private:
    [[nodiscard]] bool confirmDiscardChanges();
    bool saveTo(const std::optional<std::filesystem::path> &path);
    void editValue(const DicomPath &path, const std::string &value);
    void refreshPixelData();
    void reportError(const std::exception &error, bool refreshAfter);
    void openPaths(const std::vector<std::filesystem::path> &paths);
    [[nodiscard]] DicomDocument &document();
    [[nodiscard]] const DicomDocument &document() const;

    EditorView &view_;
    DicomWorkspace workspace_;
    bool validationEnabled_{true};
    bool pixelDataVisible_{};
    unsigned long pixelFrame_{};
    unsigned long pixelFrameCount_{};
};

} // namespace dicom_editor
