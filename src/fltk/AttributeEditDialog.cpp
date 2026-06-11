#include "AttributeEditDialog.hpp"

#include "dicom_editor/AttributeInput.hpp"

#include <dcmtk/dcmdata/dctagkey.h>

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <optional>
#include <string>
#include <utility>

class Fl_Widget;

namespace {

class Dialog final : public Fl_Window {
  public:
    Dialog(const std::string &title, bool includeTag, const std::string &currentValue)
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
    static void cancelCallback(Fl_Widget *, void *data) { static_cast<Dialog *>(data)->hide(); }

    static void okCallback(Fl_Widget *, void *data) { static_cast<Dialog *>(data)->accept(); }

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

} // namespace

std::optional<dicom_editor::AttributeInput> AttributeEditDialog::Edit(const std::string &title, const std::string &currentValue) {
    return Dialog(title, false, currentValue).run();
}

std::optional<dicom_editor::AttributeInput> AttributeEditDialog::Add() { return Dialog("Add Attribute", true, "").run(); }
