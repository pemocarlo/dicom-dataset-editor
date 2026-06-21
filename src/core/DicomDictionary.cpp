#include "dicom_editor/core/DicomDictionary.hpp"

#include "EmbeddedDictionary.hpp"

#include <dcmtk/dcmdata/dcdict.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace dicom_editor {

namespace {

std::mutex sourceMutex;
std::string activeSource = "embedded DCMTK dictionary";

class DictionaryWriteLock final {
  public:
    DictionaryWriteLock() : dictionary_(dcmDataDict.wrlock()) {}
    ~DictionaryWriteLock() { dcmDataDict.wrunlock(); }

    DictionaryWriteLock(const DictionaryWriteLock &) = delete;
    DictionaryWriteLock &operator=(const DictionaryWriteLock &) = delete;

    [[nodiscard]] DcmDataDictionary &dictionary() { return dictionary_; }

  private:
    DcmDataDictionary &dictionary_;
};

class TemporaryFile final {
  public:
    explicit TemporaryFile(std::filesystem::path path) : path_(std::move(path)) {}
    ~TemporaryFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    TemporaryFile(const TemporaryFile &) = delete;
    TemporaryFile &operator=(const TemporaryFile &) = delete;

    [[nodiscard]] const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

std::filesystem::path temporaryDictionaryPath() {
    static std::atomic<std::uint64_t> sequence{};
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           std::format("dicom-dataset-editor-{}-{}.dic", timestamp, sequence.fetch_add(1, std::memory_order_relaxed));
}

std::expected<DicomDictionaryInfo, DicomError> replaceDictionary(const std::filesystem::path &path, std::string source) {
    if (!std::filesystem::is_regular_file(path)) {
        return std::unexpected(DicomError("Dictionary is not a readable file: " + path.string()));
    }

    DcmDataDictionary candidate(OFFalse, OFFalse);
    if (candidate.loadDictionary(path.string().c_str()) == OFFalse) {
        return std::unexpected(DicomError("DCMTK rejected dictionary file: " + path.string()));
    }

    int entryCount{};
    {
        DictionaryWriteLock lock;
        auto &dictionary = lock.dictionary();
        static_cast<void>(dictionary.reloadDictionaries(OFFalse, OFFalse));
        if (dictionary.loadDictionary(path.string().c_str()) == OFFalse) {
            return std::unexpected(DicomError("DCMTK could not activate dictionary file: " + path.string()));
        }
        entryCount = dictionary.numberOfEntries();
    }

    {
        const std::scoped_lock lock(sourceMutex);
        activeSource = source;
    }
    return DicomDictionaryInfo{.source = std::move(source), .entryCount = entryCount};
}

std::expected<DicomDictionaryInfo, DicomError> loadEmbeddedDictionary() {
    const TemporaryFile temporary(temporaryDictionaryPath());
    const std::string_view contents = embeddedDicomDictionary();
    std::ofstream output(temporary.path(), std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    if (!output) {
        return std::unexpected(DicomError("Could not extract embedded DCMTK dictionary."));
    }
    return replaceDictionary(temporary.path(), "embedded DCMTK dictionary");
}

} // namespace

void ensureEmbeddedDicomDictionary() {
    static std::once_flag once;
    std::call_once(once, [] {
        const auto result = loadEmbeddedDictionary();
        if (!result) {
            throw result.error();
        }
    });
}

std::expected<DicomDictionaryInfo, DicomError> loadDicomDictionary(const std::filesystem::path &path) {
    ensureEmbeddedDicomDictionary();
    return replaceDictionary(path, path.string());
}

std::string dicomDictionarySource() {
    const std::scoped_lock lock(sourceMutex);
    return activeSource;
}

} // namespace dicom_editor
