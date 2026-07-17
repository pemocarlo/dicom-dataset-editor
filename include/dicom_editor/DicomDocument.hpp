#pragma once

#include "dicom_editor/DicomNode.hpp"

#include <dcmtk/dcmdata/dcfilefo.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class DcmDataset;
class DcmElement;
class DcmItem;

namespace dicom_editor {

class DicomPath;

/// Result of rendering DICOM pixel data for the preview pane.
struct PixelDataPreview {
    /// Rendered pixel bytes in row-major order.
    std::vector<std::uint8_t> pixels;
    /// Explanation shown when rendering is not possible.
    std::string message;
    /// Rendered image width in pixels.
    unsigned int width{};
    /// Rendered image height in pixels.
    unsigned int height{};
    /// Number of interleaved channels in `pixels`.
    int channels{};
    /// Zero-based frame index that was requested.
    unsigned long frameIndex{};
    /// Total frame count reported by the source image.
    unsigned long frameCount{};
};

/// Owns the loaded DICOM file and exposes tree, edit, and preview operations.
class DicomDocument {
  public:
    /// Creates an empty in-memory dataset.
    DicomDocument();

    /// Replaces the current content with a new empty dataset.
    void createEmpty();
    /// Loads a DICOM file and clears the dirty flag.
    void load(const std::filesystem::path &path);
    /// Saves to the current file path.
    void save();
    /// Saves to a new file path and adopts it as the active path.
    void saveAs(const std::filesystem::path &path);

    /// Returns the root dataset.
    [[nodiscard]] DcmDataset &dataset();
    /// Returns the root dataset.
    [[nodiscard]] const DcmDataset &dataset() const;
    /// Resolves a path to the owning item.
    [[nodiscard]] DcmItem &itemAt(const DicomPath &path);
    /// Resolves a path to the owning item.
    [[nodiscard]] const DcmItem &itemAt(const DicomPath &path) const;
    /// Resolves a path to the target element.
    [[nodiscard]] DcmElement &elementAt(const DicomPath &path);
    /// Resolves a path to the target element.
    [[nodiscard]] const DcmElement &elementAt(const DicomPath &path) const;
    /// Flattens the dataset into UI rows.
    [[nodiscard]] std::vector<DicomNode> nodes(bool validateValues = false) const;
    /// Renders the requested pixel frame, if available.
    [[nodiscard]] PixelDataPreview renderPixelData(unsigned long frameIndex) const;
    /// Returns the active file path.
    [[nodiscard]] const std::filesystem::path &filePath() const;
    /// Returns `true` when the document has an active file path.
    [[nodiscard]] bool hasFilePath() const;
    /// Returns `true` when the dataset has unsaved changes.
    [[nodiscard]] bool dirty() const;

    /// Marks the dataset as modified.
    void markDirty();
    /// Clears the modified state.
    void clearDirty();

  private:
    std::unique_ptr<DcmFileFormat> file_;
    std::filesystem::path filePath_;
    bool dirty_{};
};

} // namespace dicom_editor
