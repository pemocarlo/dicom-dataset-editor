#pragma once

#include <dcmtk/dcmdata/dctagkey.h>

#include <optional>
#include <string>
#include <vector>

namespace dicom_editor {

struct SequenceItemRef {
    DcmTagKey sequenceTag;
    unsigned long itemIndex{};

    [[nodiscard]] bool operator==(const SequenceItemRef &other) const = default;
};

class DicomPath {
  public:
    DicomPath() = default;

    static DicomPath dataset();
    static DicomPath element(std::vector<SequenceItemRef> parents, const DcmTagKey &tag);
    static DicomPath item(std::vector<SequenceItemRef> parents);

    [[nodiscard]] const std::vector<SequenceItemRef> &parents() const;
    [[nodiscard]] const std::optional<DcmTagKey> &elementTag() const;
    [[nodiscard]] bool pointsToDatasetItem() const;
    [[nodiscard]] bool pointsToElement() const;
    [[nodiscard]] std::string toString() const;

  private:
    std::vector<SequenceItemRef> parents_;
    std::optional<DcmTagKey> elementTag_;
};

} // namespace dicom_editor
