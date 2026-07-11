#pragma once

#include "dicom_editor/core/DicomNode.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace dicom_editor {

class DicomPath;

/// Prepares the flattened dataset tree for the table view.
class DatasetViewModel {
  public:
    /// Replaces the node list and reapplies the current filter.
    void setNodes(std::vector<DicomNode> newNodes);
    /// Updates the visible rows using a case-insensitive substring filter.
    void setFilter(std::string filter);

    /// Returns the node shown at a visible row index.
    [[nodiscard]] const DicomNode *nodeAt(std::size_t visibleIndex) const;
    /// Returns all nodes in tree order.
    [[nodiscard]] std::span<const DicomNode> nodes() const;
    /// Returns indexes of rows currently visible after filtering.
    [[nodiscard]] std::span<const std::size_t> visibleIndices() const;
    /// Expands or collapses a sequence row.
    void toggleSequence(const DicomPath &path);
    /// Returns whether a sequence row is collapsed.
    [[nodiscard]] bool sequenceCollapsed(const DicomPath &path) const;

    /// Builds the indented label shown in the attribute column.
    [[nodiscard]] static std::string attributeLabel(const DicomNode &node);
    /// Returns a label for the node kind.
    [[nodiscard]] static std::string_view kindLabel(DicomNodeKind kind);

  private:
    void rebuild();

    std::vector<DicomNode> nodes_;
    std::vector<std::size_t> visibleIndices_;
    std::string filter_;
    std::unordered_set<std::string> collapsedSequences_;
};

} // namespace dicom_editor
