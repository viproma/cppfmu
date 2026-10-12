CPPFMU
======

CPPFMU is a set of interfaces and helper functions for writing
[FMI](https://www.fmi-standard.org)-compliant model/slave code
in C++.

CPPFMU lets you write object-oriented C++ code, using high-level
features such as exceptions and automatic memory management,
rather than implement the low-level C functions specified by FMI.

Currently, only FMI for Co-simulation is supported. Support for Model
Exchange is planned.  (Therefore, we'll continue to write "model/slave
code", even though it's only the latter which is relevant for the time
being.)

CPPFMU was developed as part of the R&D project [Virtual Prototyping
of Maritime Systems and Operations](http://viproma.no) (ViProMa), and
is currently maintained by [SINTEF Ocean](http://www.sintef.no/en/ocean/).

Supported FMI Versions
----------------------

| Version | Status | Header | Library Source | C API Wrapper |
|---------|--------|--------|----------------|---------------|
| FMI 1.0 | Stable | `cppfmu_cs.hpp` | `cppfmu_cs.cpp` | `fmi_functions.cpp` |
| FMI 2.0 | Stable | `cppfmu_cs.hpp` | `cppfmu_cs.cpp` | `fmi_functions.cpp` |
| FMI 3.0 | Partial | `cppfmu_cs_fmi3.hpp` | `cppfmu_cs_fmi3.cpp`, `cppfmu_lifecycle_fmi3.cpp` | `fmi3_functions.cpp` |

### FMI 3.0 Support Status

FMI 3.0 co-simulation is supported. The `SlaveInstance3` class provides virtual
methods for all Co-Simulation API functions. Functions without a corresponding
virtual method are hardcoded stubs that always return `fmi3Error`.

**Fully implemented (virtual methods on `SlaveInstance3`):**
- All Get/Set type functions (Float32, Float64, Int8–64, UInt8–64, Boolean, String, Binary)
- Directional derivatives, adjoint derivatives, and output derivatives
- Variable dependencies and number of dependencies
- FMU state management (get/set/serialize/deserialize)
- Enhanced `DoStep` with early return, event handling, and termination output parameters
- Event Mode (`EnterEventMode`, `EvaluateDiscreteStates`, `UpdateDiscreteStates`, `EnterStepMode`)
- `GetNumberOfEventIndicators` / `GetNumberOfContinuousStates`

**Genuine stubs (no virtual method — cannot be overridden by users):**
- **Configuration Mode** — `fmi3EnterConfigurationMode` / `fmi3ExitConfigurationMode`
  (optional feature for structural parameter tuning)
- **Clock interval/shift** — `Get/SetIntervalDecimal/Fraction`, `Get/SetShiftDecimal/Fraction`
  (optional feature for variable-interval and shifted clocks)
- **Intermediate Update Callback** — parameter accepted during instantiation but ignored

Debug logging with categories (`fmi3SetDebugLogging`) is handled entirely by the
C API wrapper, so there is no virtual method for it.

**Note:** `GetClock` and `SetClock` have virtual methods on `SlaveInstance3` and can be
overridden by users; the default implementation throws `std::logic_error` for non-zero
variable references, consistent with other type-specific Get/Set methods.

### FMI 3.0 lifecycle enforcement

FMI 3.0 prescribes which calls are legal in which mode, and CPPFMU enforces
this for you. The `cppfmu::Lifecycle` class
(`cppfmu_lifecycle_fmi3.{hpp,cpp}`) holds the whole Co-Simulation state graph
in one transition table: a call made in a state that does not allow it is
refused with `fmi3Error` and never reaches your slave, and a call that throws
something other than a refusal puts the instance in a failed state, from which
only freeing, resetting or restoring a saved FMU state escapes. Your slave code
therefore does not need to check the current mode itself.

This applies to FMI 3.0 only; the FMI 1.0/2.0 implementation has no state
machine. When you compile CPPFMU by hand rather than through Conan, remember to
compile `cppfmu_lifecycle_fmi3.cpp` alongside `cppfmu_cs_fmi3.cpp`.

### Why `SlaveInstance3` Instead of Extending `SlaveInstance`?

FMI 3.0 introduces fundamental changes that make extending the existing
`SlaveInstance` class impractical:

1. **Different type system** — FMI 3.0 replaces `fmi2Real`/`fmi2Integer` with
   typed variants (Float32, Float64, Int8–64, UInt8–64, Binary, Clock) and
   adds an `nValues` parameter to every Get/Set function for array-aware access.

2. **Different memory model** — FMI 3.0 removes the FMI memory allocator
   callbacks entirely, requiring standard C++ `new`/`delete` instead of the
   `cppfmu::Memory`/`cppfmu::Allocator` infrastructure used by FMI 1.0/2.0.

3. **Different logger signature** — FMI 3.0 replaces the varargs-based logger
   with a fixed-signature callback, requiring a separate `Logger3` class.

4. **Different instantiation** — `fmi3InstantiateCoSimulation` has a different
   parameter set (eventModeUsed, earlyReturnAllowed, intermediateUpdate callback)
   that doesn't map to the existing `CppfmuInstantiateSlave` signature.

5. **Enhanced DoStep** — FMI 3.0's `DoStep` returns four additional output
   parameters (`eventHandlingNeeded`, `terminateSimulation`, `earlyReturn`,
   `lastSuccessfulTime`) that have no analogue in FMI 2.0.

Maintaining backward compatibility for FMI 1.0/2.0 users while accommodating
these changes would have required either extensive conditional compilation
within a single class or a proliferation of overloaded virtual methods.
A separate `SlaveInstance3` class keeps each FMI version's interface clean
and self-documenting.

Getting involved
----------------
If you have a question, a bug report or an enhancement request,
please use the [GitHub issue tracker](https://github.com/viproma/cppfmu/issues).
We appreciate if you do a quick search first to see if anyone has
already brought the issue up, and we will also be very happy if
you label your issue appropriately.

Contributions are very welcome, and should be submitted as
[pull requests on GitHub](https://github.com/viproma/cppfmu/pulls).

Notable changes per release are recorded in [`CHANGELOG.md`](CHANGELOG.md).


Building
--------

CPPFMU can be used by just including the headers in your model/slave
code and compile the `.cpp` files together with yours, and you're good to go.
A good tip is to include the CPPFMU repository as a Git submodule in
your own repository.

Note that this repository does not contain the FMI header files;
you'll have to get them yourself and make sure your compiler can
find them.

You'll also need a compiler with decent C++11 support.  The
following are known to work (and newer versions of these ought to
work as well):

  * Microsoft Visual C++ 12.0 (Visual Studio 2013)
  * GCC 4.9

### Conan recipe

We also provide a [Conan](https://conan.io) recipe. This recipe builds a static library
for CPPFMU that you can use in your code, but you still need to compile
`fmi_functions.cpp` (or `fmi3_functions.cpp` for FMI 3.0). The package can be created
with `conan create . --user sintef --channel stable`. The recipe and some precompiled
binaries are available on Sintef Ocean's public artifactory, which can be added with
`conan remote add sintef-public https://gitlab.sintef.no/api/v4/projects/22218/packages/conan`.
Note that when using the conan recipe, FMI 1, 2, or 3 is added as a dependency, so you
do not need to fetch them yourself.

The FMI version is selected with the `use_fmi_version` option, which accepts:

| Value   | Meaning                                                              |
|---------|---------------------------------------------------------------------|
| `1`     | FMI 1.0 (`fmi_functions.cpp`, defines `CPPFMU_USE_FMI_1_0`)          |
| `2`     | FMI 2.0 (`fmi_functions.cpp`, the default)                          |
| `3`     | FMI 3.0 (`fmi3_functions.cpp`, defines `CPPFMU_USE_FMI_3_0`)         |
| `"all"` | Combined FMI 2.0 + 3.0 in one library (ships both C API wrappers)    |

To use CPPFMU with conan, add the following lines to your `conanfile.py` and
`CMakeLists.txt`. The example below uses FMI 3.0; change the option value and
the copied `fmi*_functions.cpp` filename for other versions.

`conanfile.py`:
```python
from os import path
from conan.tools.files import copy

  ...
  def requirements(self):
      self.requires("cppfmu/1.3.0@sintef/stable", options={"use_fmi_version": 3})

  def generate(self):
      # Copy the C API wrapper into your build folder so you can compile it
      # together with your model sources (see CMakeLists.txt below).
      for require, dep in self.dependencies.items():
          if require.build or require.test:
              continue
          if dep.ref.name == "cppfmu":
              copy(self, "fmi3_functions.cpp",  # "fmi_functions.cpp" for FMI 1 or 2
                  dep.cpp_info.srcdirs[0],
                  path.join(self.build_folder, dep.ref.name),
                  keep_path=False)
```

`CMakeLists.txt`:
```cmake
find_package(cppfmu REQUIRED)

# An FMU binary is a shared library (MODULE), built from your model sources
# plus the C API wrapper copied in by the conanfile generate() step above.
# Use fmi3_functions.cpp for FMI 3.0, fmi_functions.cpp for FMI 1.0/2.0.
add_library(FmuModuleTarget MODULE
    ${fmuSourceFiles}
    ${CMAKE_BINARY_DIR}/cppfmu/fmi3_functions.cpp
    )
target_link_libraries(FmuModuleTarget PUBLIC cppfmu::cppfmu)
```

Instead of copying the wrapper, you can compile it straight out of the package
folder, which CMakeDeps exposes as `cppfmu_PACKAGE_FOLDER_RELEASE` (or
`..._DEBUG`); then `generate()` needs no `copy()` at all. Both approaches work —
[`examples/`](examples/) uses the direct one, and
`test_package/CMakeLists.txt` shows it for the combined package.

The `cppfmu::cppfmu` target carries the FMI include paths and the correct
`CPPFMU_USE_FMI_*` define for the version you selected, so you do not need to
set any of those yourself. (The `"all"` combined package is the exception: it
sets no version macro, so each of your modules must define `CPPFMU_USE_FMI_3_0`
itself when it compiles the FMI 3.0 wrapper, and define nothing for FMI 2.0.
See `test_package/CMakeLists.txt` for a worked combined-package example.)

How it works
------------
It's simple: We have already implemented all the FMI C functions
for you in `fmi_functions.cpp` (or `fmi3_functions.cpp` for FMI 3.0).
These forward to the C++ functions defined by you.  They also ensure
that exceptions are caught, logged and turned into the appropriate
error codes.

Packaging a `.fmu`
------------------
CPPFMU builds the shared library that goes *inside* an FMU; it does not
assemble the FMU archive itself. An `.fmu` file is just a ZIP archive with a
fixed layout:

```
MyModel.fmu   (a ZIP archive)
├── modelDescription.xml           # required; describes variables, VRs, capabilities
├── binaries/
│   ├── x86_64-linux/MyModel.so     # FMI 3.0 platform dir names
│   ├── x86_64-windows/MyModel.dll
│   └── x86_64-darwin/MyModel.dylib
└── resources/                      # optional; files your model loads at runtime
```

A few things to get right:

  * The shared-library base name (`MyModel`) must match the
    `modelIdentifier` attribute in `modelDescription.xml`.
  * The platform subdirectory names differ between FMI versions. FMI 2.0 uses
    names like `linux64`, `win64`, `darwin64`; FMI 3.0 uses the target-triple
    style shown above (`x86_64-linux`, `x86_64-windows`, `aarch64-darwin`, …).
    Check the relevant FMI specification for the exact list.
  * Build the library as a `MODULE` (see the CMake snippet above) so it is
    loadable at runtime by the importing tool.

Once the tree is assembled, zip it (with the files at the archive root, not
nested in a top-level folder) and rename to `.fmu`:

```sh
cd MyModel && zip -r ../MyModel.fmu . && cd ..
```

See [`examples/`](examples/) for complete `modelDescription.xml` files and
end-to-end build + packaging steps.

Usage
-----

### FMI 1.0 and FMI 2.0

To implement a *co-simulation slave*, this is what you have to do:

  1. Include the `cppfmu_cs.hpp` header in your sources.

  2. Create a class that publicly derives from `cppfmu::SlaveInstance`,
     and override the latter's virtual member functions as required.
     Look at the class definition in the header file for information
     about them.

  3. Define the function `CppfmuInstantiateSlave()` so that it
     creates and returns instances of your slave class.  This function
     is declared in the header file, in the global namespace, and you
     must define it with the exact same signature.  You'll find more
     information about this in the header too.

### FMI 3.0

  1. Include the `cppfmu_cs_fmi3.hpp` header in your sources.

  2. Create a class that publicly derives from `cppfmu::SlaveInstance3`,
     and override its virtual member functions as required.  Note that
     all Get/Set methods include an `nValues` parameter for array-aware
     access, and memory management uses standard C++ `new`/`delete`
     (use `cppfmu::AllocateUnique3` instead of `cppfmu::AllocateUnique`).

  3. Define the function `CppfmuInstantiateSlave()` with the FMI 3.0
     signature. It is declared in `cppfmu_cs_fmi3.hpp`, in the global
     namespace, and takes the FMI 3.0 instantiation parameters:

     ```cpp
     cppfmu::UniquePtr<cppfmu::SlaveInstance3> CppfmuInstantiateSlave(
         cppfmu::FMIString instanceName,
         cppfmu::FMIString instantiationToken,
         cppfmu::FMIString resourceLocation,
         cppfmu::FMIBoolean visible,
         cppfmu::FMIBoolean loggingOn,
         cppfmu::FMIBoolean eventModeUsed,
         cppfmu::FMIBoolean earlyReturnAllowed,
         const cppfmu::FMIValueReference requiredIntermediateVariables[],
         std::size_t nRequiredIntermediateVariables,
         cppfmu::FMIComponentEnvironment instanceEnvironment,
         std::function<void(cppfmu::FMIStatus, cppfmu::FMIString, cppfmu::FMIString)> logger);
     ```

Complete, runnable models for both FMI 2.0 and FMI 3.0 (a mass-spring-damper,
with a matching `modelDescription.xml` and build files) are provided in the
[`examples/`](examples/) directory.

That's more or less it. Read on below to learn how to deal with errors,
memory management, and logging.

### Error handling

CPPFMU uses exceptions to signal errors, and expects the same of
model/slave code.  Any CPPFMU function which is not marked with
`CPPFMU_NOEXCEPT` may throw.  All exceptions are required to derive
from `std::exception`.

If an exception is thrown from within model/slave code, the model/slave
instance in question is considered unusable, and no further calls
will be made to any of its member functions except the destructor.

The message associated with the exception will be logged using the
mechanism provided by the simulation environment (the `logger` callback
in the C API), and the currently executing FMI function will return an
error code.  For most exception types this will be `fmiError` (or
`fmi3Error` for FMI 3.0).

If the nature of the error is such that all other instances have also
become unusable, an exception of type `cppfmu::FatalError` (or a derived
type) must be thrown instead.  This will result in an `fmiFatal` error
code.

`CPPFMU_NOEXCEPT` and `cppfmu::FatalError` are both defined in
`cppfmu_common.hpp`.

### Memory management

FMI 1.0 specifies that *all* memory allocations and deallocations
must be performed using functions provided by the simulation
environment.  This is a tall order for some high-level programming
languages, so it was relaxed in FMI 2.0, which lets you specify the
`canNotUseMemoryManagementFunctions` flag in `modelDescription.xml`.

In C++ it is certainly possible to use custom memory allocation
mechanisms, but in high-level code that uses containers, `std::string`,
smart pointers and so on, it is somewhat of a hassle, as they all use
the built-in operator `new` by default.  CPPFMU aims to make this a
bit easier.

The `cppfmu_common.hpp` header contains a set of classes and functions
that wrap the low-level FMI memory management callbacks in a
higher-level C++ interface.  These are:

  * `cppfmu::Memory`, which simply bundles the two FMI callbacks
    (`allocateMemory` and `freeMemory`) in a single object.
    This is passed to `CppfmuInstantiateSlave()`, and must be further
    passed on to any code that needs to allocate memory. All of the
    following classes and functions need to be passed a `Memory` object.

  * `cppfmu::Allocator`, a class which satisfies the C++ Allocator
    concept, and which can therefore be used to manage memory for the
    standard container classes, among others.

  * `cppfmu::String`, which is a type alias for `std::basic_string`
    using the above allocator, and the convenience function
    `cppfmu::CopyString()`, which creates a `String` from a C-style
    character array.

  * `cppfmu::New` and `cppfmu::Delete`, which are more or less
    equivalent to the built-in operators `new` and `delete`.

  * `cppfmu::UniquePtr`, which is a type alias for `std::unique_ptr`
    with a custom deleter, and `cppfmu::AllocateUnique`, which
    allocates and constructs an object managed by a `UniquePtr`.

**FMI 3.0** does not provide memory allocator callbacks.  Use standard
C++ `new`/`delete` directly, or the convenience function
`cppfmu::AllocateUnique3` defined in `cppfmu_cs_fmi3.hpp`.

### Logging

FMI includes a logging mechanism which model/slave code can use to
pass messages to the simulation environment.  Mostly, this is used
for error messages, but those are handled automatically by CPPFMU's
exception handling mechanism.  For other cases, such as debugging
information, you can use `cppfmu::Logger`.

An object of this type is passed to `CppfmuInstantiateSlave()` and
must be passed on to any code that is to perform logging.

The `Logger` class is defined and documented in `cppfmu_common.hpp`.

**FMI 3.0** uses a different logger signature (no varargs, no instance
name parameter).  A `Logger3` class is provided in `cppfmu_common.hpp`
for this purpose.

Licence
-------
CPPFMU is subject to the terms of the [Mozilla Public License, v.
2.0](https://www.mozilla.org/MPL/2.0/).  For easily-understandable
information about what this means for you, check out the
[MPL 2.0 FAQ](https://www.mozilla.org/MPL/2.0/FAQ/).
