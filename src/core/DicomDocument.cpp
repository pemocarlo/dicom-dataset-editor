#include "dicom_editor/DicomDocument.hpp"

#include "dicom_editor/DicomError.hpp"
#include "dicom_editor/RuntimePaths.hpp"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dcvr.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace dicom_editor {

namespace {

std::string conditionMessage(const OFCondition& condition)
{
    return condition.text() == nullptr ? "unknown DCMTK error" : condition.text();
}

void requireGood(const OFCondition& condition, const std::string& action)
{
    if (condition.bad()) {
        throw DicomError(action + ": " + conditionMessage(condition));
    }
}

void ensureDictionaryPath()
{
    const auto installedPath = installedDataPath("dcmtk/dicom.dic");
    const char* current = std::getenv("DCMDICTPATH");
    if ((current == nullptr || !std::filesystem::exists(current)) && std::filesystem::exists(installedPath)) {
        setenv("DCMDICTPATH", installedPath.string().c_str(), 1);
        return;
    }

#ifdef DICOM_EDITOR_DCMTK_DICT_PATH
    const auto configuredPath = std::filesystem::path(DICOM_EDITOR_DCMTK_DICT_PATH);
    if ((current == nullptr || !std::filesystem::exists(current)) && std::filesystem::exists(configuredPath)) {
        setenv("DCMDICTPATH", configuredPath.string().c_str(), 1);
    }
#endif
}

std::string tagToString(const DcmTagKey& key)
{
    std::ostringstream out;
    out << '(' << std::hex << std::setw(4) << std::setfill('0') << key.getGroup() << ','
        << std::setw(4) << key.getElement() << ')';
    return out.str();
}

std::string keywordFor(DcmTag& tag)
{
    const char* name = tag.getTagName();
    return name == nullptr ? "" : name;
}

std::string vrFor(const DcmElement& element)
{
    DcmVR vr(element.getVR());
    const char* vrName = vr.getVRName();
    return vrName == nullptr ? "" : vrName;
}

std::string vmFor(DcmElement& element)
{
    std::ostringstream out;
    out << element.getVM();
    return out.str();
}

std::string valuePreviewFor(DcmElement& element)
{
    if (element.ident() == EVR_SQ) {
        const auto& sequence = static_cast<DcmSequenceOfItems&>(element);
        std::ostringstream out;
        out << sequence.card() << " item";
        if (sequence.card() != 1) {
            out << 's';
        }
        return out.str();
    }

    OFString value;
    if (element.getOFStringArray(value).bad()) {
        return "<binary>";
    }

    std::string preview(value.c_str());
    constexpr std::size_t maxPreviewLength = 160;
    if (preview.size() > maxPreviewLength) {
        preview.resize(maxPreviewLength - 3);
        preview += "...";
    }
    return preview;
}

bool isEditable(const DcmElement& element)
{
    return element.ident() != EVR_SQ;
}

void collectNodesFromItem(DcmItem& item,
    const std::vector<SequenceItemRef>& parents,
    unsigned int depth,
    std::vector<DicomNode>& nodes)
{
    for (unsigned long index = 0; index < item.card(); ++index) {
        DcmElement* element = item.getElement(index);
        if (element == nullptr) {
            continue;
        }

        DcmTag tag(element->getTag());
        const DcmTagKey key = tag.getXTag();
        const bool sequence = element->ident() == EVR_SQ;

        DicomNode node;
        node.kind = sequence ? DicomNodeKind::Sequence : DicomNodeKind::Element;
        node.path = DicomPath::element(parents, key);
        node.tag = tagToString(key);
        node.keyword = keywordFor(tag);
        node.vr = vrFor(*element);
        node.vm = vmFor(*element);
        node.valuePreview = valuePreviewFor(*element);
        node.depth = depth;
        node.editable = isEditable(*element);
        nodes.push_back(std::move(node));

        if (!sequence) {
            continue;
        }

        auto& sequenceElement = static_cast<DcmSequenceOfItems&>(*element);
        for (unsigned long itemIndex = 0; itemIndex < sequenceElement.card(); ++itemIndex) {
            std::vector<SequenceItemRef> itemParents = parents;
            itemParents.push_back({key, itemIndex});

            DicomNode itemNode;
            itemNode.kind = DicomNodeKind::Item;
            itemNode.path = DicomPath::item(itemParents);
            itemNode.tag = "";
            itemNode.keyword = "Item";
            itemNode.vr = "";
            itemNode.vm = "";
            itemNode.valuePreview = "#" + std::to_string(itemIndex);
            itemNode.depth = depth + 1;
            itemNode.editable = false;
            nodes.push_back(std::move(itemNode));

            DcmItem* nested = sequenceElement.getItem(itemIndex);
            if (nested != nullptr) {
                collectNodesFromItem(*nested, itemParents, depth + 2, nodes);
            }
        }
    }
}

DcmItem& resolveItem(DcmItem& root, const DicomPath& path)
{
    DcmItem* current = &root;
    for (const auto& parent : path.parents()) {
        DcmElement* element = nullptr;
        requireGood(current->findAndGetElement(parent.sequenceTag, element), "Find sequence " + tagToString(parent.sequenceTag));
        if (element == nullptr || element->ident() != EVR_SQ) {
            throw DicomError("Path segment is not a sequence: " + tagToString(parent.sequenceTag));
        }

        auto* sequence = static_cast<DcmSequenceOfItems*>(element);
        DcmItem* next = sequence->getItem(parent.itemIndex);
        if (next == nullptr) {
            throw DicomError("Sequence item index is out of range: " + std::to_string(parent.itemIndex));
        }
        current = next;
    }
    return *current;
}

const DcmItem& resolveItem(const DcmItem& root, const DicomPath& path)
{
    return resolveItem(const_cast<DcmItem&>(root), path);
}

} // namespace

