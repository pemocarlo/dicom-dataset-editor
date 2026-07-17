#include "AttributeDialog.hpp"

#include "dicom_editor/core/AttributeInput.hpp"
#include "dicom_editor/core/DicomWorkspace.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/Fl_Multiline_Output.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

class Fl_Widget;

namespace {

class AttributeDialogWindow final : public Fl_Window {
  public:
    AttributeDialogWindow(const std::string &title, bool includeTag, const std::string &currentValue)
        : Fl_Window(520, 320, title.c_str()), includeTag_(includeTag) {
        if (includeTag_) {
            group_ = new Fl_Input(115, 20, 380, 28, "Group (hex)");
            element_ = new Fl_Input(115, 58, 380, 28, "Element (hex)");
        }

        const int valueY = includeTag_ ? 96 : 20;
        value_ = new Fl_Multiline_Input(115, valueY, 380, 200, "Value");
        value_->value(currentValue.c_str());

        auto *cancel = new Fl_Button(325, 274, 80, 28, "Cancel");
        cancel->callback(cancelCallback, this);
        auto *ok = new Fl_Button(415, 274, 80, 28, "OK");
        ok->callback(okCallback, this);

        set_modal();
        end();
    }

    std::optional<dicom_editor::AttributeInput> run() {
        show();
        while (shown()) {
            Fl::wait();
        }
        return result_;
    }

  private:
    static void cancelCallback(Fl_Widget *, void *data) { static_cast<AttributeDialogWindow *>(data)->hide(); }

    static void okCallback(Fl_Widget *, void *data) { static_cast<AttributeDialogWindow *>(data)->accept(); }

    void accept() {
        dicom_editor::AttributeInput result;
        if (includeTag_) {
            result.tag = dicom_editor::parseTagKey(group_->value(), element_->value());
            if (!result.tag) {
                fl_alert("Enter a four-digit hexadecimal group and element.");
                return;
            }
        }
        result.value = value_->value();
        result_ = std::move(result);
        hide();
    }

    bool includeTag_{};
    Fl_Input *group_{};
    Fl_Input *element_{};
    Fl_Multiline_Input *value_{};
    std::optional<dicom_editor::AttributeInput> result_;
};

class BatchAttributeDialogWindow final : public Fl_Window {
  public:
    explicit BatchAttributeDialogWindow(const dicom_editor::BatchEditReport &report)
        : Fl_Window(520, 320, "Batch Edit Attribute"), attributes_(report.attributes) {
        attribute_ = new Fl_Choice(115, 20, 380, 28, "Attribute");
        for (const auto &entry : attributes_) {
            attribute_->add(entry.name.c_str());
        }
        attribute_->value(0);
        value_ = new Fl_Multiline_Input(115, 58, 380, 200, "New value");
        attribute_->callback(attributeCallback, this);
        updateValue();
        auto *cancel = new Fl_Button(325, 274, 80, 28, "Cancel");
        cancel->callback(cancelCallback, this);
        auto *ok = new Fl_Button(415, 274, 80, 28, "Apply");
        ok->callback(okCallback, this);
        set_modal();
        end();
    }

    std::optional<dicom_editor::AttributeInput> run() {
        show();
        while (shown()) {
            Fl::wait();
        }
        return result_;
    }

  private:
    static void cancelCallback(Fl_Widget *, void *data) { static_cast<BatchAttributeDialogWindow *>(data)->hide(); }
    static void okCallback(Fl_Widget *, void *data) { static_cast<BatchAttributeDialogWindow *>(data)->accept(); }
    static void attributeCallback(Fl_Widget *, void *data) { static_cast<BatchAttributeDialogWindow *>(data)->updateValue(); }

    void updateValue() {
        const int selected = attribute_->value();
        if (selected < 0 || static_cast<std::size_t>(selected) >= attributes_.size()) {
            value_->value("");
            return;
        }
        const auto &values = attributes_[static_cast<std::size_t>(selected)].values;
        const auto existing = std::ranges::find_if(values, [](const std::string &value) { return value != "<missing>"; });
        value_->value(existing == values.end() ? "" : existing->c_str());
        value_->insert_position(0, value_->size());
    }

    void accept() {
        const int selected = attribute_->value();
        if (selected < 0 || static_cast<std::size_t>(selected) >= attributes_.size()) {
            fl_alert("Select an attribute.");
            return;
        }
        result_ = dicom_editor::AttributeInput{.tag = attributes_[static_cast<std::size_t>(selected)].tag, .value = value_->value()};
        hide();
    }

    std::vector<dicom_editor::BatchAttributeState> attributes_;
    Fl_Choice *attribute_{};
    Fl_Multiline_Input *value_{};
    std::optional<dicom_editor::AttributeInput> result_;
};

class ReadOnlyAttributeDialogWindow final : public Fl_Window {
  public:
    ReadOnlyAttributeDialogWindow(const std::string &title, const std::string &value) : Fl_Window(620, 360, title.c_str()) {
        value_ = new Fl_Multiline_Output(20, 20, 580, 280);
        value_->value(value.c_str());
        auto *close = new Fl_Button(520, 312, 80, 28, "Close");
        close->callback(closeCallback, this);
        set_modal();
        end();
    }

    void run() {
        show();
        while (shown()) {
            Fl::wait();
        }
    }

  private:
    static void closeCallback(Fl_Widget *, void *data) { static_cast<ReadOnlyAttributeDialogWindow *>(data)->hide(); }

    Fl_Multiline_Output *value_{};
};

} // namespace

std::optional<dicom_editor::AttributeInput> AttributeDialog::edit(const std::string &title, const std::string &currentValue) {
    return AttributeDialogWindow(title, false, currentValue).run();
}

void AttributeDialog::view(const std::string &title, const std::string &value) { ReadOnlyAttributeDialogWindow(title, value).run(); }

std::optional<dicom_editor::AttributeInput> AttributeDialog::add() { return AttributeDialogWindow("Add Attribute", true, "").run(); }

std::optional<dicom_editor::AttributeInput> AttributeDialog::batch(const dicom_editor::BatchEditReport &report) {
    return BatchAttributeDialogWindow(report).run();
}
