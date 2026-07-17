#include "dicom_editor/DicomPath.hpp"

#include <iomanip>
#include <sstream>

namespace dicom_editor {

namespace {

std::string tagToString(const DcmTagKey& tag)
{
    std::ostringstream out;
    out << '(' << std::hex << std::setw(4) << std::setfill('0') << tag.getGroup() << ','
        << std::setw(4) << tag.getElement() << ')';
    return out.str();
}

} // namespace

bool SequenceItemRef::operator==(const SequenceItemRef& other) const
{
    return sequenceTag == other.sequenceTag && itemIndex == other.itemIndex;
}

DicomPath DicomPath::dataset()
{
    return {};
}

DicomPath DicomPath::element(std::vector<SequenceItemRef> parents, DcmTagKey tag)
{
    DicomPath path;
    path.parents_ = std::move(parents);
    path.elementTag_ = tag;
    return path;
}

DicomPath DicomPath::item(std::vector<SequenceItemRef> parents)
{
    DicomPath path;
    path.parents_ = std::move(parents);
    return path;
}

const std::vector<SequenceItemRef>& DicomPath::parents() const
{
    return parents_;
}

const std::optional<DcmTagKey>& DicomPath::elementTag() const
{
    return elementTag_;
}

bool DicomPath::pointsToDatasetItem() const
{
    return !elementTag_.has_value();
}

bool DicomPath::pointsToElement() const
{
    return elementTag_.has_value();
}

std::string DicomPath::toString() const
{
    std::ostringstream out;
    out << "/";
    for (const auto& parent : parents_) {
        out << tagToString(parent.sequenceTag) << "/Item[" << parent.itemIndex << "]/";
    }
    if (elementTag_.has_value()) {
        out << tagToString(*elementTag_);
    }
    return out.str();
}

} // namespace dicom_editor
