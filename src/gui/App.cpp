#include "MainFrame.hpp"

#include "dicom_editor/RuntimePaths.hpp"

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

void setEnvIfUnset(const char* name, const char* value)
{
    if (std::getenv(name) == nullptr) {
        setenv(name, value, 1);
    }
}

struct RuntimeEnvironment {
    RuntimeEnvironment()
    {
#ifdef DICOM_EDITOR_INSTALL_DATADIR
        const auto installedFontconfig = dicom_editor::installedDataPath("fontconfig/fonts.conf");
        setEnvIfUnsetOrInvalid("FONTCONFIG_FILE", installedFontconfig);
#if defined(__linux__)
        setEnvIfUnset("GTK_IM_MODULE", "gtk-im-context-simple");
#endif
#endif
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
