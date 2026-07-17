#include "dicom_editor/DicomWorkspace.hpp"

#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomError.hpp"

#include <algorithm>
#include <expected>
#include <filesystem>
#include <iterator>
#include <ranges>
#include <system_error>
#include <utility>
#include <vector>

namespace dicom_editor {

namespace {

std::filesystem::path normalizedPath(const std::filesystem::path &path) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);
    return error ? std::filesystem::absolute(path, error).lexically_normal() : normalized;
}

} // namespace

DicomWorkspace::DicomWorkspace() { documents_.emplace_back(); }

OpenFilesResult DicomWorkspace::open(const std::vector<std::filesystem::path> &paths) {
    OpenFilesResult summary;
    for (const auto &path : paths) {
        const auto normalized = normalizedPath(path);
        const auto duplicate = std::ranges::find_if(documents_, [&normalized](const DicomDocument &candidate) {
            return candidate.hasFilePath() && normalizedPath(candidate.filePath()) == normalized;
        });
        if (duplicate != documents_.end()) {
            activeIndex_ = static_cast<std::size_t>(std::distance(documents_.begin(), duplicate));
            ++summary.duplicates;
            continue;
        }

        DicomDocument loaded;
        const auto result = loaded.load(path);
        if (!result) {
            summary.failures.push_back({.path = path, .message = result.error().what()});
            continue;
        }
        if (loaded.isDicomDirectory()) {
            ++summary.dicomDirectories;
            continue;
        }

        if (documents_.size() == 1 && !active().hasFilePath() && !active().dirty()) {
            documents_.clear();
        }
        documents_.push_back(std::move(loaded));
        activeIndex_ = documents_.size() - 1;
        ++summary.opened;
    }
    return summary;
}

std::vector<std::filesystem::path> DicomWorkspace::discoverFiles(const std::filesystem::path &folder) {
    std::vector<std::filesystem::path> paths;
    std::error_code error;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iterator(folder, options, error), end; iterator != end; iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (iterator->is_regular_file(error) && !error) {
            paths.push_back(iterator->path());
        }
        error.clear();
    }
    std::ranges::sort(paths);
    return paths;
}

DicomDocument &DicomWorkspace::active() { return documents_[activeIndex_]; }

const DicomDocument &DicomWorkspace::active() const { return documents_[activeIndex_]; }

DicomDocument &DicomWorkspace::at(std::size_t index) { return documents_.at(index); }

std::size_t DicomWorkspace::size() const { return documents_.size(); }

bool DicomWorkspace::hasLoadedFiles() const {
    return std::ranges::any_of(documents_, [](const DicomDocument &document) { return document.hasFilePath(); });
}

std::size_t DicomWorkspace::activeIndex() const { return activeIndex_; }

bool DicomWorkspace::activate(std::size_t index) {
    if (index >= documents_.size() || index == activeIndex_) {
        return false;
    }
    activeIndex_ = index;
    return true;
}

bool DicomWorkspace::activatePrevious() { return activeIndex_ > 0 && activate(activeIndex_ - 1); }

bool DicomWorkspace::activateNext() { return activeIndex_ + 1 < documents_.size() && activate(activeIndex_ + 1); }

std::vector<OpenDicomFile> DicomWorkspace::files() const {
    std::vector<OpenDicomFile> result;
    result.reserve(documents_.size());
    for (std::size_t index = 0; index < documents_.size(); ++index) {
        const auto &entry = documents_[index];
        result.push_back({.index = index,
                          .path = entry.filePath(),
                          .hierarchy = entry.hierarchy(),
                          .dirty = entry.dirty(),
                          .active = index == activeIndex_});
    }
    return result;
}

} // namespace dicom_editor
