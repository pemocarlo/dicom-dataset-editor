#include "EditorWindow.hpp"

#include "AttributeDialog.hpp"
#include "DatasetPanel.hpp"
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
    ValidateValues,
};

MenuAction openAction = MenuAction::Open;
MenuAction saveAction = MenuAction::Save;
MenuAction saveAsAction = MenuAction::SaveAs;
MenuAction exitAction = MenuAction::Exit;
MenuAction editAction = MenuAction::Edit;
MenuAction addAction = MenuAction::Add;
MenuAction deleteAction = MenuAction::Delete;
MenuAction validateValuesAction = MenuAction::ValidateValues;

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

EditorWindow::EditorWindow() : Fl_Double_Window(920, 640, "DICOM Dataset Editor"), controller_(*this) {
    menu_ = new Fl_Menu_Bar(0, 0, w(), MenuHeight);
    menu_->add("&File/&Open...", FL_CTRL + 'o', menuCallback, &openAction);
    menu_->add("&File/&Save", FL_CTRL + 's', menuCallback, &saveAction);
    menu_->add("&File/Save &As...", FL_CTRL + FL_SHIFT + 's', menuCallback, &saveAsAction);
    menu_->add("&File/E&xit", 0, menuCallback, &exitAction);
    menu_->add("&Edit/&Edit Value...", FL_Enter, menuCallback, &editAction);
    menu_->add("&Edit/&Add Attribute...", FL_CTRL + 'n', menuCallback, &addAction);
    menu_->add("&Edit/&Delete Attribute", FL_Delete, menuCallback, &deleteAction);
    menu_->add("&Settings/&Validate DICOM Values", 0, menuCallback, &validateValuesAction, FL_MENU_TOGGLE | FL_MENU_VALUE);

    datasetPanel_ = new DatasetPanel(0, MenuHeight, w(), h() - MenuHeight - StatusHeight);
    datasetPanel_->setSelectionChangedHandler([this] { updateActions(); });
    datasetPanel_->setEditRequestedHandler([this] { controller_.editSelected(datasetPanel_->selectedNode()); });

    status_ = new Fl_Box(6, h() - StatusHeight, w() - 12, StatusHeight);
    status_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    resizable(datasetPanel_);
    callback(closeCallback, this);
    end();

    controller_.refreshView();
    datasetPanel_->focusRows();
}

int EditorWindow::handle(int event) {
    if ((event == FL_KEYDOWN || event == FL_SHORTCUT) && Fl::event_key() == FL_Escape) {
        datasetPanel_->focusRows();
        return 1;
    }
    return Fl_Double_Window::handle(event);
}

std::optional<std::filesystem::path> EditorWindow::chooseOpenFile() {
    return chooseFile(Fl_Native_File_Chooser::BROWSE_FILE, "Open DICOM File");
}

std::optional<std::filesystem::path> EditorWindow::chooseSaveFile() {
    return chooseFile(Fl_Native_File_Chooser::BROWSE_SAVE_FILE, "Save DICOM File");
}

dicom_editor::SaveChangesChoice EditorWindow::confirmSaveChanges() {
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

bool EditorWindow::confirmDelete() { return fl_choice("Delete selected attribute?", "Cancel", "Delete", nullptr) == 1; }

std::optional<dicom_editor::AttributeInput> EditorWindow::editAttribute(const std::string &title, const std::string &value) {
    return AttributeDialog::edit(title, value);
}

std::optional<dicom_editor::AttributeInput> EditorWindow::addAttribute() { return AttributeDialog::add(); }

void EditorWindow::showError(const std::string &message) { fl_alert("%s", message.c_str()); }

void EditorWindow::presentDocument(std::vector<dicom_editor::DicomNode> nodes, const std::string &title, const std::string &status) {
    datasetPanel_->setNodes(std::move(nodes));
    copy_label(title.c_str());
    setStatus(status);
    updateActions();
}

void EditorWindow::setStatus(const std::string &status) { status_->copy_label(status.c_str()); }

void EditorWindow::updateActions() {
    const auto actions = controller_.actionState(datasetPanel_->selectedNode());
    setMenuActive(*menu_, "&Edit/&Edit Value...", actions.editEnabled);
    setMenuActive(*menu_, "&Edit/&Delete Attribute", actions.deleteEnabled);
    setMenuActive(*menu_, "&File/&Save", actions.saveEnabled);
}

void EditorWindow::exit() {
    if (controller_.confirmClose()) {
        hide();
    }
}

void EditorWindow::menuCallback(Fl_Widget *widget, void *data) {
    auto *window = static_cast<EditorWindow *>(widget->window());
    const auto action = *static_cast<MenuAction *>(data);

    switch (action) {
    case MenuAction::Open:
        window->controller_.openDocument();
        break;
    case MenuAction::Save:
        window->controller_.saveDocument();
        break;
    case MenuAction::SaveAs:
        window->controller_.saveDocumentAs();
        break;
    case MenuAction::Exit:
        window->exit();
        break;
    case MenuAction::Edit:
        window->controller_.editSelected(window->datasetPanel_->selectedNode());
        break;
    case MenuAction::Add:
        window->controller_.addAttribute(window->datasetPanel_->selectedNode());
        break;
    case MenuAction::Delete:
        window->controller_.deleteAttribute(window->datasetPanel_->selectedNode());
        break;
    case MenuAction::ValidateValues:
        window->controller_.setValidationEnabled(window->menu_->mvalue()->value() != 0);
        break;
    }
}

void EditorWindow::closeCallback(Fl_Widget *, void *data) { static_cast<EditorWindow *>(data)->exit(); }