DicomDocument::DicomDocument()
{
    ensureDictionaryPath();
    createEmpty();
}

void DicomDocument::createEmpty()
{
    file_ = std::make_unique<DcmFileFormat>();
    filePath_.clear();
    dirty_ = false;
}

void DicomDocument::load(const std::filesystem::path& path)
{
    auto loaded = std::make_unique<DcmFileFormat>();
    requireGood(loaded->loadFile(path.string().c_str()), "Load DICOM file");
    file_ = std::move(loaded);
    filePath_ = path;
    dirty_ = false;
}

void DicomDocument::save()
{
    if (filePath_.empty()) {
        throw DicomError("Cannot save without a file path");
    }
    requireGood(file_->saveFile(filePath_.string().c_str(), EXS_LittleEndianExplicit), "Save DICOM file");
    dirty_ = false;
}

void DicomDocument::saveAs(const std::filesystem::path& path)
{
    filePath_ = path;
    save();
}

DcmDataset& DicomDocument::dataset()
{
    return *file_->getDataset();
}

const DcmDataset& DicomDocument::dataset() const
{
    return *file_->getDataset();
}

DcmItem& DicomDocument::itemAt(const DicomPath& path)
{
    if (!path.pointsToDatasetItem()) {
        throw DicomError("Path does not point to a dataset item: " + path.toString());
    }
    return resolveItem(dataset(), path);
}

const DcmItem& DicomDocument::itemAt(const DicomPath& path) const
{
    if (!path.pointsToDatasetItem()) {
        throw DicomError("Path does not point to a dataset item: " + path.toString());
    }
    return resolveItem(dataset(), path);
}

DcmElement& DicomDocument::elementAt(const DicomPath& path)
{
    if (!path.pointsToElement()) {
        throw DicomError("Path does not point to an element: " + path.toString());
    }
    DcmItem& parent = resolveItem(dataset(), path);
    DcmElement* element = nullptr;
    requireGood(parent.findAndGetElement(*path.elementTag(), element), "Find element " + path.toString());
    if (element == nullptr) {
        throw DicomError("Element not found: " + path.toString());
    }
    return *element;
}

const DcmElement& DicomDocument::elementAt(const DicomPath& path) const
{
    return const_cast<DicomDocument&>(*this).elementAt(path);
}

std::vector<DicomNode> DicomDocument::nodes() const
{
    std::vector<DicomNode> result;
    DicomNode root;
    root.kind = DicomNodeKind::Dataset;
    root.path = DicomPath::dataset();
    root.keyword = "Dataset";
    root.valuePreview = filePath_.empty() ? "<new>" : filePath_.string();
    root.editable = false;
    result.push_back(std::move(root));
    collectNodesFromItem(const_cast<DcmDataset&>(dataset()), {}, 1, result);
    return result;
}

const std::filesystem::path& DicomDocument::filePath() const
{
    return filePath_;
}

bool DicomDocument::hasFilePath() const
{
    return !filePath_.empty();
}

bool DicomDocument::dirty() const
{
    return dirty_;
}

void DicomDocument::markDirty()
{
    dirty_ = true;
}

void DicomDocument::clearDirty()
{
    dirty_ = false;
}

} // namespace dicom_editor
