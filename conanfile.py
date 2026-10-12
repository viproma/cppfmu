from os import path, environ
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
    description = "C++ wrapper for FMI co-simulation, v1, v2, and v3"
    topics = ("Co-simulation",)
    url = "https://github.com/viproma/cppfmu"
    settings = "os", "compiler", "build_type", "arch"
    package_type = "static-library"
    options = {
        "fPIC": [True, False],
        "use_fmi_version": [1, 2, 3, "all"]
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

    def _commit_is_on(self, git, commit, remote):
        """Is `commit` reachable from a branch of `remote` in this checkout?

        Conan's own Git.commit_in_remote() answers "is it on *any* remote", so a
        commit pushed only to a fork makes it record the origin URL, and the
        resulting package fails to clone. This asks about one named remote.
        """
        branches = git.run(f'branch -r --contains {commit} --list "{remote}/*"')
        return bool(branches.strip())

    def export(self):
        copy(self, "version.txt", self.recipe_folder, self.export_folder)
        git = Git(self, self.recipe_folder)
        if environ.get("GITHUB_ACTIONS") == "true":
            scm_commit = environ.get("GITHUB_SHA")
            scm_url = f"{environ.get('GITHUB_SERVER_URL')}/{environ.get('GITHUB_REPOSITORY')}"
        elif environ.get("GITLAB_CI") == "true":
            # The GitLab project only builds; the source of record is the URL the
            # pipeline names in CPPFMU_SOURCE_URL, which must hold CI_COMMIT_SHA.
            scm_commit = environ["CI_COMMIT_SHA"]
            scm_url = environ["CPPFMU_SOURCE_URL"]
        else:
            if git.is_dirty():
                raise ConanInvalidConfiguration(
                    f"Repo is dirty, cannot capture url and commit: {git.folder}")
            scm_commit = git.get_commit()
            scm_url = git.get_remote_url()
            if not self._commit_is_on(git, scm_commit, "origin"):
                # Unpushed, or pushed only to a fork: clone this checkout instead, so
                # a local `conan create` is buildable. Such a package is reproducible
                # only on this machine -- publish it only after pushing to origin.
                self.output.warning(
                    f"Commit {scm_commit} is not on origin; recording the local "
                    "checkout as the source URL. Not buildable elsewhere.")
                scm_url = git.get_repo_root()

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
        elif self.options.use_fmi_version == 3:
            self.requires("fmi3/3.0.2", transitive_headers=True)
        elif self.options.use_fmi_version == "all":
            # Combined build: ship both FMI 2.0 and FMI 3.0 so one package can serve
            # a consumer that builds an FMI 2.0 module and an FMI 3.0 module at once.
            self.requires("fmi2/2.0.4", transitive_headers=True)
            self.requires("fmi3/3.0.2", transitive_headers=True)
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
        tc.variables["CPPFMU_FMI_1"] = self.options.use_fmi_version == 1
        tc.variables["CPPFMU_FMI_3"] = self.options.use_fmi_version == 3
        tc.variables["CPPFMU_FMI_ALL"] = self.options.use_fmi_version == "all"
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.ctest(cli_args=["--output-on-failure"])

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
        elif self.options.use_fmi_version == 3:
            self.output.info("Define fmi3")
            self.cpp_info.defines = ["CPPFMU_USE_FMI_3_0=1"]
            self.cpp_info.requires = ["fmi3::fmi3"]
        elif self.options.use_fmi_version == "all":
            # Combined build ships both FMI 2.0 and FMI 3.0. No version macro is set
            # here on purpose: cppfmu_common.hpp selects the FMI version by macro, so
            # a single package-wide define would force every consuming translation
            # unit onto one version. Each consuming module sets the macro itself --
            # an FMI 3.0 module compiles with CPPFMU_USE_FMI_3_0, an FMI 2.0 module
            # with neither -- and links the one static library, which carries the
            # out-of-line symbols of both SlaveInstance (FMI 2.0) and SlaveInstance3
            # (FMI 3.0).
            self.output.info("Define fmi2 + fmi3 (combined)")
            self.cpp_info.requires = ["fmi2::fmi2", "fmi3::fmi3"]
        else:
            self.cpp_info.requires = ["fmi2::fmi2"]
