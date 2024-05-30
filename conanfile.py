from os import path
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.env import VirtualRunEnv
from conan.tools.scm import Git, Version
from conan.tools.files import copy, load, update_conandata

required_conan_version = ">=2.0.15"


class CppFmuConan(ConanFile):
    name = "cppfmu"
    license = "MPL-2.0"
    author = "Lars T. Kyllingstad"
    description = "C++ wrapper for FMI co-simulation, v1 and v2"
    topics = ("Co-simulation")
    url = "https://github.com/viproma/cppfmu"
    settings = "os", "compiler", "build_type", "arch"
    package_type = "static-library"
    options = {
        "fPIC": [True, False],
        "use_fmi_version": [1, 2]
    }
    default_options = {
        "fPIC": True,
        "use_fmi_version": 2
    }

    @property
    def _min_cppstd(self):
        return 11

    @property
    def _compilers_minimum_version(self):
        return {
            "apple-clang": "5.0",
            "clang": "3.3",
            "gcc": "4.9",
            "msvc": "170",
        }

    def export(self):
        copy(self, "version.txt", self.recipe_folder, self.export_folder)
        git = Git(self, self.recipe_folder)
        scm_url, scm_commit = git.get_url_and_commit()
        update_conandata(self, {"sources": {"commit": scm_commit, "url": scm_url}})

    def set_version(self):
        self.version = \
            load(self, path.join(self.recipe_folder, "version.txt")).strip()

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        if self.options.use_fmi_version == 1:
            self.requires("fmi1/1.0.1", transitive_headers=True)
        else:
            self.requires("fmi2/2.0.4", transitive_headers=True)

    def validate(self):
        if self.settings.compiler.cppstd:
            check_min_cppstd(self, self._min_cppstd)
        minimum_version = self._compilers_minimum_version.get(str(self.settings.compiler), False)
        if minimum_version and Version(self.settings.compiler.version) < minimum_version:
            raise ConanInvalidConfiguration(
                f"{self.ref} requires C++{self._min_cppstd}, which your compiler does not support."
            )

    def source(self):
        git = Git(self)
        sources = self.conan_data["sources"]
        git.clone(url=sources["url"], target=".")
        git.checkout(commit=sources["commit"])

    def generate(self):
        tc = CMakeToolchain(self)
        if self.options.use_fmi_version == 1:
            tc.variables["CPPFMU_FMI_1"] = True
        else:
            tc.variables["CPPFMU_FMI_1"] = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.ctest("--output-on-failure")

    def package(self):
        copy(self, "LICENCE.txt", self.source_folder,
             path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["cppfmu"]
        self.cpp_info.srcdirs = ["src"]
        if self.options.use_fmi_version == 1:
            self.output.info("Define fmi1")
            self.cpp_info.defines = ["CPPFMU_USE_FMI_1_0=1"]
            self.cpp_info.requires = ["fmi1::cosim"]
        else:
            self.cpp_info.requires = ["fmi2::fmi2"]
