from conan import ConanFile
from conan.tools.cmake import CMakeConfigDeps, CMakeToolchain, cmake_layout


class DicomDatasetEditorRecipe(ConanFile):
    name = "dicom_dataset_editor"
    version = "0.1.0"
    package_type = "application"

    settings = "os", "arch", "compiler", "build_type"

    options = {"gui": ["fltk", "wxwidgets"]}
    default_options = {"gui": "fltk"}

    requires = ("dcmtk/3.7.0",)

    def requirements(self):
        if self.options.gui == "fltk":
            self.requires("fltk/1.4.5")
        else:
            self.requires("wxwidgets/3.3.2")

    def layout(self):
        cmake_layout(self, build_folder="build")

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.variables["DICOM_EDITOR_GUI"] = str(self.options.gui)
        toolchain.user_presets_path = False
        toolchain.generate()

        deps = CMakeConfigDeps(self)
        deps.generate()
