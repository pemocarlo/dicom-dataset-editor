#pragma once

#include "dicom_editor/core/DicomError.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace dicom_editor {

/// Result of replacing the active DCMTK data dictionary.
struct DicomDictionaryInfo {
    /// Human-readable dictionary source.
    std::string source;
    /// Number of loaded entries, excluding DCMTK skeleton entries.
    int entryCount{};
};

/// Loads the dictionary embedded in the application once per process.
void ensureEmbeddedDicomDictionary();

/// Validates and replaces the active dictionary with a DCMTK-format file.
[[nodiscard]] std::expected<DicomDictionaryInfo, DicomError> loadDicomDictionary(const std::filesystem::path &path);

/// Describes the dictionary currently used by DCMTK.
[[nodiscard]] std::string dicomDictionarySource();

} // namespace dicom_editor
