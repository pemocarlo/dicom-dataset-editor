#include "FileTreePanel.hpp"

#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>
#include <FL/Fl_Tree_Prefs.H>
#include <FL/fl_ask.H>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

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
    enum class Kind : std::uint8_t { Patient, Study, Series, File } kind{};
    std::size_t fileIndex{};
    dicom_editor::BatchEditTarget target;
    dicom_editor::FileGroupTarget removeTarget;
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
    tree_->selectmode(FL_TREE_SELECT_MULTI);
    tree_->callback(treeCallback, this);
    resizable(tree_);
    end();
}

FileTreePanel::~FileTreePanel() {
    Fl::remove_timeout(centerActiveCallback, this);
    Fl::remove_timeout(activatePendingCallback, this);
}

void FileTreePanel::setFiles(const std::vector<dicom_editor::OpenDicomFile> &files) {
    const int previousScroll = tree_->vposition();
    std::set<std::size_t> selectedFileIndices;
    for (auto *selected = tree_->first_selected_item(); selected != nullptr; selected = tree_->next_selected_item(selected)) {
        auto *data = static_cast<TreeItemData *>(selected->user_data());
        if (data != nullptr && data->kind == TreeItemData::Kind::File) {
            selectedFileIndices.insert(data->fileIndex);
        }
    }
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
    std::optional<std::size_t> newActiveFileIndex;
    std::size_t restoredSelectionCount = 0;
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
            data->removeTarget = {
                .level = dicom_editor::FileGroupLevel::Patient, .id = hierarchy.patientId, .label = hierarchy.patientLabel};
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
            data->removeTarget = {.level = dicom_editor::FileGroupLevel::Study, .id = hierarchy.studyId, .label = hierarchy.studyLabel};
            studyItem->user_data(data.get());
            itemData_.push_back(std::move(data));
        }
        if (seriesPath != previousSeriesPath) {
            seriesItem = tree_->add(studyItem, groupLabel(hierarchy.seriesLabel, hierarchy.seriesId).c_str());
            if (seriesItem != nullptr) {
                auto data = std::make_unique<TreeItemData>();
                data->kind = TreeItemData::Kind::Series;
                data->removeTarget = {
                    .level = dicom_editor::FileGroupLevel::Series, .id = hierarchy.seriesId, .label = hierarchy.seriesLabel};
                seriesItem->user_data(data.get());
                itemData_.push_back(std::move(data));
            }
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
            if (selectedFileIndices.contains(file.index)) {
                tree_->select(item, 0);
                ++restoredSelectionCount;
            }
            if (file.active) {
                activeItem = item;
                newActiveFileIndex = file.index;
            }
        }
    }
    if (restoredSelectionCount == 0 && activeItem != nullptr) {
        tree_->select(activeItem, 0);
    }
    for (const auto &path : closedPaths) {
        tree_->close(path.c_str(), 0);
    }
    if (activeItem != nullptr) {
        for (auto *parent = activeItem->parent(); parent != nullptr; parent = parent->parent()) {
            tree_->open(parent, 0);
        }
    }
    tree_->recalc_tree();
    tree_->vposition(previousScroll);
    const bool activeFileChanged = newActiveFileIndex != activeFileIndex_;
    activeFileIndex_ = newActiveFileIndex;
    if (activeItem != nullptr && activeFileChanged && !activatingFromTree_) {
        Fl::remove_timeout(centerActiveCallback, this);
        Fl::add_timeout(0.0, centerActiveCallback, this);
    } else if (activatingFromTree_) {
        Fl::remove_timeout(centerActiveCallback, this);
    }
    tree_->redraw();
}

void FileTreePanel::setActivationHandler(std::function<void(std::size_t)> handler) { activationHandler_ = std::move(handler); }

void FileTreePanel::setRemoveHandler(std::function<void(std::size_t)> handler) { removeHandler_ = std::move(handler); }

void FileTreePanel::setRemoveDocumentsHandler(std::function<void(const std::vector<std::size_t> &)> handler) {
    removeDocumentsHandler_ = std::move(handler);
}

