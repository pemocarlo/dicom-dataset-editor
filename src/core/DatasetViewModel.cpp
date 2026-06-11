#include "dicom_editor/DatasetViewModel.hpp"

#include "dicom_editor/DicomPath.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace dicom_editor {

namespace {

std::string lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool containsCaseInsensitive(const std::string &haystack, const std::string &needle) {
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

} // namespace

void DatasetViewModel::setNodes(std::vector<DicomNode> nodes) {
    nodes_ = std::move(nodes);
    rebuild();
}

void DatasetViewModel::setFilter(std::string filter) {
    filter_ = std::move(filter);
    rebuild();
}

const DicomNode *DatasetViewModel::nodeAt(std::size_t visibleIndex) const {
    return visibleIndex < visibleIndices_.size() ? &nodes_[visibleIndices_[visibleIndex]] : nullptr;
}

const std::vector<DicomNode> &DatasetViewModel::nodes() const { return nodes_; }

const std::vector<std::size_t> &DatasetViewModel::visibleIndices() const { return visibleIndices_; }

std::string DatasetViewModel::attributeLabel(const DicomNode &node) {
    std::string label(static_cast<std::size_t>(node.depth) * 2, ' ');
    label += node.keyword.empty() ? node.tag : node.keyword;
    return label;
}

std::string DatasetViewModel::kindLabel(DicomNodeKind kind) {
    switch (kind) {
    case DicomNodeKind::Dataset:
        return "Dataset";
    case DicomNodeKind::Element:
        return "Element";
    case DicomNodeKind::Sequence:
        return "Sequence";
    case DicomNodeKind::Item:
        return "Item";
    }
    return "";
}

void DatasetViewModel::rebuild() {
    visibleIndices_.clear();
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        const auto &node = nodes_[index];
        const std::string searchable = node.tag + " " + node.keyword + " " + node.vr + " " + node.valuePreview + " " + node.path.toString();
        if (filter_.empty() || containsCaseInsensitive(searchable, filter_)) {
            visibleIndices_.push_back(index);
        }
    }
}

} // namespace dicom_editor
