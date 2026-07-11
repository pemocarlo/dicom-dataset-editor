#include "EditorWindow.hpp"

#include <FL/Enumerations.H>
#include <FL/Fl.H>

int main() {
    FL_NORMAL_SIZE = 14;
    Fl::scheme("gtk+");
    Fl::background(238, 243, 247);
    Fl::background2(252, 253, 254);
    Fl::foreground(31, 46, 58);
    EditorWindow window;
    window.show();
    return Fl::run();
}
