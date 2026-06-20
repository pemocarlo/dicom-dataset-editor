#pragma once

#include <dcmtk/dcmdata/dctagkey.h>

#include <optional>
#include <string>
#include <vector>

namespace dicom_editor {

/// References one item inside a parent sequence.
struct SequenceItemRef {
    /// Sequence tag that owns the item.
    DcmTagKey sequenceTag;
    /// Zero-based item index within the sequence.
    unsigned long itemIndex{};

    /// Compares sequence item references.
    [[nodiscard]] bool operator==(const SequenceItemRef &other) const = default;
};

/// Stable path to a dataset item or element.
class DicomPath {
  public:
    /// Creates the root dataset path.
    DicomPath() = default;

    /// Returns the root dataset path.
    static DicomPath dataset();
    /// Builds a path to an element inside nested sequence items.
    static DicomPath element(std::vector<SequenceItemRef> parents, const DcmTagKey &tag);
    /// Builds a path to a sequence item.
    static DicomPath item(std::vector<SequenceItemRef> parents);

    /// Returns the parent sequence chain.
    [[nodiscard]] const std::vector<SequenceItemRef> &parents() const;
    /// Returns the element tag when the path targets an attribute.
    [[nodiscard]] const std::optional<DcmTagKey> &elementTag() const;
    /// Returns `true` when the path targets a dataset item.
    [[nodiscard]] bool pointsToDatasetItem() const;
    /// Returns `true` when the path targets an element.
    [[nodiscard]] bool pointsToElement() const;
    /// Formats the path for diagnostics.
    [[nodiscard]] std::string toString() const;

  private:
    std::vector<SequenceItemRef> parents_;
    std::optional<DcmTagKey> elementTag_;
};

} // namespace dicom_editor
