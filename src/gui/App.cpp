#include "MainFrame.hpp"

#include <wx/app.h>

#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

void setEnvIfUnsetOrInvalid(const char* name, const std::filesystem::path& value)
{
    const char* current = std::getenv(name);
    if ((current == nullptr || !std::filesystem::exists(current)) && std::filesystem::exists(value)) {
        setenv(name, value.string().c_str(), 1);
    }
}

struct RuntimeEnvironment {
    RuntimeEnvironment()
    {
#ifdef DICOM_EDITOR_FONTCONFIG_PATH
        setEnvIfUnsetOrInvalid("FONTCONFIG_PATH", DICOM_EDITOR_FONTCONFIG_PATH);
#endif
#ifdef DICOM_EDITOR_GDK_PIXBUF_PIXDATA
        setEnvIfUnsetOrInvalid("GDK_PIXBUF_PIXDATA", DICOM_EDITOR_GDK_PIXBUF_PIXDATA);
#endif
    }
};

RuntimeEnvironment runtimeEnvironment;

} // namespace

class DicomDatasetEditorApp final : public wxApp {
public:
    bool OnInit() override
    {
        auto* frame = new MainFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(DicomDatasetEditorApp);