void FileTreePanel::setRemoveGroupHandler(std::function<void(const dicom_editor::FileGroupTarget &)> handler) {
    removeGroupHandler_ = std::move(handler);
}

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
        if (data->kind == TreeItemData::Kind::File) {
            if (tree_->is_selected(item) == 0) {
                tree_->select_only(item, 0);
            }
        } else {
            tree_->select_only(item, 0);
        }
        Fl_Menu_Button menu(Fl::event_x(), Fl::event_y(), 0, 0);
        menu.type(Fl_Menu_Button::POPUP3);
        if (data->kind == TreeItemData::Kind::File) {
            std::vector<std::size_t> selectedFiles;
            for (auto *selected = tree_->first_selected_item(); selected != nullptr; selected = tree_->next_selected_item(selected)) {
                auto *selectedData = static_cast<TreeItemData *>(selected->user_data());
                if (selectedData != nullptr && selectedData->kind == TreeItemData::Kind::File) {
                    selectedFiles.push_back(selectedData->fileIndex);
                }
            }
            const bool multiple = selectedFiles.size() > 1;
            menu.add("File Information");
            menu.add(multiple ? "Remove Selected Datasets..." : "Remove from Workspace...");
            if (const auto *selectedItem = menu.popup(); selectedItem != nullptr) {
                const auto selected = std::string{selectedItem->label()};
                if (selected == "File Information") {
                    fl_message("%s", data->details.c_str());
                } else if (selected == "Remove Selected Datasets..." && removeDocumentsHandler_) {
                    removeDocumentsHandler_(selectedFiles);
                } else if (selected == "Remove from Workspace..." && removeHandler_) {
                    removeHandler_(data->fileIndex);
                }
            }
        } else {
            if (data->kind == TreeItemData::Kind::Patient || data->kind == TreeItemData::Kind::Study) {
                menu.add(data->kind == TreeItemData::Kind::Patient ? "Batch Edit Patient Attributes..." : "Batch Edit Study Attributes...");
            }
            menu.add(data->kind == TreeItemData::Kind::Patient ? "Remove Patient from Workspace..."
                     : data->kind == TreeItemData::Kind::Study ? "Remove Study from Workspace..."
                                                               : "Remove Series from Workspace...");
            if (const auto *selectedItem = menu.popup(); selectedItem != nullptr) {
                const auto selected = std::string{selectedItem->label()};
                if ((data->kind == TreeItemData::Kind::Patient || data->kind == TreeItemData::Kind::Study) &&
                    selected.starts_with("Batch Edit") && batchEditHandler_) {
                    batchEditHandler_(data->target);
                } else if (selected.starts_with("Remove ") && removeGroupHandler_) {
                    removeGroupHandler_(data->removeTarget);
                }
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
        panel.pendingActivationIndex_ = itemData->fileIndex;
        Fl::remove_timeout(activatePendingCallback, &panel);
        Fl::add_timeout(0.0, activatePendingCallback, &panel);
    }
}

void FileTreePanel::activatePendingCallback(void *data) {
    auto &panel = *static_cast<FileTreePanel *>(data);
    if (!panel.pendingActivationIndex_ || !panel.activationHandler_) {
        return;
    }
    const std::size_t index = *panel.pendingActivationIndex_;
    panel.pendingActivationIndex_.reset();
    panel.activatingFromTree_ = true;
    panel.activationHandler_(index);
    panel.activatingFromTree_ = false;
}

void FileTreePanel::centerActiveCallback(void *data) {
    auto &panel = *static_cast<FileTreePanel *>(data);
    if (!panel.activeFileIndex_) {
        return;
    }
    for (auto *item = panel.tree_->first(); item != nullptr; item = panel.tree_->next(item)) {
        auto *itemData = static_cast<TreeItemData *>(item->user_data());
        if (itemData != nullptr && itemData->kind == TreeItemData::Kind::File && itemData->fileIndex == *panel.activeFileIndex_) {
            panel.tree_->recalc_tree();
            panel.tree_->show_item_middle(item);
            panel.tree_->redraw();
            return;
        }
    }
}
