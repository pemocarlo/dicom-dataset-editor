#include "dicom_editor/DicomDocument.hpp"

#include "dicom_editor/DicomError.hpp"
#include "dicom_editor/DicomPath.hpp"
#include "dicom_editor/RuntimePaths.hpp"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcrledrg.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dctag.h>
#include <dcmtk/dcmdata/dctagkey.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcvr.h>
#include <dcmtk/dcmdata/dcxfer.h>
#include <dcmtk/dcmimage/diregist.h> // IWYU pragma: keep
#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmimgle/diutils.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include <dcmtk/dcmjpls/djdecode.h>
#include <dcmtk/ofstd/ofcond.h>
#include <dcmtk/ofstd/offile.h>
#include <dcmtk/ofstd/ofstring.h>
#include <ofstd/oftypes.h>

#include <cstdio>
#include <cstdlib>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dicom_editor {

namespace {

constexpr std::string_view PixelDataPlaceholder = "[Pixel Data not displayed]";

class DecoderRegistry final {
  public:
    DecoderRegistry() {
        DcmRLEDecoderRegistration::registerCodecs();
        DJDecoderRegistration::registerCodecs();
        DJLSDecoderRegistration::registerCodecs();
    }

    ~DecoderRegistry() {
        DJLSDecoderRegistration::cleanup();
        DJDecoderRegistration::cleanup();
        DcmRLEDecoderRegistration::cleanup();
    }

