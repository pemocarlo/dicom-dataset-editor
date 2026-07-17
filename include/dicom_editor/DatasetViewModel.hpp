#pragma once

#include "dicom_editor/DicomNode.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dicom_editor {

class DatasetViewModel {
  public:
    void setNodes(std::vector<DicomNode> newNodes);
    void setFilter(std::string filter);

    [[nodiscard]] const DicomNode *nodeAt(std::size_t visibleIndex) const;
    [[nodiscard]] std::span<const DicomNode> nodes() const;
    [[nodiscard]] std::span<const std::size_t> visibleIndices() const;

    [[nodiscard]] static std::string attributeLabel(const DicomNode &node);
    [[nodiscard]] static std::string_view kindLabel(DicomNodeKind kind);

  private:
    void rebuild();

    std::vector<DicomNode> nodes_;
    std::vector<std::size_t> visibleIndices_;
    std::string filter_;
};

} // namespace dicom_editor
