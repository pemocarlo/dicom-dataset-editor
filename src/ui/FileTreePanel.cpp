#include "FileTreePanel.hpp"

#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/EditorController.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>
#include <FL/Fl_Tree_Prefs.H>

#include <algorithm>
#include <filesystem>
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
    if (!file.path.empty()) {
        result += " (" + safeTreeLabel(file.path.parent_path().string()) + ")";
    }
    return result;
}

} // namespace

FileTreePanel::FileTreePanel(int x, int y, int width, int height) : Fl_Group(x, y, width, height) {
    box(FL_THIN_UP_BOX);
    tree_ = new Fl_Tree(x + Padding, y + Padding, width - 2 * Padding, height - 2 * Padding);
    tree_->showroot(0);
    tree_->selectmode(FL_TREE_SELECT_SINGLE);
    tree_->callback(treeCallback, this);
    resizable(tree_);
    end();
}

void FileTreePanel::setFiles(const std::vector<dicom_editor::OpenDicomFile> &files) {
    tree_->clear();
    fileIndices_.clear();
    fileIndices_.reserve(files.size());
    for (const auto &file : files) {
        const auto &hierarchy = file.hierarchy;
        const std::string path = groupLabel(hierarchy.patientLabel, hierarchy.patientId) + "/" +
                                 groupLabel(hierarchy.studyLabel, hierarchy.studyId) + "/" +
                                 groupLabel(hierarchy.seriesLabel, hierarchy.seriesId) + "/" + fileLabel(file);
        if (auto *item = tree_->add(path.c_str()); item != nullptr) {
            fileIndices_.push_back(file.index);
            item->user_data(&fileIndices_.back());
            if (file.active) {
                tree_->select(item, 0);
                tree_->show_item(item);
            }
        }
    }
    tree_->redraw();
}

void FileTreePanel::setActivationHandler(std::function<void(std::size_t)> handler) { activationHandler_ = std::move(handler); }

void FileTreePanel::resize(int x, int y, int width, int height) {
    Fl_Group::resize(x, y, width, height);
    tree_->resize(x + Padding, y + Padding, width - 2 * Padding, height - 2 * Padding);
}

void FileTreePanel::treeCallback(Fl_Widget *, void *data) {
    auto &panel = *static_cast<FileTreePanel *>(data);
    auto *item = panel.tree_->callback_item();
    if (item != nullptr && item->user_data() != nullptr && panel.activationHandler_) {
        panel.activationHandler_(*static_cast<std::size_t *>(item->user_data()));
    }
}
