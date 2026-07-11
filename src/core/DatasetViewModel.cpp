#include "dicom_editor/core/DatasetViewModel.hpp"

#include "dicom_editor/core/DicomPath.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace dicom_editor {

namespace {

std::string lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool containsCaseInsensitive(const std::string &haystack, const std::string &needle) { return lower(haystack).contains(lower(needle)); }

} // namespace

void DatasetViewModel::setNodes(std::vector<DicomNode> newNodes) {
    nodes_ = std::move(newNodes);
    rebuild();
}

void DatasetViewModel::setFilter(std::string filter) {
    filter_ = std::move(filter);
    rebuild();
}

const DicomNode *DatasetViewModel::nodeAt(std::size_t visibleIndex) const {
    return visibleIndex < visibleIndices_.size() ? &nodes_[visibleIndices_[visibleIndex]] : nullptr;
}

std::span<const DicomNode> DatasetViewModel::nodes() const { return nodes_; }

std::span<const std::size_t> DatasetViewModel::visibleIndices() const { return visibleIndices_; }

void DatasetViewModel::toggleSequence(const DicomPath &path) {
    const auto key = path.toString();
    if (const auto found = collapsedSequences_.find(key); found != collapsedSequences_.end()) {
        collapsedSequences_.erase(found);
    } else {
        collapsedSequences_.insert(key);
    }
    rebuild();
}

bool DatasetViewModel::sequenceCollapsed(const DicomPath &path) const { return collapsedSequences_.contains(path.toString()); }

std::string DatasetViewModel::attributeLabel(const DicomNode &node) {
    std::string label(static_cast<std::size_t>(node.depth) * 2, ' ');
    label += node.keyword.empty() ? node.tag : node.keyword;
    return label;
}

std::string_view DatasetViewModel::kindLabel(DicomNodeKind kind) {
    using enum DicomNodeKind;
    switch (kind) {
    case Dataset:
        return "Dataset";
    case Element:
        return "Element";
    case Sequence:
        return "Sequence";
    case Item:
        return "Item";
    default:
        std::unreachable();
    }
}

void DatasetViewModel::rebuild() {
    visibleIndices_.clear();
    std::optional<unsigned int> collapsedDepth;
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        const auto &node = nodes_[index];
        if (filter_.empty() && collapsedDepth && node.depth > *collapsedDepth) {
            continue;
        }
        if (collapsedDepth && node.depth <= *collapsedDepth) {
            collapsedDepth.reset();
        }
        const std::string searchable = node.tag + " " + node.keyword + " " + node.vr + " " + node.valuePreview + " " + node.path.toString();
        if (filter_.empty() || containsCaseInsensitive(searchable, filter_)) {
            visibleIndices_.push_back(index);
        }
        if (filter_.empty() && node.kind == DicomNodeKind::Sequence && sequenceCollapsed(node.path)) {
            collapsedDepth = node.depth;
        }
    }
}

} // namespace dicom_editor
