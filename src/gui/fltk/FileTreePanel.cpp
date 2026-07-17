#include "FileTreePanel.hpp"

#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>
#include <FL/Fl_Tree_Prefs.H>
#include <FL/fl_ask.H>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

namespace {

constexpr int Padding = 10;
constexpr int HeaderHeight = 34;

std::string safeTreeLabel(std::string value) {
    std::ranges::replace(value, '/', '_');
    std::ranges::replace(value, '\\', '_');
    return value;
}

std::string groupLabel(const std::string &label, const std::string &id) {
    std::string result = safeTreeLabel(label);
    if (!id.empty() && id != label) {
        result += " [" + safeTreeLabel(id) + "]";
    }
    return result;
}

std::string fileLabel(const dicom_editor::OpenDicomFile &file) {
    const auto filename = file.path.empty() ? std::string{"Untitled"} : file.path.filename().string();
    std::string result = file.active ? "> " : "  ";
    result += safeTreeLabel(filename);
    if (file.dirty) {
        result += " *";
    }
    return result;
}

} // namespace

struct FileTreePanel::TreeItemData {
    enum class Kind : std::uint8_t { Patient, Study, File } kind{};
    std::size_t fileIndex{};
    dicom_editor::BatchEditTarget target;
    std::string details;
};

FileTreePanel::FileTreePanel(int x, int y, int width, int height) : Fl_Group(x, y, width, height) {
    box(FL_FLAT_BOX);
    color(fl_rgb_color(238, 243, 247));
    heading_ = new Fl_Box(x + Padding, y + Padding, width - 2 * Padding, HeaderHeight - Padding, "OPEN DATASETS");
    heading_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    heading_->labelfont(FL_HELVETICA_BOLD);
    heading_->labelsize(13);
    heading_->labelcolor(fl_rgb_color(58, 78, 94));
    tree_ = new Fl_Tree(x + Padding, y + HeaderHeight, width - 2 * Padding, height - HeaderHeight - Padding);
    tree_->box(FL_BORDER_BOX);
    tree_->color(FL_WHITE);
    tree_->selection_color(fl_rgb_color(207, 228, 245));
    tree_->item_labelsize(14);
    tree_->item_draw_mode(FL_TREE_ITEM_DRAW_LABEL_AND_WIDGET);
    tree_->showroot(0);
    tree_->selectmode(FL_TREE_SELECT_SINGLE);
    tree_->callback(treeCallback, this);
    resizable(tree_);
    end();
}

FileTreePanel::~FileTreePanel() = default;

