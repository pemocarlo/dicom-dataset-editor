#pragma once

#include "dicom_editor/AttributeInput.hpp"
#include "dicom_editor/DicomDocument.hpp"

#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dicom_editor {

class DicomPath;
struct DicomNode;

enum class SaveChangesChoice {
    Cancel,
    Discard,
    Save,
};

struct ActionState {
    bool saveEnabled{};
    bool editEnabled{};
    bool deleteEnabled{};
};

class EditorView {
  public:
    virtual ~EditorView() = default;

    [[nodiscard]] virtual std::optional<std::filesystem::path> chooseOpenFile() = 0;
    [[nodiscard]] virtual std::optional<std::filesystem::path> chooseSaveFile() = 0;
    [[nodiscard]] virtual SaveChangesChoice confirmSaveChanges() = 0;
    [[nodiscard]] virtual bool confirmDelete() = 0;
    [[nodiscard]] virtual std::optional<AttributeInput> editAttribute(const std::string &title, const std::string &value) = 0;
    [[nodiscard]] virtual std::optional<AttributeInput> addAttribute() = 0;
    virtual void showError(const std::string &message) = 0;
    virtual void presentDocument(std::vector<DicomNode> nodes, const std::string &title, const std::string &status) = 0;
    virtual void presentPixelData(std::optional<PixelDataPreview> preview) = 0;
    virtual void setStatus(const std::string &status) = 0;
};

class EditorController {
  public:
    explicit EditorController(EditorView &view);

    void refreshView();
    void openDocument();
    bool saveDocument();
    bool saveDocumentAs();
    void editSelected(const DicomNode *selected);
    void addAttribute(const DicomNode *selected);
    void deleteAttribute(const DicomNode *selected);
    void setValidationEnabled(bool enabled);
    void setPixelDataVisible(bool visible);
    void showPreviousPixelFrame();
    void showNextPixelFrame();
    [[nodiscard]] bool confirmClose();
    [[nodiscard]] ActionState actionState(const DicomNode *selected) const;

  private:
    [[nodiscard]] bool confirmDiscardChanges();
    bool saveTo(const std::optional<std::filesystem::path> &path);
    void editValue(const DicomPath &path, const std::string &value);
    void refreshPixelData();
    void reportError(const std::exception &error, bool refreshAfter);

    EditorView &view_;
    DicomDocument document_;
    bool validationEnabled_{true};
    bool pixelDataVisible_{};
    unsigned long pixelFrame_{};
    unsigned long pixelFrameCount_{};
};

} // namespace dicom_editor
