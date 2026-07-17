#pragma once

#include "dicom_editor/AttributeInput.hpp"
#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomEditorService.hpp"

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

class EditorControllerHost {
  public:
    virtual ~EditorControllerHost() = default;

    [[nodiscard]] virtual std::optional<std::filesystem::path> chooseOpenFile() = 0;
    [[nodiscard]] virtual std::optional<std::filesystem::path> chooseSaveFile() = 0;
    [[nodiscard]] virtual SaveChangesChoice confirmSaveChanges() = 0;
    [[nodiscard]] virtual bool confirmDelete() = 0;
    [[nodiscard]] virtual std::optional<AttributeInput> editAttribute(const std::string &title, const std::string &value) = 0;
    [[nodiscard]] virtual std::optional<AttributeInput> addAttribute() = 0;
    virtual void showError(const std::string &message) = 0;
    virtual void presentDocument(std::vector<DicomNode> nodes, const std::string &title, const std::string &status) = 0;
    virtual void setStatus(const std::string &status) = 0;
};

class EditorController {
  public:
    explicit EditorController(EditorControllerHost &host);

    void refresh();
    void open();
    bool save();
    bool saveAs();
    void editSelected(const DicomNode *selected);
    void editValue(const DicomPath &path, const std::string &value);
    void add(const DicomNode *selected);
    void remove(const DicomNode *selected);
    [[nodiscard]] bool confirmClose();
    [[nodiscard]] ActionState actionState(const DicomNode *selected) const;

  private:
    [[nodiscard]] bool confirmDiscardChanges();
    bool saveTo(const std::optional<std::filesystem::path> &path);
    void reportError(const std::exception &error, bool refreshAfter);

    EditorControllerHost &host_;
    DicomDocument document_;
    DicomEditorService editor_;
};

} // namespace dicom_editor
