from conan import ConanFile
from conan.tools.cmake import CMakeConfigDeps, CMakeToolchain, cmake_layout


class DicomDatasetEditorRecipe(ConanFile):
    name = "dicom_dataset_editor"
    version = "0.1.0"
    package_type = "application"

    settings = "os", "arch", "compiler", "build_type"

    requires = (
        "wxwidgets/3.3.2",
        "dcmtk/3.7.0",
    )

    def layout(self):
        cmake_layout(self, build_folder="build")

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.user_presets_path = False
        toolchain.generate()

        deps = CMakeConfigDeps(self)
        deps.generate()
