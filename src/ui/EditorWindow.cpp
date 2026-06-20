#include "EditorWindow.hpp"

#include "AttributeDialog.hpp"
#include "DatasetPanel.hpp"
#include "PixelDataPanel.hpp"
#include "dicom_editor/AttributeInput.hpp"
#include "dicom_editor/DicomDocument.hpp"
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
#include <FL/fl_draw.H>

#include <algorithm>
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
constexpr int PixelDataSplitterHeight = 6;
constexpr int PixelDataPanelMinHeight = 180;

enum class MenuAction : std::uint8_t {
    Open,
    Save,
    SaveAs,
    Exit,
    Edit,
    Add,
    Delete,
    ValidateValues,
    PixelDataPreview,
    PixelDataPreviewVertical,
};

MenuAction openAction = MenuAction::Open;
MenuAction saveAction = MenuAction::Save;
MenuAction saveAsAction = MenuAction::SaveAs;
MenuAction exitAction = MenuAction::Exit;
MenuAction editAction = MenuAction::Edit;
MenuAction addAction = MenuAction::Add;
MenuAction deleteAction = MenuAction::Delete;
MenuAction validateValuesAction = MenuAction::ValidateValues;
MenuAction pixelDataPreviewAction = MenuAction::PixelDataPreview;
MenuAction pixelDataPreviewVerticalAction = MenuAction::PixelDataPreviewVertical;

class PixelSplitter final : public Fl_Widget {
  public:
    explicit PixelSplitter(int x, int y, int width, int height, EditorWindow &owner)
        : Fl_Widget(x, y, width, height), owner_(owner) {}

  private:
    int handle(int event) override {
        const bool vertical = owner_.pixelDataPreviewVertical();
        switch (event) {
        case FL_PUSH:
            if (Fl::event_button() == FL_LEFT_MOUSE) {
                dragging_ = true;
                dragOffset_ = vertical ? Fl::event_x() - x() : Fl::event_y() - y();
                return 1;
            }
            break;
        case FL_DRAG:
            if (dragging_) {
                if (vertical) {
                    const int splitX = Fl::event_x() - dragOffset_;
                    const int previewWidth = owner_.w() - splitX - PixelDataSplitterHeight;
                    owner_.setPixelDataPanelExtent(previewWidth);
                } else {
                    const int splitY = Fl::event_y() - dragOffset_;
                    const int previewHeight = owner_.h() - StatusHeight - (splitY + PixelDataSplitterHeight);
                    owner_.setPixelDataPanelExtent(previewHeight);
                }
                return 1;
            }
            break;
        case FL_RELEASE:
            if (dragging_) {
                dragging_ = false;
                return 1;
            }
            break;
        default:
            break;
        }
        return Fl_Widget::handle(event);
    }

    void draw() override {
        fl_color(fl_rgb_color(190, 197, 205));
        fl_rectf(x(), y(), w(), h());
        fl_color(fl_rgb_color(150, 158, 166));
        const bool vertical = owner_.pixelDataPreviewVertical();
        if (vertical) {
            fl_rectf(x() + w() / 2 - 1, y() + h() / 2 - 18, 2, 36);
            fl_rectf(x() + w() / 2 - 5, y() + h() / 2 - 18, 2, 36);
            fl_rectf(x() + w() / 2 + 3, y() + h() / 2 - 18, 2, 36);
        } else {
            fl_rectf(x() + w() / 2 - 18, y() + h() / 2 - 1, 36, 2);
            fl_rectf(x() + w() / 2 - 18, y() + h() / 2 - 5, 36, 2);
            fl_rectf(x() + w() / 2 - 18, y() + h() / 2 + 3, 36, 2);
        }
    }

    EditorWindow &owner_;
    bool dragging_{};
    int dragOffset_{};
};

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

