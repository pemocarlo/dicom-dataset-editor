#include "FileTreePanel.hpp"

#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>
#include <FL/Fl_Tree_Prefs.H>
#include <FL/fl_ask.H>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

constexpr int Padding = 6;

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
    box(FL_THIN_UP_BOX);
    tree_ = new Fl_Tree(x + Padding, y + Padding, width - 2 * Padding, height - 2 * Padding);
    tree_->showroot(0);
    tree_->selectmode(FL_TREE_SELECT_SINGLE);
    tree_->callback(treeCallback, this);
    resizable(tree_);
    end();
}

FileTreePanel::~FileTreePanel() = default;

void FileTreePanel::setFiles(const std::vector<dicom_editor::OpenDicomFile> &files) {
    tree_->clear();
    itemData_.clear();
    itemData_.reserve(files.size() * 3);
    for (const auto &file : files) {
        const auto &hierarchy = file.hierarchy;
        const std::string patientPath = groupLabel(hierarchy.patientLabel, hierarchy.patientId);
        const std::string studyPath = patientPath + "/" + groupLabel(hierarchy.studyLabel, hierarchy.studyId);
        const std::string seriesPath = studyPath + "/" + groupLabel(hierarchy.seriesLabel, hierarchy.seriesId);
        if (auto *patient = tree_->add(patientPath.c_str()); patient != nullptr && patient->user_data() == nullptr) {
            auto data = std::make_unique<TreeItemData>();
            data->kind = TreeItemData::Kind::Patient;
            data->target = {.level = dicom_editor::BatchEditLevel::Patient, .id = hierarchy.patientId, .label = hierarchy.patientLabel};
            patient->user_data(data.get());
            itemData_.push_back(std::move(data));
        }
        if (auto *study = tree_->add(studyPath.c_str()); study != nullptr && study->user_data() == nullptr) {
            auto data = std::make_unique<TreeItemData>();
            data->kind = TreeItemData::Kind::Study;
            data->target = {.level = dicom_editor::BatchEditLevel::Study, .id = hierarchy.studyId, .label = hierarchy.studyLabel};
            study->user_data(data.get());
            itemData_.push_back(std::move(data));
        }
        if (auto *item = tree_->add((seriesPath + "/" + fileLabel(file)).c_str()); item != nullptr) {
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
                tree_->show_item(item);
            }
        }
    }
    tree_->redraw();
}

void FileTreePanel::setActivationHandler(std::function<void(std::size_t)> handler) { activationHandler_ = std::move(handler); }

void FileTreePanel::setBatchEditHandler(std::function<void(const dicom_editor::BatchEditTarget &)> handler) {
    batchEditHandler_ = std::move(handler);
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
    tree_->resize(x + Padding, y + Padding, width - 2 * Padding, height - 2 * Padding);
}

void FileTreePanel::treeCallback(Fl_Widget *, void *data) {
    auto &panel = *static_cast<FileTreePanel *>(data);
    auto *item = panel.tree_->callback_item();
    auto *itemData = item == nullptr ? nullptr : static_cast<TreeItemData *>(item->user_data());
    if (itemData != nullptr && itemData->kind == TreeItemData::Kind::File && panel.activationHandler_) {
        panel.activationHandler_(itemData->fileIndex);
    }
}