void FileTreePanel::setFiles(const std::vector<dicom_editor::OpenDicomFile> &files) {
    const int previousScroll = tree_->vposition();
    std::set<std::string> closedPaths;
    std::array<char, 4096> pathBuffer{};
    for (auto *item = tree_->first(); item != nullptr; item = tree_->next(item)) {
        if (item->has_children() != 0 && item->is_close() != 0 &&
            tree_->item_pathname(pathBuffer.data(), static_cast<int>(pathBuffer.size()), item) == 0) {
            closedPaths.emplace(pathBuffer.data());
        }
    }
    tree_->clear();
    itemData_.clear();
    itemData_.reserve(files.size() * 3);
    Fl_Tree_Item *patientItem = nullptr;
    Fl_Tree_Item *studyItem = nullptr;
    Fl_Tree_Item *seriesItem = nullptr;
    std::string previousPatientPath;
    std::string previousStudyPath;
    std::string previousSeriesPath;
    Fl_Tree_Item *activeItem = nullptr;
    for (const auto &file : files) {
        const auto &hierarchy = file.hierarchy;
        const std::string patientPath = groupLabel(hierarchy.patientLabel, hierarchy.patientId);
        const std::string studyPath = patientPath + "/" + groupLabel(hierarchy.studyLabel, hierarchy.studyId);
        const std::string seriesPath = studyPath + "/" + groupLabel(hierarchy.seriesLabel, hierarchy.seriesId);
        if (patientPath != previousPatientPath) {
            patientItem = tree_->add(patientPath.c_str());
            studyItem = nullptr;
            seriesItem = nullptr;
            previousPatientPath = patientPath;
            previousStudyPath.clear();
            previousSeriesPath.clear();
        }
        if (patientItem != nullptr && patientItem->user_data() == nullptr) {
            auto data = std::make_unique<TreeItemData>();
            data->kind = TreeItemData::Kind::Patient;
            data->target = {.level = dicom_editor::BatchEditLevel::Patient, .id = hierarchy.patientId, .label = hierarchy.patientLabel};
            patientItem->user_data(data.get());
            itemData_.push_back(std::move(data));
        }
        if (studyPath != previousStudyPath) {
            studyItem = tree_->add(patientItem, groupLabel(hierarchy.studyLabel, hierarchy.studyId).c_str());
            seriesItem = nullptr;
            previousStudyPath = studyPath;
            previousSeriesPath.clear();
        }
        if (studyItem != nullptr && studyItem->user_data() == nullptr) {
            auto data = std::make_unique<TreeItemData>();
            data->kind = TreeItemData::Kind::Study;
            data->target = {.level = dicom_editor::BatchEditLevel::Study, .id = hierarchy.studyId, .label = hierarchy.studyLabel};
            studyItem->user_data(data.get());
            itemData_.push_back(std::move(data));
        }
        if (seriesPath != previousSeriesPath) {
            seriesItem = tree_->add(studyItem, groupLabel(hierarchy.seriesLabel, hierarchy.seriesId).c_str());
            previousSeriesPath = seriesPath;
        }
        if (auto *item = tree_->add(seriesItem, fileLabel(file).c_str()); item != nullptr) {
            auto data = std::make_unique<TreeItemData>();
            data->kind = TreeItemData::Kind::File;
            data->fileIndex = file.index;
            data->details = std::format("Filename: {}\nFull path: {}\nInstance number: {}\nPatient: {}\nStudy: {}\nSeries: {}",
                                        file.path.filename().string(), file.path.string(),
                                        hierarchy.instanceNumber ? std::to_string(*hierarchy.instanceNumber) : "<missing>",
                                        hierarchy.patientLabel, hierarchy.studyLabel, hierarchy.seriesLabel);
            item->user_data(data.get());
            itemData_.push_back(std::move(data));
            if (file.dirty) {
                item->labelfont(FL_HELVETICA_BOLD);
            }
            if (file.active) {
                tree_->select(item, 0);
                activeItem = item;
            }
        }
    }
    for (const auto &path : closedPaths) {
        tree_->close(path.c_str(), 0);
    }
    tree_->recalc_tree();
    tree_->vposition(previousScroll);
    if (activeItem != nullptr && tree_->displayed(activeItem) == 0) {
        tree_->show_item(activeItem);
    }
    tree_->redraw();
}

void FileTreePanel::setActivationHandler(std::function<void(std::size_t)> handler) { activationHandler_ = std::move(handler); }

void FileTreePanel::setBatchEditHandler(std::function<void(const dicom_editor::BatchEditTarget &)> handler) {
    batchEditHandler_ = std::move(handler);
}

void FileTreePanel::setFontSize(int size) {
    heading_->labelsize(std::max(12, size - 1));
    tree_->item_labelsize(size);
    tree_->redraw();
}

int FileTreePanel::handle(int event) {
    if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE) {
        auto *item = tree_->find_clicked();
        auto *data = item == nullptr ? nullptr : static_cast<TreeItemData *>(item->user_data());
        if (data == nullptr) {
            return 1;
        }
        Fl_Menu_Button menu(Fl::event_x(), Fl::event_y(), 0, 0);
        menu.type(Fl_Menu_Button::POPUP3);
        if (data->kind == TreeItemData::Kind::File) {
            menu.add("File Information");
            if (menu.popup() != nullptr) {
                fl_message("%s", data->details.c_str());
            }
        } else {
            menu.add(data->kind == TreeItemData::Kind::Patient ? "Batch Edit Patient Attributes..." : "Batch Edit Study Attributes...");
            if (menu.popup() != nullptr && batchEditHandler_) {
                batchEditHandler_(data->target);
            }
        }
        return 1;
    }
    return Fl_Group::handle(event);
}

void FileTreePanel::resize(int x, int y, int width, int height) {
    Fl_Group::resize(x, y, width, height);
    heading_->resize(x + Padding, y + Padding, width - 2 * Padding, HeaderHeight - Padding);
    tree_->resize(x + Padding, y + HeaderHeight, width - 2 * Padding, height - HeaderHeight - Padding);
}

void FileTreePanel::treeCallback(Fl_Widget *, void *data) {
    auto &panel = *static_cast<FileTreePanel *>(data);
    auto *item = panel.tree_->callback_item();
    auto *itemData = item == nullptr ? nullptr : static_cast<TreeItemData *>(item->user_data());
    if (itemData != nullptr && itemData->kind == TreeItemData::Kind::File && panel.activationHandler_) {
        panel.activationHandler_(itemData->fileIndex);
    }
}
