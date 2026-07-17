#include "EditorWindow.hpp"

#include "DatasetPanel.hpp"
#include "FileTreePanel.hpp"
#include "PixelDataPanel.hpp"
#include "dicom_editor/application/EditorController.hpp"
#include "dicom_editor/core/DicomDocument.hpp"
#include "dicom_editor/core/DicomNode.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int MenuHeight = 28;
constexpr int ToolbarHeight = 48;
constexpr int ContentTop = MenuHeight + ToolbarHeight;
constexpr int StatusHeight = 30;
constexpr int PixelDataSplitterHeight = 6;
constexpr int PixelDataPanelMinExtent = 260;
constexpr int FileTreeSplitterWidth = 6;
constexpr int FileTreePanelMinWidth = 180;
constexpr int EditorPanelMinWidth = 480;

enum class MenuAction : std::uint8_t {
    OpenFiles,
    OpenFolder,
    OpenDicomDirectory,
    Save,
    SaveAs,
    SaveAll,
    ClearWorkspace,
    Exit,
    Edit,
    Add,
    Delete,
    ValidateValues,
    LoadDataDictionary,
    PixelDataPreview,
    PixelDataPreviewVertical,
    OpenFilesPanel,
    SortFilesByFilename,
    PreviousFile,
    NextFile,
    ZoomIn,
    ZoomOut,
    ZoomReset,
};

MenuAction openFilesAction = MenuAction::OpenFiles;
MenuAction openFolderAction = MenuAction::OpenFolder;
MenuAction openDicomDirectoryAction = MenuAction::OpenDicomDirectory;
MenuAction saveAction = MenuAction::Save;
MenuAction saveAsAction = MenuAction::SaveAs;
MenuAction saveAllAction = MenuAction::SaveAll;
MenuAction clearWorkspaceAction = MenuAction::ClearWorkspace;
MenuAction exitAction = MenuAction::Exit;
MenuAction editAction = MenuAction::Edit;
MenuAction addAction = MenuAction::Add;
MenuAction deleteAction = MenuAction::Delete;
MenuAction validateValuesAction = MenuAction::ValidateValues;
MenuAction loadDataDictionaryAction = MenuAction::LoadDataDictionary;
MenuAction pixelDataPreviewAction = MenuAction::PixelDataPreview;
MenuAction pixelDataPreviewVerticalAction = MenuAction::PixelDataPreviewVertical;
MenuAction openFilesPanelAction = MenuAction::OpenFilesPanel;
MenuAction sortFilesByFilenameAction = MenuAction::SortFilesByFilename;
MenuAction previousFileAction = MenuAction::PreviousFile;
MenuAction nextFileAction = MenuAction::NextFile;
MenuAction zoomInAction = MenuAction::ZoomIn;
MenuAction zoomOutAction = MenuAction::ZoomOut;
MenuAction zoomResetAction = MenuAction::ZoomReset;

class PixelSplitter final : public Fl_Widget {
  public:
    explicit PixelSplitter(int x, int y, int width, int height, EditorWindow &owner) : Fl_Widget(x, y, width, height), owner_(owner) {}

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

class FileTreeSplitter final : public Fl_Widget {
  public:
    explicit FileTreeSplitter(int x, int y, int width, int height, EditorWindow &owner) : Fl_Widget(x, y, width, height), owner_(owner) {}

