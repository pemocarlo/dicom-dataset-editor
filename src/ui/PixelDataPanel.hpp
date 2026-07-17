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

class PixelDataPanel final : public Fl_Group {
  public:
    PixelDataPanel(int x, int y, int width, int height);

    void setPreview(dicom_editor::PixelDataPreview preview);
    void setPreviousHandler(std::function<void()> handler);
    void setNextHandler(std::function<void()> handler);
    void resize(int x, int y, int width, int height) override;

  private:
    static void previousCallback(Fl_Widget *widget, void *data);
    static void nextCallback(Fl_Widget *widget, void *data);
    void layout();

    Fl_Box *title_{};
    Fl_Button *previous_{};
    Fl_Button *next_{};
    PixelCanvas *canvas_{};
    std::function<void()> previousHandler_;
    std::function<void()> nextHandler_;
};
