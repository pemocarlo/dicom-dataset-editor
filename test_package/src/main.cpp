#include <dicom_viewer/operations.hpp>

int main()
{
    const dicom_editor::DicomWorkspace workspace;
    return workspace.size() == 1U ? 0 : 1;
}
