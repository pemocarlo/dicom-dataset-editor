#include "PixelDataPanel.hpp"

#include "dicom_editor/core/DicomDocument.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int HeaderHeight = 112;
constexpr int MaximumNavigationButtonWidth = 76;
constexpr int Padding = 10;
constexpr double ZoomStep = 1.25;
constexpr double MinimumFitMultiplier = 0.25;
constexpr double MaximumFitMultiplier = 8.0;

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
        fitImage();
        redraw();
    }

    void setZoomChangedHandler(std::function<void(int)> handler) { zoomChangedHandler_ = std::move(handler); }

    void zoomIn() { zoomBy(ZoomStep, x() + w() / 2, y() + h() / 2); }

    void zoomOut() { zoomBy(1.0 / ZoomStep, x() + w() / 2, y() + h() / 2); }

    void fitImage() {
        fitMultiplier_ = 1.0;
        panX_ = 0;
        panY_ = 0;
        notifyZoomChanged();
        redraw();
    }

    void showActualSize() {
        const double scale = fittedScale();
        if (source_ == nullptr || scale <= 0.0) {
            return;
        }
        fitMultiplier_ = 1.0 / scale;
        panX_ = 0;
        panY_ = 0;
        clampPan();
        notifyZoomChanged();
        redraw();
    }

    void resize(int x, int y, int width, int height) override {
        Fl_Widget::resize(x, y, width, height);
        clampPan();
        notifyZoomChanged();
        redraw();
    }

  private:
    int handle(int event) override {
        switch (event) {
        case FL_FOCUS:
        case FL_UNFOCUS:
            return 1;
        case FL_MOUSEWHEEL:
            if (source_ != nullptr && Fl::event_dy() != 0) {
                zoomBy(Fl::event_dy() < 0 ? ZoomStep : 1.0 / ZoomStep, Fl::event_x(), Fl::event_y());
                return 1;
            }
            break;
        case FL_PUSH:
            if (Fl::event_button() == FL_LEFT_MOUSE && source_ != nullptr) {
                take_focus();
                if (Fl::event_clicks() != 0) {
                    fitImage();
                } else {
                    panning_ = true;
                    lastPointerX_ = Fl::event_x();
                    lastPointerY_ = Fl::event_y();
                }
                return 1;
            }
            break;
        case FL_DRAG:
            if (panning_) {
                panX_ += Fl::event_x() - lastPointerX_;
                panY_ += Fl::event_y() - lastPointerY_;
                lastPointerX_ = Fl::event_x();
                lastPointerY_ = Fl::event_y();
                clampPan();
                redraw();
                return 1;
            }
            break;
        case FL_RELEASE:
            if (panning_) {
                panning_ = false;
                return 1;
            }
            break;
        case FL_KEYDOWN:
            if (Fl::event_key() == '+' || Fl::event_key() == '=') {
                zoomIn();
                return 1;
            }
            if (Fl::event_key() == '-') {
                zoomOut();
                return 1;
            }
            if (Fl::event_key() == '0') {
                fitImage();
                return 1;
            }
            if (Fl::event_key() == '1') {
                showActualSize();
                return 1;
            }
            break;
        default:
            break;
        }
        return Fl_Widget::handle(event);
    }

    void draw() override {
        fl_push_clip(x(), y(), w(), h());
        fl_color(fl_rgb_color(27, 31, 36));
        fl_rectf(x(), y(), w(), h());
        fl_color(fl_rgb_color(52, 60, 68));
        fl_rect(x(), y(), w(), h());

        if (source_ == nullptr) {
            const std::string message = preview_.message.empty() ? "No pixel data to display." : preview_.message;
            fl_color(fl_rgb_color(205, 211, 218));
            fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
            fl_draw(message.c_str(), x() + 24, y() + 24, w() - 48, h() - 48, FL_ALIGN_CENTER | FL_ALIGN_WRAP);
            fl_pop_clip();
            return;
        }

        clampPan();
        const double scale = displayedScale();
        const int targetWidth = std::max(1, static_cast<int>(source_->w() * scale));
        const int targetHeight = std::max(1, static_cast<int>(source_->h() * scale));
        const int drawX = x() + (w() - targetWidth) / 2 + panX_;
        const int drawY = y() + (h() - targetHeight) / 2 + panY_;
        if (targetWidth == source_->w() && targetHeight == source_->h()) {
            scaled_.reset();
            scaledWidth_ = 0;
            scaledHeight_ = 0;
            source_->draw(drawX, drawY);
            fl_pop_clip();
            return;
        }
        if (scaled_ == nullptr || targetWidth != scaledWidth_ || targetHeight != scaledHeight_) {
            scaled_.reset(source_->copy(targetWidth, targetHeight));
            scaledWidth_ = targetWidth;
            scaledHeight_ = targetHeight;
        }

        scaled_->draw(drawX, drawY);
        fl_pop_clip();
    }

    [[nodiscard]] double fittedScale() const {
        if (source_ == nullptr) {
            return 1.0;
        }
        const int availableWidth = std::max(1, w() - 2 * Padding);
        const int availableHeight = std::max(1, h() - 2 * Padding);
        return std::min(static_cast<double>(availableWidth) / source_->w(), static_cast<double>(availableHeight) / source_->h());
    }

    [[nodiscard]] double displayedScale() const { return fittedScale() * fitMultiplier_; }

    void zoomBy(double factor, int focalX, int focalY) {
        if (source_ == nullptr) {
            return;
        }
        const double oldScale = displayedScale();
        const int oldWidth = std::max(1, static_cast<int>(source_->w() * oldScale));
        const int oldHeight = std::max(1, static_cast<int>(source_->h() * oldScale));
        const double oldLeft = x() + (static_cast<double>(w()) - oldWidth) / 2.0 + panX_;
        const double oldTop = y() + (static_cast<double>(h()) - oldHeight) / 2.0 + panY_;
        const double imageX = (focalX - oldLeft) / oldScale;
        const double imageY = (focalY - oldTop) / oldScale;

        fitMultiplier_ = std::clamp(fitMultiplier_ * factor, MinimumFitMultiplier, MaximumFitMultiplier);
        const double newScale = displayedScale();
        const int newWidth = std::max(1, static_cast<int>(source_->w() * newScale));
        const int newHeight = std::max(1, static_cast<int>(source_->h() * newScale));
        const double newLeft = x() + (static_cast<double>(w()) - newWidth) / 2.0;
        const double newTop = y() + (static_cast<double>(h()) - newHeight) / 2.0;
        panX_ = static_cast<int>(std::lround(focalX - imageX * newScale - newLeft));
        panY_ = static_cast<int>(std::lround(focalY - imageY * newScale - newTop));
        clampPan();
        notifyZoomChanged();
        redraw();
    }

    void clampPan() {
        if (source_ == nullptr) {
            panX_ = 0;
            panY_ = 0;
            return;
        }
        const int targetWidth = std::max(1, static_cast<int>(source_->w() * displayedScale()));
        const int targetHeight = std::max(1, static_cast<int>(source_->h() * displayedScale()));
        const int maximumPanX = std::max(0, (targetWidth - w()) / 2 + Padding);
        const int maximumPanY = std::max(0, (targetHeight - h()) / 2 + Padding);
        panX_ = std::clamp(panX_, -maximumPanX, maximumPanX);
        panY_ = std::clamp(panY_, -maximumPanY, maximumPanY);
    }

    void notifyZoomChanged() {
        if (zoomChangedHandler_) {
            zoomChangedHandler_(source_ == nullptr ? 0 : static_cast<int>(std::lround(displayedScale() * 100.0)));
        }
    }

    dicom_editor::PixelDataPreview preview_;
    std::unique_ptr<Fl_RGB_Image> source_;
    std::unique_ptr<Fl_Image> scaled_;
    std::function<void(int)> zoomChangedHandler_;
    int scaledWidth_{};
    int scaledHeight_{};
    double fitMultiplier_{1.0};
    int panX_{};
    int panY_{};
    int lastPointerX_{};
    int lastPointerY_{};
    bool panning_{};
};