EditorWindow::EditorWindow() : Fl_Double_Window(920, 700, "DICOM Dataset Editor"), controller_(*this) {
    menu_ = new Fl_Menu_Bar(0, 0, w(), MenuHeight);
    menu_->add("&File/&Open...", FL_CTRL + 'o', menuCallback, &openAction);
    menu_->add("&File/&Save", FL_CTRL + 's', menuCallback, &saveAction);
    menu_->add("&File/Save &As...", FL_CTRL + FL_SHIFT + 's', menuCallback, &saveAsAction);
    menu_->add("&File/E&xit", 0, menuCallback, &exitAction);
    menu_->add("&Edit/&Edit Value...", FL_Enter, menuCallback, &editAction);
    menu_->add("&Edit/&Add Attribute...", FL_CTRL + 'n', menuCallback, &addAction);
    menu_->add("&Edit/&Delete Attribute", FL_Delete, menuCallback, &deleteAction);
    menu_->add("&Settings/&Validate DICOM Values", 0, menuCallback, &validateValuesAction, FL_MENU_TOGGLE | FL_MENU_VALUE);
    menu_->add("&View/&Pixel Data Preview", 0, menuCallback, &pixelDataPreviewAction, FL_MENU_TOGGLE);
    menu_->add("&View/Pixel Data Preview on &Right", 0, menuCallback, &pixelDataPreviewVerticalAction, FL_MENU_TOGGLE);

    datasetPanel_ = new DatasetPanel(0, MenuHeight, w(), h() - MenuHeight - StatusHeight);
    datasetPanel_->setSelectionChangedHandler([this] { updateActions(); });
    datasetPanel_->setEditRequestedHandler([this] { controller_.editSelected(datasetPanel_->selectedNode()); });

    pixelSplitter_ = new PixelSplitter(0, 0, 1, PixelDataSplitterHeight, *this);
    pixelSplitter_->hide();

    pixelDataPanel_ = new PixelDataPanel(0, 0, 1, 1);
    pixelDataPanel_->setPreviousHandler([this] { controller_.showPreviousPixelFrame(); });
    pixelDataPanel_->setNextHandler([this] { controller_.showNextPixelFrame(); });
    pixelDataPanel_->hide();

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

void EditorWindow::presentPixelData(std::optional<dicom_editor::PixelDataPreview> preview) {
    if (preview) {
        pixelDataPanel_->setPreview(std::move(*preview));
        pixelDataPanel_->show();
        pixelSplitter_->show();
    } else {
        pixelDataPanel_->hide();
        pixelSplitter_->hide();
    }
    layoutContent();
}

void EditorWindow::setStatus(const std::string &status) { status_->copy_label(status.c_str()); }

void EditorWindow::resize(int x, int y, int width, int height) {
    Fl_Double_Window::resize(x, y, width, height);
    if (menu_ != nullptr && datasetPanel_ != nullptr && pixelDataPanel_ != nullptr && status_ != nullptr) {
        layoutContent();
    }
}

void EditorWindow::setPixelDataPanelExtent(int extent) {
    const int contentExtent = pixelDataPreviewVertical_ ? w() : h() - MenuHeight - StatusHeight;
    const int maximumPreviewExtent = std::max(PixelDataPanelMinHeight, contentExtent - PixelDataSplitterHeight - PixelDataPanelMinHeight);
    const int clamped = std::clamp(extent, PixelDataPanelMinHeight, maximumPreviewExtent);
    if (pixelDataPanelExtent_ != clamped) {
        pixelDataPanelExtent_ = clamped;
        layoutContent();
    }
}

void EditorWindow::setPixelDataPreviewVertical(bool vertical) {
    if (pixelDataPreviewVertical_ != vertical) {
        pixelDataPreviewVertical_ = vertical;
        layoutContent();
    }
}

bool EditorWindow::pixelDataPreviewVertical() const { return pixelDataPreviewVertical_; }

void EditorWindow::layoutContent() {
    menu_->resize(0, 0, w(), MenuHeight);
    status_->resize(6, h() - StatusHeight, w() - 12, StatusHeight);

    const int contentHeight = h() - MenuHeight - StatusHeight;
    if (pixelDataPanel_->visible() != 0) {
        if (pixelDataPreviewVertical_) {
            const int maxPreviewWidth =
                std::max(PixelDataPanelMinHeight, w() - PixelDataSplitterHeight - PixelDataPanelMinHeight);
            const int previewWidth = std::clamp(pixelDataPanelExtent_, PixelDataPanelMinHeight, maxPreviewWidth);
            const int datasetWidth = std::max(1, w() - previewWidth - PixelDataSplitterHeight);
            datasetPanel_->resize(0, MenuHeight, datasetWidth, contentHeight);
            pixelSplitter_->resize(datasetWidth, MenuHeight, PixelDataSplitterHeight, contentHeight);
            pixelDataPanel_->resize(datasetWidth + PixelDataSplitterHeight, MenuHeight, previewWidth, contentHeight);
        } else {
            const int maxPreviewHeight =
                std::max(PixelDataPanelMinHeight, contentHeight - PixelDataSplitterHeight - PixelDataPanelMinHeight);
            const int previewHeight = std::clamp(pixelDataPanelExtent_, PixelDataPanelMinHeight, maxPreviewHeight);
            const int datasetHeight = std::max(1, contentHeight - previewHeight - PixelDataSplitterHeight);
            datasetPanel_->resize(0, MenuHeight, w(), datasetHeight);
            pixelSplitter_->resize(0, MenuHeight + datasetHeight, w(), PixelDataSplitterHeight);
            pixelDataPanel_->resize(6, MenuHeight + datasetHeight + PixelDataSplitterHeight, w() - 12, previewHeight);
        }
    } else {
        datasetPanel_->resize(0, MenuHeight, w(), contentHeight);
        pixelSplitter_->hide();
    }
    redraw();
}

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
    case MenuAction::PixelDataPreview:
        window->controller_.setPixelDataVisible(window->menu_->mvalue()->value() != 0);
        break;
    case MenuAction::PixelDataPreviewVertical:
        window->setPixelDataPreviewVertical(window->menu_->mvalue()->value() != 0);
        break;
    }
}

void EditorWindow::closeCallback(Fl_Widget *, void *data) { static_cast<EditorWindow *>(data)->exit(); }
