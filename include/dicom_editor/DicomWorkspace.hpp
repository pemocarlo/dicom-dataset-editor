#pragma once

#include "dicom_editor/DicomDocument.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace dicom_editor {

/// One file displayed in the workspace hierarchy.
struct OpenDicomFile {
    std::size_t index{};
    std::filesystem::path path;
    DicomHierarchy hierarchy;
    bool dirty{};
    bool active{};
};

/// Failure produced while opening one candidate file.
struct FileOpenFailure {
    std::filesystem::path path;
    std::string message;
};

/// Summary from adding files to a workspace.
struct OpenFilesResult {
    std::size_t opened{};
    std::size_t duplicates{};
    std::size_t dicomDirectories{};
    std::vector<FileOpenFailure> failures;
};

/// Owns open DICOM datasets and active-file navigation.
class DicomWorkspace {
  public:
    DicomWorkspace();

    /// Adds valid datasets, ignores duplicates, and skips DICOMDIR files.
    [[nodiscard]] OpenFilesResult open(const std::vector<std::filesystem::path> &paths);
    /// Returns recursively discovered regular files in stable path order.
    [[nodiscard]] static std::vector<std::filesystem::path> discoverFiles(const std::filesystem::path &folder);

    /// Returns active document.
    [[nodiscard]] DicomDocument &active();
    /// Returns active document.
    [[nodiscard]] const DicomDocument &active() const;
    /// Returns document at index.
    [[nodiscard]] DicomDocument &at(std::size_t index);
    /// Returns document count, including initial empty document.
    [[nodiscard]] std::size_t size() const;
    /// Returns true when workspace contains at least one file-backed dataset.
    [[nodiscard]] bool hasLoadedFiles() const;
    /// Returns active document index.
    [[nodiscard]] std::size_t activeIndex() const;
    /// Activates document when index valid and different.
    [[nodiscard]] bool activate(std::size_t index);
    /// Activates previous document when available.
    [[nodiscard]] bool activatePrevious();
    /// Activates next document when available.
    [[nodiscard]] bool activateNext();
    /// Projects workspace state for file-tree views.
    [[nodiscard]] std::vector<OpenDicomFile> files() const;

  private:
    std::vector<DicomDocument> documents_;
    std::size_t activeIndex_{};
};

} // namespace dicom_editor
