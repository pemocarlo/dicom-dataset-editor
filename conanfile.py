import os
import shutil

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeConfigDeps, CMakeToolchain, cmake_layout


class DicomDatasetEditorRecipe(ConanFile):
    required_conan_version = ">=2.28"
    name = "dicom_dataset_editor"
    version = "0.1.0"
    package_type = "application"

    settings = "os", "arch", "compiler", "build_type"
    exports_sources = (
        "CMakeLists.txt",
        "CMakePresets.json",
        "cmake/*",
        "include/*",
        "src/*",
        "tests/*",
    )

    requires = ("dcmtk/3.7.0", "fltk/1.4.5")

    def build_requirements(self):
        self.tool_requires("cmake/4.3.2")
        self.tool_requires("cppcheck/2.20.0")

    def validate(self):
        check_min_cppstd(self, "23")

    def layout(self):
        cmake_layout(self, build_folder="build")
        self.folders.build = os.path.join("build", str(self.settings.build_type))
        self.folders.generators = os.path.join(self.folders.build, "generators")

    def generate(self):
        deps = CMakeConfigDeps(self)
        deps.generate()

        toolchain = CMakeToolchain(self)
        toolchain.user_presets_path = False
        toolchain.cache_variables["BUILD_TESTING"] = not self.conf.get("tools.build:skip_test", default=False)
        dcmtk = self.dependencies["dcmtk"]
        dict_file = os.path.join(
            dcmtk.package_folder,
            "bin",
            "share",
            f"dcmtk-{dcmtk.ref.version}",
            "dicom.dic",
        )
        if not os.path.isfile(dict_file):
            raise ConanInvalidConfiguration(f"DCMTK dictionary not found: {dict_file}")
        embedded_dict_file = os.path.join(self.generators_folder, "dicom.dic")
        os.makedirs(self.generators_folder, exist_ok=True)
        shutil.copyfile(dict_file, embedded_dict_file)
        toolchain.cache_variables["DICOM_EDITOR_DCMTK_DICT_FILE"] = embedded_dict_file
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if not self.conf.get("tools.build:skip_test", default=False):
            cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()
