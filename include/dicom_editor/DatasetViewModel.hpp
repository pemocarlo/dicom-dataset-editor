#pragma once

#include "dicom_editor/DicomNode.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace dicom_editor {

class DatasetViewModel {
  public:
    void setNodes(std::vector<DicomNode> newNodes);
    void setFilter(std::string filter);

    [[nodiscard]] const DicomNode *nodeAt(std::size_t visibleIndex) const;
    [[nodiscard]] const std::vector<DicomNode> &nodes() const;
    [[nodiscard]] const std::vector<std::size_t> &visibleIndices() const;

    [[nodiscard]] static std::string attributeLabel(const DicomNode &node);
    [[nodiscard]] static std::string kindLabel(DicomNodeKind kind);

  private:
    void rebuild();

    std::vector<DicomNode> nodes_;
    std::vector<std::size_t> visibleIndices_;
    std::string filter_;
};

} // namespace dicom_editor
