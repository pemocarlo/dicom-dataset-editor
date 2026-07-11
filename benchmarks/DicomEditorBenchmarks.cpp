#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <ofstd/oftypes.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() /
                std::format("dicom_editor_benchmark_{}", Clock::now().time_since_epoch().count());
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    [[nodiscard]] const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

std::size_t parseCount(int argc, char **argv) {
    if (argc < 2) {
        return 100;
    }
    const auto value = std::stoull(argv[1]);
    if (value == 0) {
        throw std::invalid_argument("file count must be positive");
    }
    return static_cast<std::size_t>(value);
}

void createSyntheticFile(const std::filesystem::path &path, std::size_t index) {
    dicom_editor::DicomDocument document;
    auto &dataset = document.dataset();
    const auto suffix = std::to_string(index + 1);
    dataset.putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
    dataset.putAndInsertString(DCM_SOPInstanceUID, std::format("1.2.826.0.1.3680043.10.543.1.{}", suffix).c_str());
    dataset.putAndInsertString(DCM_StudyInstanceUID, "1.2.826.0.1.3680043.10.543.2");
    dataset.putAndInsertString(DCM_SeriesInstanceUID, "1.2.826.0.1.3680043.10.543.3");
    dataset.putAndInsertString(DCM_PatientName, "Benchmark^Patient");
    dataset.putAndInsertString(DCM_PatientID, "BENCHMARK");
    dataset.putAndInsertString(DCM_InstanceNumber, suffix.c_str());
    dataset.putAndInsertUint16(DCM_Rows, 512);
    dataset.putAndInsertUint16(DCM_Columns, 512);
    dataset.putAndInsertUint16(DCM_SamplesPerPixel, 1);
    dataset.putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
    dataset.putAndInsertUint16(DCM_BitsAllocated, 8);
    dataset.putAndInsertUint16(DCM_BitsStored, 8);
    dataset.putAndInsertUint16(DCM_HighBit, 7);
    dataset.putAndInsertUint16(DCM_PixelRepresentation, 0);

    auto *sequence = new DcmSequenceOfItems(DCM_ReferencedImageSequence);
    for (std::size_t itemIndex = 0; itemIndex < 64; ++itemIndex) {
        auto *item = new DcmItem();
        item->putAndInsertString(DCM_ReferencedSOPClassUID, UID_SecondaryCaptureImageStorage);
        item->putAndInsertString(DCM_ReferencedSOPInstanceUID,
                                 std::format("1.2.826.0.1.3680043.10.543.9.{}.{}", suffix, itemIndex + 1).c_str());
        sequence->append(item);
    }
    dataset.insert(sequence, true);

    std::vector<Uint8> pixels(512U * 512U, static_cast<Uint8>(index % 256U));
    dataset.putAndInsertUint8Array(DCM_PixelData, pixels.data(), static_cast<unsigned long>(pixels.size()));
    const auto saved = document.saveAs(path);
    if (!saved) {
        throw saved.error();
    }
}

template <typename Function> double milliseconds(Function &&function) {
    const auto start = Clock::now();
    function();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

} // namespace

int main(int argc, char **argv) {
    try {
        const auto fileCount = parseCount(argc, argv);
        TemporaryDirectory directory;
        std::vector<std::filesystem::path> paths;
        paths.reserve(fileCount);
        const auto generationMs = milliseconds([&] {
            for (std::size_t index = 0; index < fileCount; ++index) {
                paths.push_back(directory.path() / std::format("image_{:05}.dcm", index));
                createSyntheticFile(paths.back(), index);
            }
        });

        dicom_editor::DicomWorkspace workspace;
        const auto openMs = milliseconds([&] {
            const auto opened = workspace.open(paths);
            if (opened.opened != fileCount) {
                throw std::runtime_error("synthetic workspace did not open completely");
            }
        });
        constexpr std::size_t ProjectionIterations = 200;
        std::size_t projectedFiles{};
        const auto projectionMs = milliseconds([&] {
            for (std::size_t iteration = 0; iteration < ProjectionIterations; ++iteration) {
                projectedFiles += workspace.files().size();
            }
        });
        constexpr std::size_t DatasetIterations = 25;
        std::size_t projectedNodes{};
        const auto datasetMs = milliseconds([&] {
            for (std::size_t iteration = 0; iteration < DatasetIterations; ++iteration) {
                projectedNodes += workspace.active().nodes(false).size();
            }
        });

        std::println("Synthetic DICOM benchmark: {} files, 256 KiB Pixel Data and 64 sequence items each", fileCount);
        std::println("generate: {:.2f} ms", generationMs);
        std::println("open: {:.2f} ms ({:.2f} ms/file)", openMs, openMs / static_cast<double>(fileCount));
        std::println("file-tree projection: {:.2f} ms ({} iterations, {} projected files)", projectionMs, ProjectionIterations,
                     projectedFiles);
        std::println("active dataset projection: {:.2f} ms ({} iterations, {} projected nodes)", datasetMs, DatasetIterations,
                     projectedNodes);
        return 0;
    } catch (const std::exception &error) {
        std::println(stderr, "benchmark failed: {}", error.what());
        return 1;
    }
}
