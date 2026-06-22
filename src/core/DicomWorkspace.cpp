#include "dicom_editor/core/DicomWorkspace.hpp"

#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomEditorService.hpp"
#include "dicom_editor/core/DicomError.hpp"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdicdir.h>
#include <dcmtk/dcmdata/dcdirrec.h>
#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/ofstd/ofcond.h>
#include <dcmtk/ofstd/offile.h>
#include <dcmtk/ofstd/ofstring.h>

#include <algorithm>
#include <compare>
#include <expected>
#include <filesystem>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace dicom_editor {

namespace {

std::filesystem::path normalizedPath(const std::filesystem::path &path) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);
    return error ? std::filesystem::absolute(path, error).lexically_normal() : normalized;
}

void collectReferencedFiles(DcmDirectoryRecord &parent, const std::filesystem::path &base, std::vector<std::filesystem::path> &paths) {
    for (unsigned long index = 0; index < parent.cardSub(); ++index) {
        auto *record = parent.getSub(index);
        if (record == nullptr) {
            continue;
        }
        OFString referencedFileId;
        if (record->findAndGetOFStringArray(DCM_ReferencedFileID, referencedFileId).good() && !referencedFileId.empty()) {
            std::string relative = referencedFileId;
#ifndef _WIN32
            std::ranges::replace(relative, '\\', '/');
#endif
            const std::filesystem::path candidate(relative);
            const bool unsafe = candidate.is_absolute() || std::ranges::any_of(candidate, [](const auto &part) { return part == ".."; });
            if (!unsafe) {
                paths.push_back(base / candidate);
            }
        }
        collectReferencedFiles(*record, base, paths);
    }
}

bool matches(const DicomHierarchy &hierarchy, const BatchEditTarget &target) {
    if (target.level == BatchEditLevel::Patient) {
        return target.id.empty() ? hierarchy.patientLabel == target.label : hierarchy.patientId == target.id;
    }
    return target.id.empty() ? hierarchy.studyLabel == target.label : hierarchy.studyId == target.id;
}

std::vector<std::pair<DcmTagKey, std::string>> attributesFor(BatchEditLevel level) {
    if (level == BatchEditLevel::Patient) {
        return {{DCM_PatientName, "Patient Name"},
                {DCM_PatientID, "Patient ID"},
                {DCM_PatientBirthDate, "Birth Date"},
                {DCM_PatientSex, "Sex"}};
    }
    return {{DCM_StudyInstanceUID, "Study Instance UID"},
            {DCM_StudyDate, "Study Date"},
            {DCM_StudyTime, "Study Time"},
            {DCM_AccessionNumber, "Accession Number"},
            {DCM_ReferringPhysicianName, "Referring Physician"},
            {DCM_StudyDescription, "Study Description"}};
}

