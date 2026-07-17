#include "EditorWindow.hpp"

#include "AttributeDialog.hpp"
#include "dicom_editor/application/EditorController.hpp"
#include "dicom_editor/core/AttributeInput.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class Fl_Widget;

namespace {

std::optional<std::filesystem::path> chooseSingleFile(Fl_Native_File_Chooser::Type type, const char *title, const char *filter = nullptr) {
    Fl_Native_File_Chooser chooser(type);
    chooser.title(title);
    if (filter != nullptr) {
        chooser.filter(filter);
    }
    return chooser.show() == 0 ? std::optional<std::filesystem::path>{chooser.filename()} : std::nullopt;
}

} // namespace

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
    return chooseSingleFile(Fl_Native_File_Chooser::BROWSE_DIRECTORY, "Open Folder of DICOM Files");
}

std::optional<std::filesystem::path> EditorWindow::chooseSaveFile() {
    return chooseSingleFile(Fl_Native_File_Chooser::BROWSE_SAVE_FILE, "Save DICOM File", "DICOM files\t*.dcm\nAll files\t*");
}

std::optional<std::filesystem::path> EditorWindow::chooseDicomDirectory() {
    return chooseSingleFile(Fl_Native_File_Chooser::BROWSE_FILE, "Open DICOMDIR");
}

std::optional<std::filesystem::path> EditorWindow::chooseDataDictionary() {
    return chooseSingleFile(Fl_Native_File_Chooser::BROWSE_FILE, "Load DCMTK Data Dictionary", "DCMTK dictionaries\t*.dic\nAll files\t*");
}

dicom_editor::SaveChangesChoice EditorWindow::confirmSaveChanges() {
    const int answer = fl_choice("Save changes before continuing?", "Cancel", "Don't Save", "Save");
    if (answer == 1) {
        return dicom_editor::SaveChangesChoice::Discard;
    }
    return answer == 2 ? dicom_editor::SaveChangesChoice::Save : dicom_editor::SaveChangesChoice::Cancel;
}

dicom_editor::SaveChangesChoice EditorWindow::confirmWorkspaceChanges(std::size_t dirtyCount) {
    const int answer = fl_choice("%zu datasets have unsaved changes.", "Cancel", "Discard All", "Save All", dirtyCount);
    if (answer == 1) {
        return dicom_editor::SaveChangesChoice::Discard;
    }
    return answer == 2 ? dicom_editor::SaveChangesChoice::Save : dicom_editor::SaveChangesChoice::Cancel;
}

bool EditorWindow::confirmDelete() { return fl_choice("Delete selected attribute?", "Cancel", "Delete", nullptr) == 1; }

std::optional<dicom_editor::AttributeInput> EditorWindow::editAttribute(const std::string &title, const std::string &value) {
    return AttributeDialog::edit(title, value);
}

void EditorWindow::viewAttribute(const std::string &title, const std::string &value) { AttributeDialog::view(title, value); }

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
    return fl_choice("%s", "Cancel", "Continue", nullptr, summary.c_str()) == 1 ? AttributeDialog::batch(report) : std::nullopt;
}

dicom_editor::SaveAllReport EditorWindow::runSaveAllJob(dicom_editor::SaveAllTask task) {
    struct JobState {
        std::mutex mutex;
        dicom_editor::SaveAllProgress progress;
        std::optional<dicom_editor::SaveAllReport> report;
    } state;

    Fl_Window dialog(520, 150, "Saving DICOM Files");
    Fl_Box current(20, 15, 480, 35, "Preparing save...");
    current.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    Fl_Progress progress(20, 60, 480, 25);
    progress.minimum(0.0F);
    progress.maximum(1.0F);
    Fl_Button cancel(210, 105, 100, 30, "Cancel");
    dialog.set_modal();
    dialog.end();

    std::jthread worker([&state, task = std::move(task)](std::stop_token stop) {
        auto report = task(std::move(stop), [&state](const dicom_editor::SaveAllProgress &value) {
            {
                const std::scoped_lock lock(state.mutex);
                state.progress = value;
            }
            Fl::awake();
        });
        {
            const std::scoped_lock lock(state.mutex);
            state.report = std::move(report);
        }
        Fl::awake();
    });
    const auto requestStop = [](Fl_Widget *, void *data) { static_cast<std::jthread *>(data)->request_stop(); };
    cancel.callback(requestStop, &worker);
    dialog.callback(requestStop, &worker);

    deactivate();
    dialog.show();
    for (;;) {
        dicom_editor::SaveAllProgress snapshot;
        bool done{};
        {
            const std::scoped_lock lock(state.mutex);
            snapshot = state.progress;
            done = state.report.has_value();
        }
        if (snapshot.total != 0) {
            progress.maximum(static_cast<float>(snapshot.total));
            progress.value(static_cast<float>(snapshot.completed));
            current.copy_label(std::format("{} of {}: {}", snapshot.completed, snapshot.total, snapshot.currentPath.string()).c_str());
        }
        if (done) {
            break;
        }
        Fl::wait(0.05);
    }
    worker.join();
    dialog.hide();
    activate();
    take_focus();
    const std::scoped_lock lock(state.mutex);
    return std::move(*state.report);
}

void EditorWindow::showError(const std::string &message) { fl_alert("%s", message.c_str()); }
