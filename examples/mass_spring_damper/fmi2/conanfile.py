from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout


class MassSpringDamperFmi2Conan(ConanFile):
    """Consumer recipe for the FMI 2.0 mass-spring-damper example.

    Build it with:  conan build . --build=missing
    """

    name = "mass_spring_damper_fmi2"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    exports_sources = "CMakeLists.txt", "mass_spring_damper.cpp", "modelDescription.xml", "../model.hpp"

    def requirements(self):
        # use_fmi_version=2 is the default, but spelling it out makes the
        # example self-documenting -- and the FMI 3.0 sibling only differs here.
        self.requires("cppfmu/1.3.0@sintef/stable", options={"use_fmi_version": 2})

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