bool tagAllowed(BatchEditLevel level, const DcmTagKey &tag) {
    return std::ranges::any_of(attributesFor(level), [&tag](const auto &attribute) { return attribute.first == tag; });
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

std::expected<std::vector<std::filesystem::path>, DicomError> DicomWorkspace::discoverDicomDirectory(const std::filesystem::path &path) {
    DcmDicomDir directory(path.string().c_str());
    if (directory.error().bad()) {
        return std::unexpected(DicomError(std::format("Open DICOMDIR: {}", directory.error().text())));
    }
    std::vector<std::filesystem::path> paths;
    collectReferencedFiles(directory.getRootRecord(), path.parent_path(), paths);
    std::ranges::sort(paths);
    const auto duplicates = std::ranges::unique(paths);
    paths.erase(duplicates.begin(), duplicates.end());
    return paths;
}

DicomDocument &DicomWorkspace::active() { return documents_[activeIndex_]; }

const DicomDocument &DicomWorkspace::active() const { return documents_[activeIndex_]; }

DicomDocument &DicomWorkspace::at(std::size_t index) { return documents_.at(index); }

std::size_t DicomWorkspace::size() const { return documents_.size(); }

bool DicomWorkspace::hasLoadedFiles() const {
    return std::ranges::any_of(documents_, [](const DicomDocument &document) { return document.hasFilePath(); });
}

bool DicomWorkspace::hasDirtyDocuments() const {
    return std::ranges::any_of(documents_, [](const DicomDocument &document) { return document.dirty(); });
}

std::size_t DicomWorkspace::dirtyDocumentCount() const {
    return static_cast<std::size_t>(std::ranges::count_if(documents_, [](const DicomDocument &document) { return document.dirty(); }));
}

std::size_t DicomWorkspace::activeIndex() const { return activeIndex_; }

bool DicomWorkspace::activate(std::size_t index) {
    if (index >= documents_.size() || index == activeIndex_) {
        return false;
    }
    activeIndex_ = index;
    return true;
}

bool DicomWorkspace::activatePrevious(FileSortOrder order) {
    const auto ordered = files(order);
    const auto current = std::ranges::find_if(ordered, [](const OpenDicomFile &file) { return file.active; });
    return current != ordered.end() && current != ordered.begin() && activate(std::prev(current)->index);
}

bool DicomWorkspace::activateNext(FileSortOrder order) {
    const auto ordered = files(order);
    const auto current = std::ranges::find_if(ordered, [](const OpenDicomFile &file) { return file.active; });
    return current != ordered.end() && std::next(current) != ordered.end() && activate(std::next(current)->index);
}

void DicomWorkspace::clear() {
    documents_.clear();
    documents_.emplace_back();
    activeIndex_ = 0;
}

std::vector<OpenDicomFile> DicomWorkspace::files(FileSortOrder order) const {
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
    std::ranges::stable_sort(result, [order](const OpenDicomFile &left, const OpenDicomFile &right) {
        const auto &a = left.hierarchy;
        const auto &b = right.hierarchy;
        const auto groupsA = std::tie(a.patientLabel, a.patientId, a.studyLabel, a.studyId, a.seriesLabel, a.seriesId);
        const auto groupsB = std::tie(b.patientLabel, b.patientId, b.studyLabel, b.studyId, b.seriesLabel, b.seriesId);
        if (groupsA != groupsB) {
            return groupsA < groupsB;
        }
        if (order == FileSortOrder::Filename) {
            return left.path.filename().string() < right.path.filename().string();
        }
        const long instanceA = a.instanceNumber.value_or(std::numeric_limits<long>::max());
        const long instanceB = b.instanceNumber.value_or(std::numeric_limits<long>::max());
        return instanceA == instanceB ? left.path.filename().string() < right.path.filename().string() : instanceA < instanceB;
    });
    return result;
}

BatchEditReport DicomWorkspace::batchEditReport(const BatchEditTarget &target) const {
    BatchEditReport report{.target = target, .documentCount = 0, .attributes = {}};
    const auto attributes = attributesFor(target.level);
    report.attributes.reserve(attributes.size());
    for (const auto &[tag, name] : attributes) {
        report.attributes.push_back({.tag = tag, .name = name, .values = {}});
    }
    for (const auto &document : documents_) {
        if (!document.hasFilePath() || !matches(document.hierarchy(), target)) {
            continue;
        }
        ++report.documentCount;
        for (auto &attribute : report.attributes) {
            const auto value = document.attributeValue(attribute.tag).value_or("<missing>");
            if (std::ranges::find(attribute.values, value) == attribute.values.end()) {
                attribute.values.push_back(value);
            }
        }
    }
    return report;
}

std::size_t DicomWorkspace::batchEdit(const BatchEditTarget &target, const DcmTagKey &tag, const std::string &value, bool validate) {
    if (!tagAllowed(target.level, tag)) {
        throw DicomError("Attribute is not valid for selected batch-edit level");
    }
    std::vector<DicomDocument *> selected;
    for (auto &document : documents_) {
        if (document.hasFilePath() && matches(document.hierarchy(), target)) {
            selected.push_back(&document);
        }
    }
    if (selected.empty()) {
        throw DicomError("No open datasets match selected batch-edit group");
    }
    for (auto *document : selected) {
        DicomEditorService::setAttribute(*document, tag, value, validate);
    }
    return selected.size();
}

} // namespace dicom_editor
