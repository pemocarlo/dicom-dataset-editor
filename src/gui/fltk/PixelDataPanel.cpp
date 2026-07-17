#include "PixelDataPanel.hpp"

#include "dicom_editor/core/DicomDocument.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int HeaderHeight = 68;
constexpr int ButtonWidth = 64;
constexpr int Padding = 8;

} // namespace

class PixelCanvas final : public Fl_Widget {
  public:
    PixelCanvas(int x, int y, int width, int height) : Fl_Widget(x, y, width, height) {}

    void setPreview(dicom_editor::PixelDataPreview preview) {
        preview_ = std::move(preview);
        source_.reset();
        scaled_.reset();
        scaledWidth_ = 0;
        scaledHeight_ = 0;
        if (!preview_.pixels.empty()) {
            source_ = std::make_unique<Fl_RGB_Image>(preview_.pixels.data(), static_cast<int>(preview_.width),
                                                     static_cast<int>(preview_.height), preview_.channels);
        }
        redraw();
    }

  private:
    void draw() override {
        fl_push_clip(x(), y(), w(), h());
        fl_color(fl_rgb_color(27, 31, 36));
        fl_rectf(x(), y(), w(), h());

        if (source_ == nullptr) {
            const std::string message = preview_.message.empty() ? "No pixel data to display." : preview_.message;
            fl_color(fl_rgb_color(205, 211, 218));
            fl_font(FL_HELVETICA, 14);
            fl_draw(message.c_str(), x() + 24, y() + 24, w() - 48, h() - 48, FL_ALIGN_CENTER | FL_ALIGN_WRAP);
            fl_pop_clip();
            return;
        }

        const double scale =
            std::min(static_cast<double>(w() - 2 * Padding) / source_->w(), static_cast<double>(h() - 2 * Padding) / source_->h());
        const int targetWidth = std::max(1, static_cast<int>(source_->w() * scale));
        const int targetHeight = std::max(1, static_cast<int>(source_->h() * scale));
        if (scaled_ == nullptr || targetWidth != scaledWidth_ || targetHeight != scaledHeight_) {
            scaled_.reset(source_->copy(targetWidth, targetHeight));
            scaledWidth_ = targetWidth;
            scaledHeight_ = targetHeight;
        }

        scaled_->draw(x() + (w() - targetWidth) / 2, y() + (h() - targetHeight) / 2);
        fl_pop_clip();
    }

    dicom_editor::PixelDataPreview preview_;
    std::unique_ptr<Fl_RGB_Image> source_;
    std::unique_ptr<Fl_Image> scaled_;
    int scaledWidth_{};
    int scaledHeight_{};
};

PixelDataPanel::PixelDataPanel(int x, int y, int width, int height) : Fl_Group(x, y, width, height) {
    box(FL_THIN_UP_BOX);
    title_ = new Fl_Box(0, 0, 1, 1, "Pixel Data");
    title_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    title_->labelfont(FL_HELVETICA_BOLD);
    previousFile_ = new Fl_Button(0, 0, 1, 1, "< File");
    previousFile_->callback(previousFileCallback, this);
    nextFile_ = new Fl_Button(0, 0, 1, 1, "File >");
    nextFile_->callback(nextFileCallback, this);
    previous_ = new Fl_Button(0, 0, 1, 1, "< Frame");
    previous_->callback(previousCallback, this);
    next_ = new Fl_Button(0, 0, 1, 1, "Frame >");
    next_->callback(nextCallback, this);
    canvas_ = new PixelCanvas(0, 0, 1, 1);
    resizable(canvas_);
    end();
    layout();
}

void PixelDataPanel::setPreview(dicom_editor::PixelDataPreview preview) {
    const bool hasFrames = preview.frameCount > 0;
    if (!preview.pixels.empty()) {
        title_->copy_label(std::format("{} | File {} of {} | {} x {} | Frame {} of {}", preview.sourceName, preview.sourceIndex + 1,
                                       preview.sourceCount, preview.width, preview.height, preview.frameIndex + 1, preview.frameCount)
                               .c_str());
    } else {
        title_->copy_label(
            std::format("{} | File {} of {} | Pixel Data", preview.sourceName, preview.sourceIndex + 1, preview.sourceCount).c_str());
    }
    if (hasFrames && preview.frameIndex > 0) {
        previous_->activate();
    } else {
        previous_->deactivate();
    }
    if (hasFrames && preview.frameIndex + 1 < preview.frameCount) {
        next_->activate();
    } else {
        next_->deactivate();
    }
    preview.sourceIndex > 0 ? previousFile_->activate() : previousFile_->deactivate();
    preview.sourceIndex + 1 < preview.sourceCount ? nextFile_->activate() : nextFile_->deactivate();
    canvas_->setPreview(std::move(preview));
}

void PixelDataPanel::setPreviousHandler(std::function<void()> handler) { previousHandler_ = std::move(handler); }

void PixelDataPanel::setNextHandler(std::function<void()> handler) { nextHandler_ = std::move(handler); }

void PixelDataPanel::setPreviousFileHandler(std::function<void()> handler) { previousFileHandler_ = std::move(handler); }

void PixelDataPanel::setNextFileHandler(std::function<void()> handler) { nextFileHandler_ = std::move(handler); }

void PixelDataPanel::resize(int x, int y, int width, int height) {
    Fl_Group::resize(x, y, width, height);
    layout();
}

void PixelDataPanel::previousCallback(Fl_Widget *, void *data) {
    auto &panel = *static_cast<PixelDataPanel *>(data);
    if (panel.previousHandler_) {
        panel.previousHandler_();
    }
}

void PixelDataPanel::nextCallback(Fl_Widget *, void *data) {
    auto &panel = *static_cast<PixelDataPanel *>(data);
    if (panel.nextHandler_) {
        panel.nextHandler_();
    }
}

void PixelDataPanel::previousFileCallback(Fl_Widget *, void *data) {
    auto &panel = *static_cast<PixelDataPanel *>(data);
    if (panel.previousFileHandler_) {
        panel.previousFileHandler_();
    }
}

void PixelDataPanel::nextFileCallback(Fl_Widget *, void *data) {
    auto &panel = *static_cast<PixelDataPanel *>(data);
    if (panel.nextFileHandler_) {
        panel.nextFileHandler_();
    }
}

void PixelDataPanel::layout() {
    const int buttonHeight = 28;
    title_->resize(x() + Padding, y(), std::max(1, w() - 2 * Padding), HeaderHeight - buttonHeight - Padding);
    previousFile_->resize(x() + Padding, y() + HeaderHeight - buttonHeight, ButtonWidth, buttonHeight);
    nextFile_->resize(previousFile_->x() + ButtonWidth + Padding, previousFile_->y(), ButtonWidth, buttonHeight);
    next_->resize(x() + w() - Padding - ButtonWidth, previousFile_->y(), ButtonWidth, buttonHeight);
    previous_->resize(next_->x() - Padding - ButtonWidth, previousFile_->y(), ButtonWidth, buttonHeight);
    canvas_->resize(x() + Padding, y() + HeaderHeight, std::max(1, w() - 2 * Padding), std::max(1, h() - HeaderHeight - Padding));
}