PixelDataPanel::PixelDataPanel(int x, int y, int width, int height) : Fl_Group(x, y, width, height) {
    box(FL_FLAT_BOX);
    color(fl_rgb_color(238, 243, 247));
    title_ = new Fl_Box(0, 0, 1, 1, "Pixel Data");
    title_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    title_->labelfont(FL_HELVETICA_BOLD);
    title_->labelcolor(fl_rgb_color(42, 62, 78));
    previousFile_ = new Fl_Button(0, 0, 1, 1, "< File");
    previousFile_->callback(previousFileCallback, this);
    nextFile_ = new Fl_Button(0, 0, 1, 1, "File >");
    nextFile_->callback(nextFileCallback, this);
    previous_ = new Fl_Button(0, 0, 1, 1, "< Frame");
    previous_->callback(previousCallback, this);
    next_ = new Fl_Button(0, 0, 1, 1, "Frame >");
    next_->callback(nextCallback, this);
    zoomOut_ = new Fl_Button(0, 0, 1, 1, "-");
    zoomOut_->callback(zoomOutCallback, this);
    zoomLevel_ = new Fl_Box(0, 0, 1, 1, "Fit");
    zoomLevel_->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    zoomLevel_->labelcolor(fl_rgb_color(42, 62, 78));
    zoomIn_ = new Fl_Button(0, 0, 1, 1, "+");
    zoomIn_->callback(zoomInCallback, this);
    fit_ = new Fl_Button(0, 0, 1, 1, "Fit");
    fit_->callback(fitCallback, this);
    actualSize_ = new Fl_Button(0, 0, 1, 1, "1:1");
    actualSize_->callback(actualSizeCallback, this);
    for (auto *button : {previousFile_, nextFile_, previous_, next_, zoomOut_, zoomIn_, fit_, actualSize_}) {
        button->box(FL_FLAT_BOX);
        button->color(fl_rgb_color(218, 228, 236));
        button->selection_color(fl_rgb_color(190, 216, 239));
        button->labelcolor(fl_rgb_color(31, 55, 74));
        button->labelfont(FL_HELVETICA_BOLD);
    }
    canvas_ = new PixelCanvas(0, 0, 1, 1);
    canvas_->setZoomChangedHandler([this](int percent) { updateZoomLabel(percent); });
    canvas_->tooltip("Mouse wheel or +/-: zoom | Drag: pan | Double-click or 0: fit | 1: actual size");
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

void PixelDataPanel::zoomIn() { canvas_->zoomIn(); }

void PixelDataPanel::zoomOut() { canvas_->zoomOut(); }

void PixelDataPanel::fitImage() { canvas_->fitImage(); }

void PixelDataPanel::showActualSize() { canvas_->showActualSize(); }

void PixelDataPanel::setFontSize(int size) {
    title_->labelsize(size);
    zoomLevel_->labelsize(size);
    for (auto *button : {previousFile_, nextFile_, previous_, next_, zoomOut_, zoomIn_, fit_, actualSize_}) {
        button->labelsize(size);
    }
    redraw();
}

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

void PixelDataPanel::zoomInCallback(Fl_Widget *, void *data) { static_cast<PixelDataPanel *>(data)->zoomIn(); }

void PixelDataPanel::zoomOutCallback(Fl_Widget *, void *data) { static_cast<PixelDataPanel *>(data)->zoomOut(); }

void PixelDataPanel::fitCallback(Fl_Widget *, void *data) { static_cast<PixelDataPanel *>(data)->fitImage(); }

void PixelDataPanel::actualSizeCallback(Fl_Widget *, void *data) { static_cast<PixelDataPanel *>(data)->showActualSize(); }

void PixelDataPanel::updateZoomLabel(int percent) {
    if (percent <= 0) {
        zoomLevel_->copy_label("--");
    } else {
        zoomLevel_->copy_label(std::format("{}%", percent).c_str());
    }
}

void PixelDataPanel::layout() {
    const int buttonHeight = 28;
    const int compactGap = 6;
    const int navigationWidth = std::clamp((w() - 2 * Padding - 3 * compactGap) / 4, 48, MaximumNavigationButtonWidth);
    title_->resize(x() + Padding, y(), std::max(1, w() - 2 * Padding), 34);
    previousFile_->resize(x() + Padding, y() + 36, navigationWidth, buttonHeight);
    nextFile_->resize(previousFile_->x() + navigationWidth + compactGap, previousFile_->y(), navigationWidth, buttonHeight);
    previous_->resize(nextFile_->x() + navigationWidth + compactGap, previousFile_->y(), navigationWidth, buttonHeight);
    next_->resize(previous_->x() + navigationWidth + compactGap, previousFile_->y(), navigationWidth, buttonHeight);

    const int viewerY = y() + 70;
    const int controlsWidth = 32 + 54 + 32 + 48 + 48 + 4 * compactGap;
    const int viewerX = x() + std::max(Padding, (w() - controlsWidth) / 2);
    zoomOut_->resize(viewerX, viewerY, 32, buttonHeight);
    zoomLevel_->resize(zoomOut_->x() + zoomOut_->w() + compactGap, viewerY, 54, buttonHeight);
    zoomIn_->resize(zoomLevel_->x() + zoomLevel_->w() + compactGap, viewerY, 32, buttonHeight);
    fit_->resize(zoomIn_->x() + zoomIn_->w() + compactGap, viewerY, 48, buttonHeight);
    actualSize_->resize(fit_->x() + fit_->w() + compactGap, viewerY, 48, buttonHeight);
    canvas_->resize(x() + Padding, y() + HeaderHeight, std::max(1, w() - 2 * Padding), std::max(1, h() - HeaderHeight - Padding));
}
