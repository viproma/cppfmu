from os import path
from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import copy


class CppFmuTestConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        for require, dep in self.dependencies.items():
            if require.build or require.test:
                continue
            if dep.ref.name == "cppfmu" and len(dep.cpp_info.srcdirs) > 0:
                copy(self, "fmi_functions.cpp", dep.cpp_info.srcdirs[0],
                    path.join(self.build_folder, dep.ref.name), keep_path=False)


    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            bin_path = path.join(self.cpp.build.bindir, "tester")
            self.run(bin_path, env="conanrun")
