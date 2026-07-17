#include "dicom_editor/DicomDocument.hpp"

#include "dicom_editor/DicomError.hpp"
#include "dicom_editor/DicomPath.hpp"
#include "dicom_editor/RuntimePaths.hpp"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dctag.h>
#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/dcmdata/dcvr.h>
#include <dcmtk/dcmdata/dcxfer.h>
#include <dcmtk/ofstd/ofcond.h>
#include <dcmtk/ofstd/offile.h>
#include <dcmtk/ofstd/ofstring.h>

#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace dicom_editor {

namespace {

constexpr std::string_view PixelDataPlaceholder = "[Pixel Data not displayed]";

bool isPixelData(const DcmTagKey &key) { return key.getGroup() == 0x7fe0 && key.getElement() == 0x0010; }

std::string conditionMessage(const OFCondition &condition) {
    return condition.text() == nullptr ? "unknown DCMTK error" : condition.text();
}

void requireGood(const OFCondition &condition, const std::string &action) {
    if (condition.bad()) {
        throw DicomError(std::format("{}: {}", action, conditionMessage(condition)));
    }
}

void setDictionaryPath(const std::filesystem::path &path) {
#ifdef _WIN32
    _putenv_s("DCMDICTPATH", path.string().c_str());
#else
    setenv("DCMDICTPATH", path.string().c_str(), 1);
#endif
}

void ensureDictionaryPath() {
    const auto installedPath = installedDataPath("dcmtk/dicom.dic");
    const char *current = std::getenv("DCMDICTPATH");
    if ((current == nullptr || !std::filesystem::exists(current)) && std::filesystem::exists(installedPath)) {
        setDictionaryPath(installedPath);
        return;
    }

#ifdef DICOM_EDITOR_DCMTK_DICT_FILE
    const auto configuredPath = std::filesystem::path(DICOM_EDITOR_DCMTK_DICT_FILE);
    if ((current == nullptr || !std::filesystem::exists(current)) && std::filesystem::exists(configuredPath)) {
        setDictionaryPath(configuredPath);
    }
#endif
}

std::string tagToString(const DcmTagKey &key) { return std::format("({:04x},{:04x})", key.getGroup(), key.getElement()); }

std::string keywordFor(DcmTag &tag) {
    const char *name = tag.getTagName();
    return name == nullptr ? std::string{} : std::string{name};
}

std::string vrFor(const DcmElement &element) {
    DcmVR vr(element.getVR());
    const char *vrName = vr.getVRName();
    return vrName == nullptr ? std::string{} : std::string{vrName};
}

std::string vmFor(DcmElement &element) { return std::to_string(element.getVM()); }

std::string valueFor(DcmElement &element) {
    if (element.ident() == EVR_SQ) {
        const auto &sequence = static_cast<DcmSequenceOfItems &>(element);
        return std::format("{} item{}", sequence.card(), sequence.card() == 1 ? "" : "s");
    }

    OFString value;
    if (element.getOFStringArray(value).bad()) {
        return "<binary>";
    }

    return value;
}

std::string valuePreviewFor(const std::string &value) {
    std::string preview(value);
    constexpr std::size_t maxPreviewLength = 160;
    if (preview.size() > maxPreviewLength) {
        preview.resize(maxPreviewLength - 3);
        preview += "...";
    }
    return preview;
}

bool isEditable(const DcmElement &element) { return element.ident() != EVR_SQ && !isPixelData(element.getTag().getXTag()); }

void collectNodesFromItem(DcmItem &item, const std::vector<SequenceItemRef> &parents, unsigned int depth, std::vector<DicomNode> &nodes) {
    for (unsigned long index = 0; index < item.card(); ++index) {
        DcmElement *element = item.getElement(index);
        if (element == nullptr) {
            continue;
        }

        DcmTag tag(element->getTag());
        const DcmTagKey key = tag.getXTag();
        const bool sequence = element->ident() == EVR_SQ;
        const bool pixelData = isPixelData(key);

        std::string value = pixelData ? std::string{PixelDataPlaceholder} : valueFor(*element);
        std::string valuePreview = pixelData ? std::string{} : valuePreviewFor(value);
        DicomNode node{
            .kind = sequence ? DicomNodeKind::Sequence : DicomNodeKind::Element,
            .path = DicomPath::element(parents, key),
            .tag = tagToString(key),
            .keyword = keywordFor(tag),
            .vr = vrFor(*element),
            .vm = vmFor(*element),
            .value = std::move(value),
            .valuePreview = std::move(valuePreview),
            .depth = depth,
            .editable = isEditable(*element),
        };
        nodes.push_back(std::move(node));

        if (!sequence) {
            continue;
        }

        auto &sequenceElement = static_cast<DcmSequenceOfItems &>(*element);
        for (unsigned long itemIndex = 0; itemIndex < sequenceElement.card(); ++itemIndex) {
            std::vector<SequenceItemRef> itemParents = parents;
            itemParents.push_back({key, itemIndex});

            const auto itemValue = std::format("#{}", itemIndex);
            DicomNode itemNode{
                .kind = DicomNodeKind::Item,
                .path = DicomPath::item(itemParents),
                .tag = {},
                .keyword = "Item",
                .vr = {},
                .vm = {},
                .value = itemValue,
                .valuePreview = itemValue,
                .depth = depth + 1,
                .editable = false,
            };
            nodes.push_back(std::move(itemNode));

            DcmItem *nested = sequenceElement.getItem(itemIndex);
            if (nested != nullptr) {
                collectNodesFromItem(*nested, itemParents, depth + 2, nodes);
            }
        }
    }
}

DcmItem &resolveItem(DcmItem &root, const DicomPath &path) {
    DcmItem *current = &root;
    for (const auto &parent : path.parents()) {
        DcmElement *element = nullptr;
        requireGood(current->findAndGetElement(parent.sequenceTag, element), "Find sequence " + tagToString(parent.sequenceTag));
        if (element == nullptr || element->ident() != EVR_SQ) {
            throw DicomError("Path segment is not a sequence: " + tagToString(parent.sequenceTag));
        }

        auto *sequence = static_cast<DcmSequenceOfItems *>(element);
        DcmItem *next = sequence->getItem(parent.itemIndex);
        if (next == nullptr) {
            throw DicomError(std::format("Sequence item index is out of range: {}", parent.itemIndex));
        }
        current = next;
    }
    return *current;
}

const DcmItem &resolveItem(const DcmItem &root, const DicomPath &path) { return resolveItem(const_cast<DcmItem &>(root), path); }

} // namespace

