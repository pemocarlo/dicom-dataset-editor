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

struct PixelDataPreview {
    std::vector<std::uint8_t> pixels;
    std::string message;
    unsigned int width{};
    unsigned int height{};
    int channels{};
    unsigned long frameIndex{};
    unsigned long frameCount{};
};

class DicomDocument {
  public:
    DicomDocument();

    void createEmpty();
    void load(const std::filesystem::path &path);
    void save();
    void saveAs(const std::filesystem::path &path);

    [[nodiscard]] DcmDataset &dataset();
    [[nodiscard]] const DcmDataset &dataset() const;
    [[nodiscard]] DcmItem &itemAt(const DicomPath &path);
    [[nodiscard]] const DcmItem &itemAt(const DicomPath &path) const;
    [[nodiscard]] DcmElement &elementAt(const DicomPath &path);
    [[nodiscard]] const DcmElement &elementAt(const DicomPath &path) const;
    [[nodiscard]] std::vector<DicomNode> nodes(bool validateValues = false) const;
    [[nodiscard]] PixelDataPreview renderPixelData(unsigned long frameIndex) const;
    [[nodiscard]] const std::filesystem::path &filePath() const;
    [[nodiscard]] bool hasFilePath() const;
    [[nodiscard]] bool dirty() const;

    void markDirty();
    void clearDirty();

  private:
    std::unique_ptr<DcmFileFormat> file_;
    std::filesystem::path filePath_;
    bool dirty_{};
};

} // namespace dicom_editor