    DecoderRegistry(const DecoderRegistry &) = delete;
    DecoderRegistry &operator=(const DecoderRegistry &) = delete;
};

void ensureDecodersRegistered() {
    static const DecoderRegistry registry;
    static_cast<void>(registry);
}

void logPixelPreview(std::string_view message) { std::println(stderr, "[pixel-preview] {}", message); }

std::string transferSyntaxName(E_TransferSyntax syntax) {
    const DcmXfer transferSyntax(syntax);
    const char *name = transferSyntax.getXferName();
    const char *uid = transferSyntax.getXferID();
    return std::format("{} ({})", name == nullptr ? "unknown" : name, uid == nullptr ? "unknown UID" : uid);
}

std::string missingImageAttributes(DcmDataset &dataset) {
    std::vector<std::string_view> missing;
    const auto require = [&dataset, &missing](const DcmTagKey &tag, std::string_view name) {
        if (dataset.tagExistsWithValue(tag) == OFFalse) {
            missing.push_back(name);
        }
    };
    require(DCM_Rows, "Rows");
    require(DCM_Columns, "Columns");
    require(DCM_SamplesPerPixel, "SamplesPerPixel");
    require(DCM_PhotometricInterpretation, "PhotometricInterpretation");
    require(DCM_BitsAllocated, "BitsAllocated");
    require(DCM_BitsStored, "BitsStored");
    require(DCM_HighBit, "HighBit");
    require(DCM_PixelRepresentation, "PixelRepresentation");

    std::string result;
    for (const auto name : missing) {
        if (!result.empty()) {
            result += ", ";
        }
        result += name;
    }
    return result;
}

bool isPixelData(const DcmTagKey &key) { return key.getGroup() == 0x7fe0 && key.getElement() == 0x0010; }

std::string conditionMessage(const OFCondition &condition) {
    return condition.text() == nullptr ? "unknown DCMTK error" : condition.text();
}

void requireGood(const OFCondition &condition, const std::string &action) {
    if (condition.bad()) {
        throw DicomError(std::format("{}: {}", action, conditionMessage(condition)));
    }
}

std::expected<void, DicomError> makeUnexpected(std::string action, const OFCondition &condition) {
    return std::unexpected(DicomError(std::format("{}: {}", action, conditionMessage(condition))));
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

std::string datasetString(DcmDataset &dataset, const DcmTagKey &tag) {
    OFString value;
    return dataset.findAndGetOFString(tag, value).good() ? std::string{value} : std::string{};
}

std::string labelOr(const std::string &preferred, const std::string &fallback, std::string_view missing) {
    if (!preferred.empty()) {
        return preferred;
    }
    return fallback.empty() ? std::string{missing} : fallback;
}

bool isEditable(const DcmElement &element) { return element.ident() != EVR_SQ && !isPixelData(element.getTag().getXTag()); }

void collectNodesFromItem(DcmItem &item, const std::vector<SequenceItemRef> &parents, unsigned int depth, bool validateValues,
                          std::vector<DicomNode> &nodes) {
    for (unsigned long index = 0; index < item.card(); ++index) {
        DcmElement *element = item.getElement(index);
        if (element == nullptr) {
            continue;
        }

        DcmTag tag(element->getTag());
        const DcmTagKey key = tag.getXTag();
        const bool sequence = element->ident() == EVR_SQ;
        const bool pixelData = isPixelData(key);
        const bool editable = isEditable(*element);

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
            .editable = editable,
            .invalidValue = validateValues && editable && element->checkValue("1-n").bad(),
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
                collectNodesFromItem(*nested, itemParents, depth + 2, validateValues, nodes);
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

std::expected<void, DicomError> DicomDocument::load(const std::filesystem::path &path) {
    auto loaded = std::make_unique<DcmFileFormat>();
    const auto result = loaded->loadFile(path.string().c_str());
    if (result.bad()) {
        return makeUnexpected("Load DICOM file", result);
    }
    file_ = std::move(loaded);
    filePath_ = path;
    dirty_ = false;
    return {};
}

std::expected<void, DicomError> DicomDocument::save() {
    if (filePath_.empty()) {
        return std::unexpected(DicomError("Cannot save without a file path"));
    }
    const auto result = file_->saveFile(filePath_.string().c_str(), EXS_LittleEndianExplicit);
    if (result.bad()) {
        return makeUnexpected("Save DICOM file", result);
    }
    dirty_ = false;
    return {};
}

std::expected<void, DicomError> DicomDocument::saveAs(const std::filesystem::path &path) {
    filePath_ = path;
    return save();
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

std::vector<DicomNode> DicomDocument::nodes(bool validateValues) const {
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
    collectNodesFromItem(const_cast<DcmDataset &>(dataset()), {}, 1, validateValues, result);
    return result;
}

PixelDataPreview DicomDocument::renderPixelData(unsigned long frameIndex) const {
    PixelDataPreview preview;
    auto &mutableDataset = const_cast<DcmDataset &>(dataset());
    logPixelPreview(std::format("request file='{}' frame={} original={} current={}",
                                filePath_.empty() ? "<new dataset>" : filePath_.string(), frameIndex + 1,
                                transferSyntaxName(mutableDataset.getOriginalXfer()), transferSyntaxName(mutableDataset.getCurrentXfer())));

    DcmElement *pixelData = nullptr;
    const auto findPixelData = mutableDataset.findAndGetElement(DCM_PixelData, pixelData);
    if (findPixelData.bad() || pixelData == nullptr) {
        logPixelPreview(std::format("PixelData lookup failed: {}", conditionMessage(findPixelData)));
        preview.message = "No pixel data in this dataset.";
        return preview;
    }
    const auto originalLength = pixelData->getLength(mutableDataset.getOriginalXfer());
    const auto currentLength = pixelData->getLength(mutableDataset.getCurrentXfer());
    logPixelPreview(std::format("PixelData found: VR={} class={} original-length={} current-length={}",
                                DcmVR(pixelData->getVR()).getVRName(), DcmVR(pixelData->ident()).getVRName(), originalLength,
                                currentLength));

    if (pixelData->ident() != EVR_PixelData) {
        preview.message = std::format("Pixel Data has unsupported internal VR {}.", DcmVR(pixelData->ident()).getVRName());
        logPixelPreview(preview.message);
        return preview;
    }
    const std::string missing = missingImageAttributes(mutableDataset);
    if (!missing.empty()) {
        preview.message = "Missing required image attribute(s): " + missing + ".";
        logPixelPreview(preview.message);
        return preview;
    }

    ensureDecodersRegistered();
    const auto representation = mutableDataset.chooseRepresentation(EXS_LittleEndianExplicit, nullptr);
    if (representation.bad()) {
        preview.message = std::format("Cannot decode {} pixel data: {}", transferSyntaxName(mutableDataset.getOriginalXfer()),
                                      conditionMessage(representation));
        logPixelPreview(preview.message);
        return preview;
    }
    const auto nativeLength = pixelData->getLength(EXS_LittleEndianExplicit);
    logPixelPreview(std::format("native representation ready: {} bytes", nativeLength));
    if (nativeLength == 0) {
        preview.message = "Pixel Data element is present but empty.";
        logPixelPreview(preview.message);
        return preview;
    }

    DicomImage image(&mutableDataset, EXS_LittleEndianExplicit, 0, frameIndex, 1);
    if (image.getStatus() != EIS_Normal) {
        const char *status = DicomImage::getString(image.getStatus());
        preview.message = std::format("Pixel data cannot be displayed: {}", status == nullptr ? "unknown error" : status);
        logPixelPreview(std::format("DicomImage failed: status={} ({})", static_cast<int>(image.getStatus()), preview.message));
        return preview;
    }

    const auto width = image.getWidth();
    const auto height = image.getHeight();
    const auto frameCount = image.getNumberOfFrames();
    if (width == 0 || height == 0 || width > static_cast<unsigned long>(std::numeric_limits<int>::max()) ||
        height > static_cast<unsigned long>(std::numeric_limits<int>::max())) {
        preview.message = "Pixel data has unsupported dimensions.";
        logPixelPreview(std::format("unsupported dimensions: {} x {}", width, height));
        return preview;
    }

    const auto outputSize = image.getOutputDataSize(8);
    if (outputSize == 0) {
        preview.message = "Pixel data could not be rendered.";
        logPixelPreview("DCMTK reported zero-byte rendered output");
        return preview;
    }

    preview.pixels.resize(static_cast<std::size_t>(outputSize));
    if (image.getOutputData(preview.pixels.data(), outputSize, 8, 0, 0) == 0) {
        preview.pixels.clear();
        preview.message = "Pixel data could not be rendered.";
        logPixelPreview("DCMTK getOutputData failed");
        return preview;
    }

    preview.width = static_cast<unsigned int>(width);
    preview.height = static_cast<unsigned int>(height);
    preview.channels = image.isMonochrome() != 0 ? 1 : 3;
    preview.frameIndex = frameIndex;
    preview.frameCount = frameCount;
    logPixelPreview(std::format("rendered {} x {}, channels={}, bytes={}, frame={}/{}", preview.width, preview.height, preview.channels,
                                preview.pixels.size(), preview.frameIndex + 1, preview.frameCount));
    return preview;
}

DicomHierarchy DicomDocument::hierarchy() const {
    auto &mutableDataset = const_cast<DcmDataset &>(dataset());
    DicomHierarchy result;
    result.patientId = datasetString(mutableDataset, DCM_PatientID);
    result.studyId = datasetString(mutableDataset, DCM_StudyInstanceUID);
    result.seriesId = datasetString(mutableDataset, DCM_SeriesInstanceUID);
    result.patientLabel = labelOr(datasetString(mutableDataset, DCM_PatientName), result.patientId, "Unknown patient");
    result.studyLabel = labelOr(datasetString(mutableDataset, DCM_StudyDescription), result.studyId, "Unknown study");
    result.seriesLabel = labelOr(datasetString(mutableDataset, DCM_SeriesDescription), result.seriesId, "Unknown series");
    return result;
}

bool DicomDocument::isDicomDirectory() const {
    OFString sopClassUid;
    if (file_->getMetaInfo()->findAndGetOFString(DCM_MediaStorageSOPClassUID, sopClassUid).good() &&
        sopClassUid == UID_MediaStorageDirectoryStorage) {
        return true;
    }
    return datasetString(const_cast<DcmDataset &>(dataset()), DCM_SOPClassUID) == UID_MediaStorageDirectoryStorage;
}

const std::filesystem::path &DicomDocument::filePath() const { return filePath_; }

bool DicomDocument::hasFilePath() const { return !filePath_.empty(); }

bool DicomDocument::dirty() const { return dirty_; }

void DicomDocument::markDirty() { dirty_ = true; }

void DicomDocument::clearDirty() { dirty_ = false; }

} // namespace dicom_editor
