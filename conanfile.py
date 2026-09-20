import os
import shutil

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeConfigDeps, CMakeToolchain, cmake_layout


class DicomViewerRecipe(ConanFile):
    required_conan_version = ">=2.28"
    name = "dicom_viewer"
    version = "0.1.0"
    package_type = "library"

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_gui": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_gui": False,
    }
    exports_sources = (
        "CMakeLists.txt",
        "CMakePresets.json",
        "cmake/*",
        "include/*",
        "src/*",
        "tests/*",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        # DCMTK types are intentionally part of the public operations API, so
        # consumers need its headers and libraries even when this package is shared.
        self.requires("dcmtk/3.7.0", transitive_headers=True, transitive_libs=True)
        if self.options.with_gui:
            self.requires("fltk/1.4.5")

    def build_requirements(self):
        if not self.conf.get("tools.build:skip_test", default=False):
            self.test_requires("catch2/3.15.1")
        self.tool_requires("cmake/4.3.2")
        self.tool_requires("cppcheck/2.20.0")
        generator = self.conf.get("tools.cmake.cmaketoolchain:generator", default=None)
        if generator == "Ninja":
            self.tool_requires("ninja/1.13.2")

    def validate(self):
        check_min_cppstd(self, "23")

    def layout(self):
        cmake_layout(self, build_folder="build")
        default_build_folder = os.path.join("build", str(self.settings.build_type))
        self.folders.build = self.conf.get(
            "user.dicom_dataset_editor:build_folder",
            default=default_build_folder,
        )
        self.folders.generators = os.path.join(self.folders.build, "generators")

    def generate(self):
        deps = CMakeConfigDeps(self)
        deps.generate()

        toolchain = CMakeToolchain(self)
        toolchain.user_presets_path = False
        toolchain.cache_variables["BUILD_TESTING"] = not self.conf.get("tools.build:skip_test", default=False)
        toolchain.cache_variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        toolchain.cache_variables["DICOM_EDITOR_BUILD_FLTK"] = bool(self.options.with_gui)
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

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "DicomViewer")
        self.cpp_info.set_property("cmake_target_name", "DicomViewer::operations")
        self.cpp_info.libs = ["dicom_viewer_operations"]
        self.cpp_info.requires = ["dcmtk::dcmtk"]
        if self.options.with_gui:
            # FLTK is linked only into the packaged executable, not the public library.
            self.cpp_info.ignored_requires = ["fltk"]
