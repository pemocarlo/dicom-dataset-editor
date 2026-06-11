#include "MainFrame.hpp"

#include "AttributeEditDialog.hpp"
#include "DatasetTreePanel.hpp"
#include "dicom_editor/AttributeInput.hpp"
#include "dicom_editor/DicomNode.hpp"
#include "dicom_editor/EditorController.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_ask.H>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dicom_editor {
class DicomPath;
}

namespace {

constexpr int MenuHeight = 28;
constexpr int StatusHeight = 26;

enum class MenuAction : std::uint8_t {
    Open,
    Save,
    SaveAs,
    Exit,
    Edit,
    Add,
    Delete,
};

MenuAction openAction = MenuAction::Open;
MenuAction saveAction = MenuAction::Save;
MenuAction saveAsAction = MenuAction::SaveAs;
MenuAction exitAction = MenuAction::Exit;
MenuAction editAction = MenuAction::Edit;
MenuAction addAction = MenuAction::Add;
MenuAction deleteAction = MenuAction::Delete;

std::optional<std::filesystem::path> chooseFile(Fl_Native_File_Chooser::Type type, const char *title) {
    Fl_Native_File_Chooser chooser(type);
    chooser.title(title);
    chooser.filter("DICOM files\t*.dcm\nAll files\t*");
    if (chooser.show() != 0) {
        return std::nullopt;
    }
    return chooser.filename();
}

void setMenuActive(Fl_Menu_Bar &menu, const char *path, bool active) {
    const int index = menu.find_index(path);
    if (index < 0) {
        return;
    }
    const int flags = menu.mode(index);
    menu.mode(index, active ? flags & ~FL_MENU_INACTIVE : flags | FL_MENU_INACTIVE);
}

} // namespace

MainFrame::MainFrame() : Fl_Double_Window(1180, 760, "DICOM Dataset Editor"), controller_(*this) {
    menu_ = new Fl_Menu_Bar(0, 0, w(), MenuHeight);
    menu_->add("&File/&Open...", FL_CTRL + 'o', menuCallback, &openAction);
    menu_->add("&File/&Save", FL_CTRL + 's', menuCallback, &saveAction);
    menu_->add("&File/Save &As...", FL_CTRL + FL_SHIFT + 's', menuCallback, &saveAsAction);
    menu_->add("&File/E&xit", 0, menuCallback, &exitAction);
    menu_->add("&Edit/&Edit Value...", FL_Enter, menuCallback, &editAction);
    menu_->add("&Edit/&Add Attribute...", FL_CTRL + 'n', menuCallback, &addAction);
    menu_->add("&Edit/&Delete Attribute", FL_Delete, menuCallback, &deleteAction);

    datasetPanel_ = new DatasetTreePanel(0, MenuHeight, w(), h() - MenuHeight - StatusHeight);
    datasetPanel_->SetSelectionChangedHandler([this] { updateActions(); });
    datasetPanel_->SetValueChangedHandler(
        [this](const dicom_editor::DicomPath &, const std::string &) { controller_.editSelected(datasetPanel_->SelectedNode()); });

    status_ = new Fl_Box(6, h() - StatusHeight, w() - 12, StatusHeight);
    status_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    resizable(datasetPanel_);
    callback(closeCallback, this);
    end();

    controller_.refresh();
    datasetPanel_->FocusRows();
}

int MainFrame::handle(int event) {
    if ((event == FL_KEYDOWN || event == FL_SHORTCUT) && Fl::event_key() == FL_Escape) {
        datasetPanel_->FocusRows();
        return 1;
    }
    return Fl_Double_Window::handle(event);
}

std::optional<std::filesystem::path> MainFrame::chooseOpenFile() {
    return chooseFile(Fl_Native_File_Chooser::BROWSE_FILE, "Open DICOM File");
}

std::optional<std::filesystem::path> MainFrame::chooseSaveFile() {
    return chooseFile(Fl_Native_File_Chooser::BROWSE_SAVE_FILE, "Save DICOM File");
}

dicom_editor::SaveChangesChoice MainFrame::confirmSaveChanges() {
    const int answer = fl_choice("Save changes before continuing?", "Cancel", "Don't Save", "Save");
    switch (answer) {
    case 1:
        return dicom_editor::SaveChangesChoice::Discard;
    case 2:
        return dicom_editor::SaveChangesChoice::Save;
    default:
        return dicom_editor::SaveChangesChoice::Cancel;
    }
}

bool MainFrame::confirmDelete() { return fl_choice("Delete selected attribute?", "Cancel", "Delete", nullptr) == 1; }

std::optional<dicom_editor::AttributeInput> MainFrame::editAttribute(const std::string &title, const std::string &value) {
    return AttributeEditDialog::Edit(title, value);
}

std::optional<dicom_editor::AttributeInput> MainFrame::addAttribute() { return AttributeEditDialog::Add(); }

void MainFrame::showError(const std::string &message) { fl_alert("%s", message.c_str()); }

void MainFrame::presentDocument(std::vector<dicom_editor::DicomNode> nodes, const std::string &title, const std::string &status) {
    datasetPanel_->SetNodes(std::move(nodes));
    copy_label(title.c_str());
    setStatus(status);
    updateActions();
}

void MainFrame::setStatus(const std::string &status) { status_->copy_label(status.c_str()); }

void MainFrame::updateActions() {
    const auto actions = controller_.actionState(datasetPanel_->SelectedNode());
    setMenuActive(*menu_, "&Edit/&Edit Value...", actions.editEnabled);
    setMenuActive(*menu_, "&Edit/&Delete Attribute", actions.deleteEnabled);
    setMenuActive(*menu_, "&File/&Save", actions.saveEnabled);
}

void MainFrame::exit() {
    if (controller_.confirmClose()) {
        hide();
    }
}

void MainFrame::menuCallback(Fl_Widget *widget, void *data) {
    auto *frame = static_cast<MainFrame *>(widget->window());
    const auto action = *static_cast<MenuAction *>(data);

    switch (action) {
    case MenuAction::Open:
        frame->controller_.open();
        break;
    case MenuAction::Save:
        frame->controller_.save();
        break;
    case MenuAction::SaveAs:
        frame->controller_.saveAs();
        break;
    case MenuAction::Exit:
        frame->exit();
        break;
    case MenuAction::Edit:
        frame->controller_.editSelected(frame->datasetPanel_->SelectedNode());
        break;
    case MenuAction::Add:
        frame->controller_.add(frame->datasetPanel_->SelectedNode());
        break;
    case MenuAction::Delete:
        frame->controller_.remove(frame->datasetPanel_->SelectedNode());
        break;
    }
}

void MainFrame::closeCallback(Fl_Widget *, void *data) { static_cast<MainFrame *>(data)->exit(); }
