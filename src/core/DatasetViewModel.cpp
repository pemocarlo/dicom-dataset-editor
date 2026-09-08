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
    if (std::ranges::none_of(nodes_, [&path](const DicomNode &node) {
            return node.kind != DicomNodeKind::Element && node.path.toString() == path.toString();
        })) {
        return;
    }
    const auto key = path.toString();
    if (const auto found = collapsedSequences_.find(key); found != collapsedSequences_.end()) {
        collapsedSequences_.erase(found);
    } else {
        collapsedSequences_.insert(key);
    }
    rebuild();
}

bool DatasetViewModel::sequenceCollapsed(const DicomPath &path) const { return collapsedSequences_.contains(path.toString()); }

void DatasetViewModel::collapseSubtree(const DicomPath &path) {
    const auto key = path.toString();
    const auto start = std::ranges::find_if(nodes_, [&key](const DicomNode &node) { return node.path.toString() == key; });
    if (start == nodes_.end() || start->kind == DicomNodeKind::Element) {
        return;
    }

    for (auto node = start; node != nodes_.end(); ++node) {
        if (node != start && node->depth <= start->depth) {
            break;
        }
        if (node->kind != DicomNodeKind::Element) {
            collapsedSequences_.insert(node->path.toString());
        }
    }
    rebuild();
}

void DatasetViewModel::expandSubtree(const DicomPath &path) {
    const auto key = path.toString();
    const auto start = std::ranges::find_if(nodes_, [&key](const DicomNode &node) { return node.path.toString() == key; });
    if (start == nodes_.end() || start->kind == DicomNodeKind::Element) {
        return;
    }

    for (auto node = start; node != nodes_.end(); ++node) {
        if (node != start && node->depth <= start->depth) {
            break;
        }
        if (node->kind != DicomNodeKind::Element) {
            collapsedSequences_.erase(node->path.toString());
        }
    }
    rebuild();
}

std::optional<DicomPath> DatasetViewModel::containingSubtree(const DicomPath &path) const {
    const auto key = path.toString();
    const auto selected = std::ranges::find_if(nodes_, [&key](const DicomNode &node) { return node.path.toString() == key; });
    if (selected == nodes_.end()) {
        return std::nullopt;
    }

    auto branch = selected;
    if (branch->kind == DicomNodeKind::Element) {
        while (branch != nodes_.begin()) {
            --branch;
            if (branch->kind != DicomNodeKind::Element && branch->depth < selected->depth) {
                break;
            }
        }
        if (branch->kind == DicomNodeKind::Element) {
            return std::nullopt;
        }
    }
    return branch->path;
}

std::optional<DicomPath> DatasetViewModel::collapseContainingSubtree(const DicomPath &path) {
    const auto branch = containingSubtree(path);
    if (!branch) {
        return std::nullopt;
    }
    collapseSubtree(*branch);
    return branch;
}

void DatasetViewModel::collapseAll() {
    for (const auto &node : nodes_) {
        if (node.kind != DicomNodeKind::Element) {
            collapsedSequences_.insert(node.path.toString());
        }
    }
    rebuild();
}

void DatasetViewModel::showAll() {
    collapsedSequences_.clear();
    rebuild();
}

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
        if (filter_.empty() && node.kind != DicomNodeKind::Element && sequenceCollapsed(node.path)) {
            collapsedDepth = node.depth;
        }
    }
}

} // namespace dicom_editor
