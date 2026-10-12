Changelog
=========

All notable changes to CPPFMU are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
The current version is recorded in `version.txt`.

Entries for releases up to and including 1.2.0 were reconstructed from the Git
history, so they summarise the notable changes rather than being contemporary
release notes.

Unreleased
----------

### Added
- Combined FMI 2.0 + FMI 3.0 build, selected with the Conan option
  `use_fmi_version="all"`. The resulting package ships both C API wrappers and
  sets no version macro, so a single package can serve one consumer that builds
  an FMI 2.0 module and an FMI 3.0 module at the same time.
- `examples/` directory with a mass-spring-damper co-simulation slave for both
  FMI 2.0 and FMI 3.0, each with a matching `modelDescription.xml`, Conan
  recipe and `CMakeLists.txt`.

### Changed
- FMI 3.0 lifecycle handling extracted into a separate, testable `Lifecycle`
  module (`cppfmu_lifecycle_fmi3.{hpp,cpp}`). It holds the whole FMI 3.0
  Co-Simulation state graph in one transition table and is FMI 3.0 only; the
  FMI 1.0/2.0 implementation has no state machine.
- FMI 1.0/2.0 C API dispatch centralised behind a `RunCall` helper, removing the
  repeated exception-handling boilerplate from every wrapper function.

### Fixed
- The Conan `generate()` snippet in the README no longer has its `if` statement
  dedented out of the enclosing `for` loop, which silently prevented the C API
  wrapper from being copied; and the file it names is `fmi_functions.cpp`, not
  `fmi_function.cpp`.

### Documentation
- Documented the Conan `use_fmi_version` option values (`1`, `2`, `3`, `"all"`).
- Documented consuming the package through the `cppfmu::cppfmu` CMake target.
- Added the FMI 3.0 `CppfmuInstantiateSlave()` signature to the README.
- Added a section on assembling a `.fmu` archive (ZIP layout, platform
  directory names, `modelIdentifier` matching).

1.2.0 — 2026-05-04
------------------

### Added
- FMI 3.0 co-simulation support through a new `cppfmu::SlaveInstance3` class
  (`cppfmu_cs_fmi3.{hpp,cpp}`) and C API wrapper (`fmi3_functions.cpp`), with
  virtual methods for the typed Get/Set functions, FMU state management,
  directional and adjoint derivatives, and the extended `DoStep`.
- Event Mode support for FMI 3.0 (`EnterEventMode`, `EvaluateDiscreteStates`,
  `UpdateDiscreteStates`, `EnterStepMode`).
- Directional derivative and input/output derivative APIs for FMI 1.0 and 2.0,
  with test coverage.
- Test coverage for the FMI 3.0 implementation, including the Binary type.

### Removed
- A non-standard FMI 1.0 function that had no counterpart in the specification.

### Fixed
- Several FMI 3.0 defects found during review: a duplicate null guard in
  `fmi3FreeInstance`, an incorrect lambda capture in
  `fmi3InstantiateCoSimulation`, and missing null-instance validation across the
  FMI 3.0 entry points.

1.1.1 — 2026-04-13
------------------

### Changed
- CI maintenance update.

1.1.0 — 2025-10-31
------------------

### Changed
- CI scripts updated to use new build images.

1.0.0 — 2024-04-16
------------------

### Added
- Conan recipe for building and distributing CPPFMU as a package.
- Basic test suite.
- FMU state saving and serialisation support for FMI 2.0.

### Fixed
- Incorrect use of `componentEnvironment` in FMI 2.0.
- Typo in the use of `FMICallbackFunctions`.

Earlier history
---------------

CPPFMU was first published in 2017 with FMI 1.0 co-simulation support, and
gained FMI 2.0 support in 2019. See the Git history for details.
