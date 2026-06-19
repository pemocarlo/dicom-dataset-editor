#include "dicom_editor/DicomPath.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <format>
#include <iterator>
#include <string>
#include <utility>

namespace dicom_editor {

namespace {

std::string tagToString(const DcmTagKey &tag) { return std::format("({:04x},{:04x})", tag.getGroup(), tag.getElement()); }

} // namespace

DicomPath DicomPath::dataset() { return {}; }

DicomPath DicomPath::element(std::vector<SequenceItemRef> parents, const DcmTagKey &tag) {
    DicomPath path;
    path.parents_ = std::move(parents);
    path.elementTag_ = tag;
    return path;
}

DicomPath DicomPath::item(std::vector<SequenceItemRef> parents) {
    DicomPath path;
    path.parents_ = std::move(parents);
    return path;
}

const std::vector<SequenceItemRef> &DicomPath::parents() const { return parents_; }

const std::optional<DcmTagKey> &DicomPath::elementTag() const { return elementTag_; }

bool DicomPath::pointsToDatasetItem() const { return !elementTag_.has_value(); }

bool DicomPath::pointsToElement() const { return elementTag_.has_value(); }

std::string DicomPath::toString() const {
    std::string result = "/";
    for (const auto &parent : parents_) {
        std::format_to(std::back_inserter(result), "{}/Item[{}]/", tagToString(parent.sequenceTag), parent.itemIndex);
    }
    if (elementTag_) {
        result += tagToString(*elementTag_);
    }
    return result;
}

} // namespace dicom_editor
