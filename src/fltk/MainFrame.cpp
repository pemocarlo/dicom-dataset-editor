#include "MainFrame.hpp"

#include "AttributeEditDialog.hpp"
#include "DatasetTreePanel.hpp"
#include "dicom_editor/DicomNode.hpp"
#include "dicom_editor/DicomPath.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_ask.H>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
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
};

MenuAction openAction = MenuAction::Open;
MenuAction saveAction = MenuAction::Save;
MenuAction saveAsAction = MenuAction::SaveAs;
MenuAction exitAction = MenuAction::Exit;
MenuAction editAction = MenuAction::Edit;
MenuAction addAction = MenuAction::Add;
MenuAction deleteAction = MenuAction::Delete;

std::string documentTitle(const dicom_editor::DicomDocument &document) {
    const std::string name = document.hasFilePath() ? document.filePath().filename().string() : "Untitled";
    return "DICOM Dataset Editor - " + name + (document.dirty() ? "*" : "");
}

std::filesystem::path chooseFile(Fl_Native_File_Chooser::Type type, const char *title) {
    Fl_Native_File_Chooser chooser(type);
    chooser.title(title);
    chooser.filter("DICOM files\t*.dcm\nAll files\t*");
    if (chooser.show() != 0) {
        return {};
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

MainFrame::MainFrame() : Fl_Double_Window(1180, 760, "DICOM Dataset Editor") {
    menu_ = new Fl_Menu_Bar(0, 0, w(), MenuHeight);
    menu_->add("&File/&Open...", FL_CTRL + 'o', menuCallback, &openAction);
    menu_->add("&File/&Save", FL_CTRL + 's', menuCallback, &saveAction);
    menu_->add("&File/Save &As...", FL_CTRL + FL_SHIFT + 's', menuCallback, &saveAsAction);
    menu_->add("&File/E&xit", 0, menuCallback, &exitAction);
    menu_->add("&Edit/&Edit Value...", FL_Enter, menuCallback, &editAction);
    menu_->add("&Edit/&Add Attribute...", FL_CTRL + 'n', menuCallback, &addAction);
    menu_->add("&Edit/&Delete Attribute", FL_Delete, menuCallback, &deleteAction);

    datasetPanel_ = new DatasetTreePanel(0, MenuHeight, w(), h() - MenuHeight - StatusHeight);
    datasetPanel_->SetSelectionChangedHandler([this] { UpdateActions(); });
    datasetPanel_->SetValueChangedHandler([this](const dicom_editor::DicomPath &path, const std::string &value) {
        const auto *selected = datasetPanel_->SelectedNode();
        const std::string title = selected == nullptr ? "Edit Value" : "Edit " + selected->keyword;
        const auto result = AttributeEditDialog::Edit(title, value);
        if (result) {
            EditValue(path, result->value);
        }
    });

    status_ = new Fl_Box(6, h() - StatusHeight, w() - 12, StatusHeight);
    status_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    resizable(datasetPanel_);
    callback(closeCallback, this);
    end();

    RefreshDataset();
    datasetPanel_->FocusRows();
}

int MainFrame::handle(int event) {
    if ((event == FL_KEYDOWN || event == FL_SHORTCUT) && Fl::event_key() == FL_Escape) {
        datasetPanel_->FocusRows();
        return 1;
    }
    return Fl_Double_Window::handle(event);
}

void MainFrame::RefreshDataset() {
    datasetPanel_->SetNodes(document_.nodes());
    copy_label(documentTitle(document_).c_str());
    status_->copy_label(document_.hasFilePath() ? document_.filePath().string().c_str() : "New dataset");
    UpdateActions();
}

bool MainFrame::ConfirmDiscardChanges() {
    if (!document_.dirty()) {
        return true;
    }
    const int answer = fl_choice("Save changes before continuing?", "Cancel", "Don't Save", "Save");
    return answer == 1 || (answer == 2 && SaveCurrent());
}

bool MainFrame::SaveCurrent() {
    try {
        if (!document_.hasFilePath()) {
            return SaveAs();
        }
        document_.save();
        dicom_editor::DicomDocument verified;
        verified.load(document_.filePath());
        RefreshDataset();
        status_->copy_label(("Saved and reloaded successfully: " + document_.filePath().string()).c_str());
        return true;
    } catch (const std::exception &error) {
        ShowError(error.what());
        return false;
    }
}

bool MainFrame::SaveAs() {
    const auto path = chooseFile(Fl_Native_File_Chooser::BROWSE_SAVE_FILE, "Save DICOM File");
    if (path.empty()) {
        return false;
    }

    try {
        document_.saveAs(path);
        dicom_editor::DicomDocument verified;
        verified.load(document_.filePath());
        RefreshDataset();
        status_->copy_label(("Saved and reloaded successfully: " + document_.filePath().string()).c_str());
        return true;
    } catch (const std::exception &error) {
        ShowError(error.what());
        return false;
    }
}

void MainFrame::EditSelectedValue() { datasetPanel_->EditSelectedValue(); }

void MainFrame::EditValue(const dicom_editor::DicomPath &path, const std::string &value) {
    try {
        editor_.editValue(document_, {.path = path, .value = value});
        RefreshDataset();
    } catch (const std::exception &error) {
        ShowError(error.what());
        RefreshDataset();
    }
}

void MainFrame::AddAttribute() {
    const auto *selected = datasetPanel_->SelectedNode();
    dicom_editor::DicomPath parent = dicom_editor::DicomPath::dataset();
    if (selected != nullptr) {
        parent =
            selected->kind == dicom_editor::DicomNodeKind::Item ? selected->path : dicom_editor::DicomPath::item(selected->path.parents());
    }

    const auto result = AttributeEditDialog::Add();
    if (!result || !result->tag) {
        return;
    }

    try {
        editor_.addAttribute(document_, {.parentItemPath = parent, .tag = *result->tag, .value = result->value});
        RefreshDataset();
    } catch (const std::exception &error) {
        ShowError(error.what());
    }
}

void MainFrame::DeleteAttribute() {
    const auto *selected = datasetPanel_->SelectedNode();
    if (selected == nullptr || !selected->editable || fl_choice("Delete selected attribute?", "Cancel", "Delete", nullptr) != 1) {
        return;
    }

    try {
        editor_.deleteAttribute(document_, selected->path);
        RefreshDataset();
    } catch (const std::exception &error) {
        ShowError(error.what());
    }
}

void MainFrame::ShowError(const std::string &message) { fl_alert("%s", message.c_str()); }

void MainFrame::UpdateActions() {
    const auto *selected = datasetPanel_->SelectedNode();
    const bool editable = selected != nullptr && selected->editable;
    setMenuActive(*menu_, "&Edit/&Edit Value...", editable);
    setMenuActive(*menu_, "&Edit/&Delete Attribute", editable);
    setMenuActive(*menu_, "&File/&Save", document_.dirty() || !document_.hasFilePath());
}

void MainFrame::Exit() {
    if (ConfirmDiscardChanges()) {
        hide();
    }
}

void MainFrame::menuCallback(Fl_Widget *widget, void *data) {
    auto *frame = static_cast<MainFrame *>(widget->window());
    const auto action = *static_cast<MenuAction *>(data);

    switch (action) {
    case MenuAction::Open: {
        if (!frame->ConfirmDiscardChanges()) {
            return;
        }
        const auto file = chooseFile(Fl_Native_File_Chooser::BROWSE_FILE, "Open DICOM File");
        if (!file.empty()) {
            try {
                frame->document_.load(file);
                frame->RefreshDataset();
            } catch (const std::exception &error) {
                frame->ShowError(error.what());
            }
        }
        break;
    }
    case MenuAction::Save:
        frame->SaveCurrent();
        break;
    case MenuAction::SaveAs:
        frame->SaveAs();
        break;
    case MenuAction::Exit:
        frame->Exit();
        break;
    case MenuAction::Edit:
        frame->EditSelectedValue();
        break;
    case MenuAction::Add:
        frame->AddAttribute();
        break;
    case MenuAction::Delete:
        frame->DeleteAttribute();
        break;
    }
}

void MainFrame::closeCallback(Fl_Widget *, void *data) { static_cast<MainFrame *>(data)->Exit(); }