DicomDocument::DicomDocument() {
    ensureDictionaryPath();
    createEmpty();
}

void DicomDocument::createEmpty() {
    file_ = std::make_unique<DcmFileFormat>();
    filePath_.clear();
    dirty_ = false;
}

void DicomDocument::load(const std::filesystem::path &path) {
    auto loaded = std::make_unique<DcmFileFormat>();
    requireGood(loaded->loadFile(path.string().c_str()), "Load DICOM file");
    file_ = std::move(loaded);
    filePath_ = path;
    dirty_ = false;
}

void DicomDocument::save() {
    if (filePath_.empty()) {
        throw DicomError("Cannot save without a file path");
    }
    requireGood(file_->saveFile(filePath_.string().c_str(), EXS_LittleEndianExplicit), "Save DICOM file");
    dirty_ = false;
}

void DicomDocument::saveAs(const std::filesystem::path &path) {
    filePath_ = path;
    save();
}

DcmDataset &DicomDocument::dataset() { return *file_->getDataset(); }

const DcmDataset &DicomDocument::dataset() const { return *file_->getDataset(); }

DcmItem &DicomDocument::itemAt(const DicomPath &path) {
    if (!path.pointsToDatasetItem()) {
        throw DicomError("Path does not point to a dataset item: " + path.toString());
    }
    return resolveItem(dataset(), path);
}

const DcmItem &DicomDocument::itemAt(const DicomPath &path) const {
    if (!path.pointsToDatasetItem()) {
        throw DicomError("Path does not point to a dataset item: " + path.toString());
    }
    return resolveItem(dataset(), path);
}

DcmElement &DicomDocument::elementAt(const DicomPath &path) {
    const auto &tag = path.elementTag();
    if (!tag) {
        throw DicomError("Path does not point to an element: " + path.toString());
    }
    DcmItem &parent = resolveItem(dataset(), path);
    DcmElement *element = nullptr;
    requireGood(parent.findAndGetElement(*tag, element), "Find element " + path.toString());
    if (element == nullptr) {
        throw DicomError("Element not found: " + path.toString());
    }
    return *element;
}

const DcmElement &DicomDocument::elementAt(const DicomPath &path) const { return const_cast<DicomDocument &>(*this).elementAt(path); }

std::vector<DicomNode> DicomDocument::nodes() const {
    const auto rootValue = filePath_.empty() ? std::string{"<new>"} : filePath_.string();
    std::vector<DicomNode> result{{
        .kind = DicomNodeKind::Dataset,
        .path = DicomPath::dataset(),
        .tag = {},
        .keyword = "Dataset",
        .vr = {},
        .vm = {},
        .value = rootValue,
        .valuePreview = rootValue,
        .depth = 0,
        .editable = false,
    }};
    collectNodesFromItem(const_cast<DcmDataset &>(dataset()), {}, 1, result);
    return result;
}

const std::filesystem::path &DicomDocument::filePath() const { return filePath_; }

bool DicomDocument::hasFilePath() const { return !filePath_.empty(); }

bool DicomDocument::dirty() const { return dirty_; }

void DicomDocument::markDirty() { dirty_ = true; }

void DicomDocument::clearDirty() { dirty_ = false; }

} // namespace dicom_editor