  private:
    int handle(int event) override {
        switch (event) {
        case FL_PUSH:
            if (Fl::event_button() == FL_LEFT_MOUSE) {
                dragging_ = true;
                dragOffset_ = Fl::event_x() - x();
                return 1;
            }
            break;
        case FL_DRAG:
            if (dragging_) {
                owner_.setFileTreePanelExtent(Fl::event_x() - dragOffset_);
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
        fl_rectf(x() + w() / 2 - 1, y() + h() / 2 - 18, 2, 36);
    }

    EditorWindow &owner_;
    bool dragging_{};
    int dragOffset_{};
};

void setMenuActive(Fl_Menu_Bar &menu, const char *path, bool active) {
    const int index = menu.find_index(path);
    if (index < 0) {
        return;
    }
    const int flags = menu.mode(index);
    menu.mode(index, active ? flags & ~FL_MENU_INACTIVE : flags | FL_MENU_INACTIVE);
}

void setMenuChecked(Fl_Menu_Bar &menu, const char *path, bool checked) {
    const int index = menu.find_index(path);
    if (index < 0) {
        return;
    }
    const int flags = menu.mode(index);
    menu.mode(index, checked ? flags | FL_MENU_VALUE : flags & ~FL_MENU_VALUE);
}

Fl_Button *makeToolbarButton(int x, int width, const char *label, MenuAction &action) {
    auto *button = new Fl_Button(x, MenuHeight + 8, width, ToolbarHeight - 16, label);
    button->box(FL_FLAT_BOX);
    button->color(fl_rgb_color(229, 236, 243));
    button->selection_color(fl_rgb_color(190, 216, 239));
    button->labelcolor(fl_rgb_color(31, 55, 74));
    button->labelfont(FL_HELVETICA_BOLD);
    button->callback(EditorWindow::menuCallback, &action);
    return button;
}

void setWidgetActive(Fl_Widget &widget, bool active) { active ? widget.activate() : widget.deactivate(); }

} // namespace

EditorWindow::EditorWindow() : Fl_Double_Window(1280, 820, "DICOM Dataset Editor"), controller_(*this) {
    menu_ = new Fl_Menu_Bar(0, 0, w(), MenuHeight);
    menu_->box(FL_FLAT_BOX);
    menu_->color(fl_rgb_color(35, 57, 75));
    menu_->textcolor(FL_WHITE);
    menu_->add("&File/&Open Files...", FL_CTRL + 'o', menuCallback, &openFilesAction);
    menu_->add("&File/Open &Folder...", FL_CTRL + FL_SHIFT + 'o', menuCallback, &openFolderAction);
    menu_->add("&File/Open &DICOMDIR...", 0, menuCallback, &openDicomDirectoryAction);
    menu_->add("&File/&Save", FL_CTRL + 's', menuCallback, &saveAction);
    menu_->add("&File/Save &As...", FL_CTRL + FL_SHIFT + 's', menuCallback, &saveAsAction);
    menu_->add("&File/Save A&ll", FL_CTRL + FL_ALT + 's', menuCallback, &saveAllAction);
    menu_->add("&File/&Clear Workspace", FL_CTRL + 'w', menuCallback, &clearWorkspaceAction);
    menu_->add("&File/E&xit", 0, menuCallback, &exitAction);
    menu_->add("&Edit/Edit or &View Value...", FL_Enter, menuCallback, &editAction);
    menu_->add("&Edit/&Add Attribute...", FL_CTRL + 'n', menuCallback, &addAction);
    menu_->add("&Edit/&Delete Attribute", FL_Delete, menuCallback, &deleteAction);
    menu_->add("&Settings/&Validate DICOM Values", 0, menuCallback, &validateValuesAction, FL_MENU_TOGGLE | FL_MENU_VALUE);
    menu_->add("&Settings/&Load Data Dictionary...", 0, menuCallback, &loadDataDictionaryAction);
    menu_->add("&View/&Pixel Data Preview", 0, menuCallback, &pixelDataPreviewAction, FL_MENU_TOGGLE);
    menu_->add("&View/Pixel Data Preview on &Right", 0, menuCallback, &pixelDataPreviewVerticalAction, FL_MENU_TOGGLE);
    menu_->add("&View/&Open Files Panel", 0, menuCallback, &openFilesPanelAction, FL_MENU_TOGGLE);
    menu_->add("&View/Sort Files by &Filename", 0, menuCallback, &sortFilesByFilenameAction, FL_MENU_TOGGLE);
    menu_->add("&View/&Previous File", FL_CTRL + FL_Page_Up, menuCallback, &previousFileAction);
    menu_->add("&View/&Next File", FL_CTRL + FL_Page_Down, menuCallback, &nextFileAction);
    menu_->add("&View/Zoom/Zoom &In", FL_CTRL + '+', menuCallback, &zoomInAction);
    menu_->add("&View/Zoom/Zoom &Out", FL_CTRL + '-', menuCallback, &zoomOutAction);
    menu_->add("&View/Zoom/&Reset", FL_CTRL + '0', menuCallback, &zoomResetAction);

    toolbar_ = new Fl_Box(0, MenuHeight, w(), ToolbarHeight);
    toolbar_->box(FL_FLAT_BOX);
    toolbar_->color(fl_rgb_color(245, 248, 251));
    openButton_ = makeToolbarButton(10, 104, "Open files", openFilesAction);
    folderButton_ = makeToolbarButton(120, 104, "Open folder", openFolderAction);
    saveButton_ = makeToolbarButton(238, 82, "Save", saveAction);
    addButton_ = makeToolbarButton(334, 82, "+ Add", addAction);
    editButton_ = makeToolbarButton(422, 82, "Edit", editAction);
    deleteButton_ = makeToolbarButton(510, 82, "Delete", deleteAction);
    previousFileButton_ = makeToolbarButton(610, 82, "< File", previousFileAction);
    nextFileButton_ = makeToolbarButton(698, 82, "File >", nextFileAction);
    pixelPreviewButton_ = new Fl_Toggle_Button(798, MenuHeight + 8, 104, ToolbarHeight - 16, "Preview");
    pixelPreviewButton_->box(FL_FLAT_BOX);
    pixelPreviewButton_->color(fl_rgb_color(229, 236, 243));
    pixelPreviewButton_->selection_color(fl_rgb_color(77, 139, 186));
    pixelPreviewButton_->labelcolor(fl_rgb_color(31, 55, 74));
    pixelPreviewButton_->labelfont(FL_HELVETICA_BOLD);
    pixelPreviewButton_->callback(menuCallback, &pixelDataPreviewAction);
    openButton_->tooltip("Open one or more DICOM files (Ctrl+O)");
    folderButton_->tooltip("Recursively open a folder (Ctrl+Shift+O)");
    saveButton_->tooltip("Save the active dataset (Ctrl+S)");
    addButton_->tooltip("Add an attribute (Ctrl+N)");
    editButton_->tooltip("Edit or inspect the selected value (Enter)");
    deleteButton_->tooltip("Delete the selected attribute (Delete)");
    previousFileButton_->tooltip("Show previous open dataset (Ctrl+Page Up)");
    nextFileButton_->tooltip("Show next open dataset (Ctrl+Page Down)");
    pixelPreviewButton_->tooltip("Show or hide pixel data preview");
    setMenuChecked(*menu_, "&View/Pixel Data Preview on &Right", true);

    fileTreePanel_ = new FileTreePanel(0, ContentTop, fileTreePanelExtent_, h() - ContentTop - StatusHeight);
    fileTreePanel_->setActivationHandler([this](std::size_t index) { controller_.activateDocument(index); });
    fileTreePanel_->setBatchEditHandler([this](const dicom_editor::BatchEditTarget &target) { controller_.batchEdit(target); });
    fileTreePanel_->hide();

    fileTreeSplitter_ = new FileTreeSplitter(0, ContentTop, FileTreeSplitterWidth, h() - ContentTop - StatusHeight, *this);
    fileTreeSplitter_->hide();

    datasetPanel_ = new DatasetPanel(0, ContentTop, w(), h() - ContentTop - StatusHeight);
    datasetPanel_->setSelectionChangedHandler([this] { updateActions(); });
    datasetPanel_->setEditRequestedHandler([this] { controller_.editSelected(datasetPanel_->selectedNode()); });

    pixelSplitter_ = new PixelSplitter(0, 0, 1, PixelDataSplitterHeight, *this);
    pixelSplitter_->hide();

    pixelDataPanel_ = new PixelDataPanel(0, 0, 1, 1);
    pixelDataPanel_->setPreviousHandler([this] { controller_.showPreviousPixelFrame(); });
    pixelDataPanel_->setNextHandler([this] { controller_.showNextPixelFrame(); });
    pixelDataPanel_->setPreviousFileHandler([this] { controller_.showPreviousDocument(); });
    pixelDataPanel_->setNextFileHandler([this] { controller_.showNextDocument(); });
    pixelDataPanel_->hide();

    status_ = new Fl_Box(6, h() - StatusHeight, w() - 12, StatusHeight);
    status_->box(FL_FLAT_BOX);
    status_->color(fl_rgb_color(35, 57, 75));
    status_->labelcolor(fl_rgb_color(232, 240, 246));
    status_->labelfont(FL_HELVETICA);
    status_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    resizable(datasetPanel_);
    callback(closeCallback, this);
    end();

    controller_.refreshView();
    setUiZoom(uiFontSize_);
    datasetPanel_->focusRows();
}

int EditorWindow::handle(int event) {
    if (event == FL_KEYDOWN || event == FL_SHORTCUT) {
        const int key = Fl::event_key();
        if (Fl::event_state(FL_CTRL) != 0) {
            if (key == '+' || key == '=' || key == FL_KP + '+') {
                setUiZoom(uiFontSize_ + 1);
                return 1;
            }
            if (key == '-' || key == FL_KP + '-') {
                setUiZoom(uiFontSize_ - 1);
                return 1;
            }
            if (key == '0' || key == FL_KP + '0') {
                setUiZoom(14);
                return 1;
            }
        }
        if (key != FL_Escape) {
            return Fl_Double_Window::handle(event);
        }
        datasetPanel_->focusRows();
        return 1;
    }
    return Fl_Double_Window::handle(event);
}

void EditorWindow::presentDocument(dicom_editor::DocumentPresentation presentation) {
    datasetPanel_->setNodes(std::move(presentation.nodes));
    copy_label(presentation.title.c_str());
    setStatus(presentation.status);
    updateActions();
}

void EditorWindow::presentOpenFiles(dicom_editor::OpenFilesPresentation presentation) {
    fileTreePanel_->setFiles(presentation.files);
    const auto active = std::ranges::find_if(presentation.files, [](const dicom_editor::OpenDicomFile &file) { return file.active; });
    setWidgetActive(*previousFileButton_, active != presentation.files.end() && active != presentation.files.begin());
    setWidgetActive(*nextFileButton_, active != presentation.files.end() && std::next(active) != presentation.files.end());
    if (presentation.hasLoadedFiles && !workspaceHadFiles_) {
        setFileTreeVisible(true);
    } else if (!presentation.hasLoadedFiles && workspaceHadFiles_) {
        setFileTreeVisible(false);
    }
    workspaceHadFiles_ = presentation.hasLoadedFiles;
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
    if (menu_ != nullptr && fileTreePanel_ != nullptr && fileTreeSplitter_ != nullptr && datasetPanel_ != nullptr &&
        pixelDataPanel_ != nullptr && status_ != nullptr) {
        layoutContent();
    }
}

void EditorWindow::setPixelDataPanelExtent(int extent) {
    const int fileTreeWidth = fileTreeVisible_ ? fileTreePanelExtent_ + FileTreeSplitterWidth : 0;
    const int contentExtent = pixelDataPreviewVertical_ ? w() - fileTreeWidth : h() - ContentTop - StatusHeight;
    const int maximumPreviewExtent = std::max(PixelDataPanelMinExtent, contentExtent - PixelDataSplitterHeight - PixelDataPanelMinExtent);
    const int clamped = std::clamp(extent, PixelDataPanelMinExtent, maximumPreviewExtent);
    if (pixelDataPanelExtent_ != clamped) {
        pixelDataPanelExtent_ = clamped;
        layoutContent();
    }
}

void EditorWindow::setFileTreePanelExtent(int extent) {
    const int maximumWidth = std::max(FileTreePanelMinWidth, w() - FileTreeSplitterWidth - EditorPanelMinWidth);
    const int clamped = std::clamp(extent, FileTreePanelMinWidth, maximumWidth);
    if (fileTreePanelExtent_ != clamped) {
        fileTreePanelExtent_ = clamped;
        layoutContent();
    }
}

void EditorWindow::setFileTreeVisible(bool visible) {
    if (fileTreeVisible_ == visible) {
        return;
    }
    fileTreeVisible_ = visible;
    if (visible) {
        fileTreePanel_->show();
        fileTreeSplitter_->show();
    } else {
        fileTreePanel_->hide();
        fileTreeSplitter_->hide();
    }
    setMenuChecked(*menu_, "&View/&Open Files Panel", visible);
    layoutContent();
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
    toolbar_->resize(0, MenuHeight, w(), ToolbarHeight);
    status_->resize(6, h() - StatusHeight, w() - 12, StatusHeight);

    const int contentHeight = h() - ContentTop - StatusHeight;
    const int fileTreeWidth = fileTreeVisible_
                                  ? std::clamp(fileTreePanelExtent_, FileTreePanelMinWidth,
                                               std::max(FileTreePanelMinWidth, w() - FileTreeSplitterWidth - EditorPanelMinWidth))
                                  : 0;
    const int editorX = fileTreeVisible_ ? fileTreeWidth + FileTreeSplitterWidth : 0;
    const int editorWidth = std::max(1, w() - editorX);
    if (fileTreeVisible_) {
        fileTreePanel_->resize(0, ContentTop, fileTreeWidth, contentHeight);
        fileTreeSplitter_->resize(fileTreeWidth, ContentTop, FileTreeSplitterWidth, contentHeight);
    }
    if (pixelDataPanel_->visible() != 0) {
        if (pixelDataPreviewVertical_) {
            const int maxPreviewWidth = std::max(PixelDataPanelMinExtent, editorWidth - PixelDataSplitterHeight - PixelDataPanelMinExtent);
            const int previewWidth = std::clamp(pixelDataPanelExtent_, PixelDataPanelMinExtent, maxPreviewWidth);
            const int datasetWidth = std::max(1, editorWidth - previewWidth - PixelDataSplitterHeight);
            datasetPanel_->resize(editorX, ContentTop, datasetWidth, contentHeight);
            pixelSplitter_->resize(editorX + datasetWidth, ContentTop, PixelDataSplitterHeight, contentHeight);
            pixelDataPanel_->resize(editorX + datasetWidth + PixelDataSplitterHeight, ContentTop, previewWidth, contentHeight);
        } else {
            const int maxPreviewHeight =
                std::max(PixelDataPanelMinExtent, contentHeight - PixelDataSplitterHeight - PixelDataPanelMinExtent);
            const int previewHeight = std::clamp(pixelDataPanelExtent_, PixelDataPanelMinExtent, maxPreviewHeight);
            const int datasetHeight = std::max(1, contentHeight - previewHeight - PixelDataSplitterHeight);
            datasetPanel_->resize(editorX, ContentTop, editorWidth, datasetHeight);
            pixelSplitter_->resize(editorX, ContentTop + datasetHeight, editorWidth, PixelDataSplitterHeight);
            pixelDataPanel_->resize(editorX + 6, ContentTop + datasetHeight + PixelDataSplitterHeight, editorWidth - 12, previewHeight);
        }
    } else {
        datasetPanel_->resize(editorX, ContentTop, editorWidth, contentHeight);
        pixelSplitter_->hide();
    }
    redraw();
}

void EditorWindow::setUiZoom(int size) {
    uiFontSize_ = std::clamp(size, 12, 20);
    FL_NORMAL_SIZE = uiFontSize_;
    Fl::scrollbar_size(uiFontSize_ + 8);
    menu_->textsize(uiFontSize_);
    status_->labelsize(uiFontSize_);
    for (auto *button :
         {openButton_, folderButton_, saveButton_, addButton_, editButton_, deleteButton_, previousFileButton_, nextFileButton_}) {
        button->labelsize(uiFontSize_);
    }
    pixelPreviewButton_->labelsize(uiFontSize_);
    datasetPanel_->setFontSize(uiFontSize_);
    fileTreePanel_->setFontSize(uiFontSize_);
    pixelDataPanel_->setFontSize(uiFontSize_);
    redraw();
}

void EditorWindow::updateActions() {
    const auto actions = controller_.actionState(datasetPanel_->selectedNode());
    setMenuActive(*menu_, "&Edit/Edit or &View Value...", actions.editEnabled);
    setMenuActive(*menu_, "&Edit/&Delete Attribute", actions.deleteEnabled);
    setMenuActive(*menu_, "&File/&Save", actions.saveEnabled);
    setMenuActive(*menu_, "&File/Save A&ll", actions.saveAllEnabled);
    setMenuActive(*menu_, "&File/&Clear Workspace", actions.clearWorkspaceEnabled);
    setWidgetActive(*saveButton_, actions.saveEnabled);
    setWidgetActive(*editButton_, actions.editEnabled);
    setWidgetActive(*deleteButton_, actions.deleteEnabled);
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
    case MenuAction::OpenFiles:
        window->controller_.openDocument();
        break;
    case MenuAction::OpenFolder:
        window->controller_.openFolder();
        break;
    case MenuAction::OpenDicomDirectory:
        window->controller_.openDicomDirectory();
        break;
    case MenuAction::Save:
        window->controller_.saveDocument();
        break;
    case MenuAction::SaveAs:
        window->controller_.saveDocumentAs();
        break;
    case MenuAction::SaveAll:
        window->controller_.saveAllDocuments();
        break;
    case MenuAction::ClearWorkspace:
        window->controller_.clearWorkspace();
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
    case MenuAction::LoadDataDictionary:
        window->controller_.loadDataDictionary();
        break;
    case MenuAction::PixelDataPreview:
        if (widget == window->menu_) {
            window->pixelPreviewButton_->value(window->menu_->mvalue()->value());
        } else {
            setMenuChecked(*window->menu_, "&View/&Pixel Data Preview", window->pixelPreviewButton_->value() != 0);
        }
        window->controller_.setPixelDataVisible(window->pixelPreviewButton_->value() != 0);
        break;
    case MenuAction::PixelDataPreviewVertical:
        window->setPixelDataPreviewVertical(window->menu_->mvalue()->value() != 0);
        break;
    case MenuAction::OpenFilesPanel:
        window->setFileTreeVisible(window->menu_->mvalue()->value() != 0);
        break;
    case MenuAction::SortFilesByFilename:
        window->controller_.setFileSortOrder(window->menu_->mvalue()->value() != 0 ? dicom_editor::FileSortOrder::Filename
                                                                                   : dicom_editor::FileSortOrder::InstanceNumber);
        break;
    case MenuAction::PreviousFile:
        window->controller_.showPreviousDocument();
        break;
    case MenuAction::NextFile:
        window->controller_.showNextDocument();
        break;
    case MenuAction::ZoomIn:
        window->setUiZoom(window->uiFontSize_ + 1);
        break;
    case MenuAction::ZoomOut:
        window->setUiZoom(window->uiFontSize_ - 1);
        break;
    case MenuAction::ZoomReset:
        window->setUiZoom(14);
        break;
    }
}

void EditorWindow::closeCallback(Fl_Widget *, void *data) { static_cast<EditorWindow *>(data)->exit(); }
