#include "EditorWindow.hpp"

#include "AttributeDialog.hpp"
#include "DatasetPanel.hpp"
#include "FileTreePanel.hpp"
#include "PixelDataPanel.hpp"
#include "dicom_editor/AttributeInput.hpp"
#include "dicom_editor/DicomDocument.hpp"
#include "dicom_editor/DicomNode.hpp"
#include "dicom_editor/DicomWorkspace.hpp"
#include "dicom_editor/EditorController.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

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
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
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
constexpr int FileTreeSplitterWidth = 6;
constexpr int FileTreePanelMinWidth = 180;
constexpr int EditorPanelMinWidth = 400;

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
    PixelDataPreview,
    PixelDataPreviewVertical,
    OpenFilesPanel,
    SortFilesByFilename,
    PreviousFile,
    NextFile,
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
MenuAction pixelDataPreviewAction = MenuAction::PixelDataPreview;
MenuAction pixelDataPreviewVerticalAction = MenuAction::PixelDataPreviewVertical;
MenuAction openFilesPanelAction = MenuAction::OpenFilesPanel;
MenuAction sortFilesByFilenameAction = MenuAction::SortFilesByFilename;
MenuAction previousFileAction = MenuAction::PreviousFile;
MenuAction nextFileAction = MenuAction::NextFile;

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

