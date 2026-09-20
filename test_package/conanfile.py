import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeConfigDeps, CMakeToolchain, cmake_layout


class DicomViewerTestPackage(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def build_requirements(self):
        self.tool_requires("cmake/4.3.2")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeConfigDeps(self).generate()
        toolchain = CMakeToolchain(self)
        toolchain.user_presets_path = False
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            executable = os.path.join(self.cpp.build.bindir, "dicom_viewer_package_test")
            self.run(f'cmake -E env --unset=DCMDICTPATH "{executable}"', env="conanrun")
