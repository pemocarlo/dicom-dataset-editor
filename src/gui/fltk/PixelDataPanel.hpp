#pragma once

#include <FL/Fl_Group.H>

#include <functional>

class Fl_Box;
class Fl_Button;
class Fl_Widget;
class PixelCanvas;

namespace dicom_editor {
struct PixelDataPreview;
} // namespace dicom_editor

/// Pixel preview panel used by the main window.
class PixelDataPanel final : public Fl_Group {
  public:
    /// Creates the panel with navigation controls and canvas.
    PixelDataPanel(int x, int y, int width, int height);

    /// Updates the rendered preview and title text.
    void setPreview(dicom_editor::PixelDataPreview preview);
    /// Sets the callback for the Previous button.
    void setPreviousHandler(std::function<void()> handler);
    /// Sets the callback for the Next button.
    void setNextHandler(std::function<void()> handler);
    /// Sets the callback for the previous-file button.
    void setPreviousFileHandler(std::function<void()> handler);
    /// Sets the callback for the next-file button.
    void setNextFileHandler(std::function<void()> handler);
    /// Increases image magnification around the canvas center.
    void zoomIn();
    /// Decreases image magnification around the canvas center.
    void zoomOut();
    /// Fits the whole image in the available canvas.
    void fitImage();
    /// Displays one source pixel per screen pixel.
    void showActualSize();
    void setFontSize(int size);
    /// Repositions child widgets after resize.
    void resize(int x, int y, int width, int height) override;

  private:
    static void previousCallback(Fl_Widget *widget, void *data);
    static void nextCallback(Fl_Widget *widget, void *data);
    static void previousFileCallback(Fl_Widget *widget, void *data);
    static void nextFileCallback(Fl_Widget *widget, void *data);
    static void zoomInCallback(Fl_Widget *widget, void *data);
    static void zoomOutCallback(Fl_Widget *widget, void *data);
    static void fitCallback(Fl_Widget *widget, void *data);
    static void actualSizeCallback(Fl_Widget *widget, void *data);
    void updateZoomLabel(int percent);
    void layout();

    Fl_Box *title_{};
    Fl_Button *previous_{};
    Fl_Button *next_{};
    Fl_Button *previousFile_{};
    Fl_Button *nextFile_{};
    Fl_Button *zoomOut_{};
    Fl_Box *zoomLevel_{};
    Fl_Button *zoomIn_{};
    Fl_Button *fit_{};
    Fl_Button *actualSize_{};
    PixelCanvas *canvas_{};
    std::function<void()> previousHandler_;
    std::function<void()> nextHandler_;
    std::function<void()> previousFileHandler_;
    std::function<void()> nextFileHandler_;
};