std::optional<std::filesystem::path> chooseSaveFile(const char *title) {
    Fl_Native_File_Chooser chooser(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
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

void setMenuChecked(Fl_Menu_Bar &menu, const char *path, bool checked) {
    const int index = menu.find_index(path);
    if (index < 0) {
        return;
    }
    const int flags = menu.mode(index);
    menu.mode(index, checked ? flags | FL_MENU_VALUE : flags & ~FL_MENU_VALUE);
}

} // namespace

EditorWindow::EditorWindow() : Fl_Double_Window(1180, 760, "DICOM Dataset Editor"), controller_(*this) {
    menu_ = new Fl_Menu_Bar(0, 0, w(), MenuHeight);
    menu_->add("&File/&Open Files...", FL_CTRL + 'o', menuCallback, &openFilesAction);
    menu_->add("&File/Open &Folder...", FL_CTRL + FL_SHIFT + 'o', menuCallback, &openFolderAction);
    menu_->add("&File/Open &DICOMDIR...", 0, menuCallback, &openDicomDirectoryAction);
    menu_->add("&File/&Save", FL_CTRL + 's', menuCallback, &saveAction);
    menu_->add("&File/Save &As...", FL_CTRL + FL_SHIFT + 's', menuCallback, &saveAsAction);
    menu_->add("&File/Save A&ll", FL_CTRL + FL_ALT + 's', menuCallback, &saveAllAction);
    menu_->add("&File/&Clear Workspace", FL_CTRL + 'w', menuCallback, &clearWorkspaceAction);
    menu_->add("&File/E&xit", 0, menuCallback, &exitAction);
    menu_->add("&Edit/&Edit Value...", FL_Enter, menuCallback, &editAction);
    menu_->add("&Edit/&Add Attribute...", FL_CTRL + 'n', menuCallback, &addAction);
    menu_->add("&Edit/&Delete Attribute", FL_Delete, menuCallback, &deleteAction);
    menu_->add("&Settings/&Validate DICOM Values", 0, menuCallback, &validateValuesAction, FL_MENU_TOGGLE | FL_MENU_VALUE);
    menu_->add("&View/&Pixel Data Preview", 0, menuCallback, &pixelDataPreviewAction, FL_MENU_TOGGLE);
    menu_->add("&View/Pixel Data Preview on &Right", 0, menuCallback, &pixelDataPreviewVerticalAction, FL_MENU_TOGGLE);
    menu_->add("&View/&Open Files Panel", 0, menuCallback, &openFilesPanelAction, FL_MENU_TOGGLE);
    menu_->add("&View/Sort Files by &Filename", 0, menuCallback, &sortFilesByFilenameAction, FL_MENU_TOGGLE);
    menu_->add("&View/&Previous File", FL_CTRL + FL_Page_Up, menuCallback, &previousFileAction);
    menu_->add("&View/&Next File", FL_CTRL + FL_Page_Down, menuCallback, &nextFileAction);

    fileTreePanel_ = new FileTreePanel(0, MenuHeight, fileTreePanelExtent_, h() - MenuHeight - StatusHeight);
    fileTreePanel_->setActivationHandler([this](std::size_t index) { controller_.activateDocument(index); });
    fileTreePanel_->setBatchEditHandler([this](const dicom_editor::BatchEditTarget &target) { controller_.batchEdit(target); });
    fileTreePanel_->hide();

    fileTreeSplitter_ = new FileTreeSplitter(0, MenuHeight, FileTreeSplitterWidth, h() - MenuHeight - StatusHeight, *this);
    fileTreeSplitter_->hide();

    datasetPanel_ = new DatasetPanel(0, MenuHeight, w(), h() - MenuHeight - StatusHeight);
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

std::vector<std::filesystem::path> EditorWindow::chooseOpenFiles() {
    Fl_Native_File_Chooser chooser(Fl_Native_File_Chooser::BROWSE_MULTI_FILE);
    chooser.title("Open DICOM Files");
    chooser.filter("DICOM files\t*.dcm");
    if (chooser.show() != 0) {
        return {};
    }
    std::vector<std::filesystem::path> result;
    result.reserve(static_cast<std::size_t>(chooser.count()));
    for (int index = 0; index < chooser.count(); ++index) {
        result.emplace_back(chooser.filename(index));
    }
    return result;
}

std::optional<std::filesystem::path> EditorWindow::chooseOpenFolder() {
    Fl_Native_File_Chooser chooser(Fl_Native_File_Chooser::BROWSE_DIRECTORY);
    chooser.title("Open Folder of DICOM Files");
    return chooser.show() == 0 ? std::optional<std::filesystem::path>{chooser.filename()} : std::nullopt;
}

std::optional<std::filesystem::path> EditorWindow::chooseSaveFile() { return ::chooseSaveFile("Save DICOM File"); }

std::optional<std::filesystem::path> EditorWindow::chooseDicomDirectory() {
    Fl_Native_File_Chooser chooser(Fl_Native_File_Chooser::BROWSE_FILE);
    chooser.title("Open DICOMDIR");
    return chooser.show() == 0 ? std::optional<std::filesystem::path>{chooser.filename()} : std::nullopt;
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

dicom_editor::SaveChangesChoice EditorWindow::confirmWorkspaceChanges(std::size_t dirtyCount) {
    const int answer = fl_choice("%zu datasets have unsaved changes.", "Cancel", "Discard All", "Save All", dirtyCount);
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

std::optional<dicom_editor::AttributeInput> EditorWindow::batchEditAttribute(const dicom_editor::BatchEditReport &report) {
    std::string summary =
        std::format("{} dataset(s) in {} '{}'.\n\n", report.documentCount,
                    report.target.level == dicom_editor::BatchEditLevel::Patient ? "patient" : "study", report.target.label);
    for (const auto &attribute : report.attributes) {
        summary += std::format("{} ({:04x},{:04x}): ", attribute.name, attribute.tag.getGroup(), attribute.tag.getElement());
        for (std::size_t index = 0; index < attribute.values.size(); ++index) {
            summary += (index == 0 ? "" : " | ") + attribute.values[index];
        }
        summary += attribute.values.size() <= 1 ? " [consistent]\n" : " [DIFFERS]\n";
    }
    summary += "\nContinue to choose one listed level attribute and replacement value?";
    if (fl_choice("%s", "Cancel", "Continue", nullptr, summary.c_str()) != 1) {
        return std::nullopt;
    }
    return AttributeDialog::batch(report);
}

void EditorWindow::showError(const std::string &message) { fl_alert("%s", message.c_str()); }

void EditorWindow::presentDocument(std::vector<dicom_editor::DicomNode> nodes, const std::string &title, const std::string &status) {
    datasetPanel_->setNodes(std::move(nodes));
    copy_label(title.c_str());
    setStatus(status);
    updateActions();
}

void EditorWindow::presentOpenFiles(const std::vector<dicom_editor::OpenDicomFile> &files, bool hasLoadedFiles) {
    fileTreePanel_->setFiles(files);
    if (hasLoadedFiles && !workspaceHadFiles_) {
        setFileTreeVisible(true);
    } else if (!hasLoadedFiles && workspaceHadFiles_) {
        setFileTreeVisible(false);
    }
    workspaceHadFiles_ = hasLoadedFiles;
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
    const int contentExtent = pixelDataPreviewVertical_ ? w() - fileTreeWidth : h() - MenuHeight - StatusHeight;
    const int maximumPreviewExtent = std::max(PixelDataPanelMinHeight, contentExtent - PixelDataSplitterHeight - PixelDataPanelMinHeight);
    const int clamped = std::clamp(extent, PixelDataPanelMinHeight, maximumPreviewExtent);
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
    status_->resize(6, h() - StatusHeight, w() - 12, StatusHeight);

    const int contentHeight = h() - MenuHeight - StatusHeight;
    const int fileTreeWidth = fileTreeVisible_
                                  ? std::clamp(fileTreePanelExtent_, FileTreePanelMinWidth,
                                               std::max(FileTreePanelMinWidth, w() - FileTreeSplitterWidth - EditorPanelMinWidth))
                                  : 0;
    const int editorX = fileTreeVisible_ ? fileTreeWidth + FileTreeSplitterWidth : 0;
    const int editorWidth = std::max(1, w() - editorX);
    if (fileTreeVisible_) {
        fileTreePanel_->resize(0, MenuHeight, fileTreeWidth, contentHeight);
        fileTreeSplitter_->resize(fileTreeWidth, MenuHeight, FileTreeSplitterWidth, contentHeight);
    }
    if (pixelDataPanel_->visible() != 0) {
        if (pixelDataPreviewVertical_) {
            const int maxPreviewWidth = std::max(PixelDataPanelMinHeight, editorWidth - PixelDataSplitterHeight - PixelDataPanelMinHeight);
            const int previewWidth = std::clamp(pixelDataPanelExtent_, PixelDataPanelMinHeight, maxPreviewWidth);
            const int datasetWidth = std::max(1, editorWidth - previewWidth - PixelDataSplitterHeight);
            datasetPanel_->resize(editorX, MenuHeight, datasetWidth, contentHeight);
            pixelSplitter_->resize(editorX + datasetWidth, MenuHeight, PixelDataSplitterHeight, contentHeight);
            pixelDataPanel_->resize(editorX + datasetWidth + PixelDataSplitterHeight, MenuHeight, previewWidth, contentHeight);
        } else {
            const int maxPreviewHeight =
                std::max(PixelDataPanelMinHeight, contentHeight - PixelDataSplitterHeight - PixelDataPanelMinHeight);
            const int previewHeight = std::clamp(pixelDataPanelExtent_, PixelDataPanelMinHeight, maxPreviewHeight);
            const int datasetHeight = std::max(1, contentHeight - previewHeight - PixelDataSplitterHeight);
            datasetPanel_->resize(editorX, MenuHeight, editorWidth, datasetHeight);
            pixelSplitter_->resize(editorX, MenuHeight + datasetHeight, editorWidth, PixelDataSplitterHeight);
            pixelDataPanel_->resize(editorX + 6, MenuHeight + datasetHeight + PixelDataSplitterHeight, editorWidth - 12, previewHeight);
        }
    } else {
        datasetPanel_->resize(editorX, MenuHeight, editorWidth, contentHeight);
        pixelSplitter_->hide();
    }
    redraw();
}

void EditorWindow::updateActions() {
    const auto actions = controller_.actionState(datasetPanel_->selectedNode());
    setMenuActive(*menu_, "&Edit/&Edit Value...", actions.editEnabled);
    setMenuActive(*menu_, "&Edit/&Delete Attribute", actions.deleteEnabled);
    setMenuActive(*menu_, "&File/&Save", actions.saveEnabled);
    setMenuActive(*menu_, "&File/Save A&ll", actions.saveAllEnabled);
    setMenuActive(*menu_, "&File/&Clear Workspace", actions.clearWorkspaceEnabled);
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
    case MenuAction::PixelDataPreview:
        window->controller_.setPixelDataVisible(window->menu_->mvalue()->value() != 0);
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
    }
}

void EditorWindow::closeCallback(Fl_Widget *, void *data) { static_cast<EditorWindow *>(data)->exit(); }
