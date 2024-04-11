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

Getting involved
----------------
If you have a question, a bug report or an enhancement request,
please use the [GitHub issue tracker](https://github.com/viproma/cppfmu/issues).
We appreciate if you do a quick search first to see if anyone has
already brought the issue up, and we will also be very happy if
you label your issue appropriately.

Contributions are very welcome, and should be submitted as
[pull requests on GitHub](https://github.com/viproma/cppfmu/pulls).


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
`fmi_functions.cpp`. The package can be created with `conan create . --user sintef
--channel stable`. The recipe and some precompiled binaries are available on Sintef
Ocean's public artifactory], which can be added with `conan remote add sintef-public
https://artifactory.smd.sintef.no/artifactory/api/conan/conan-local`. Note that when using
the conan recipe, FMI 1 or 2 is added as a dependency, so you do not need to fetch them
yourself. To use CPPFMU with conan, add the following lines to your `conanfile.py` and
`CMakeLists.txt`:

`conanfile.py`:
```python
  ...
  def requirements(self):
      self.requires("cppfmu/1.0@sintef/stable")

  def generate(self):
      # Copy fmi_function.cpp to your binary directory
      for require, dep in self.dependencies.items():
          if require.build or require.test:
              continue
      if dep.ref.name == "cppfmu":
          copy(self, "fmi_functions.cpp",
              dep.cpp_info.srcdirs[0],
              path.join(self.build_folder, dep.ref.name),
              keep_path=False)
```

`CMakeLists.txt`:
```cmake
  find_package(cppfmu REQUIRED)
  add_library(FmuModuleTarget MODULE
    ${fmuSourceFiles}
    ${CMAKE_BINARY_DIR}/cppfmu/fmi_functions.cpp
    )
  target_link_libraries(FmuModuleTarget PUBLIC cppfmu::cppfmu)
```

How it works
------------
It's simple: We have already implemented all the FMI C functions
for you in `fmi_functions.cpp`.  These forward to the C++ functions
defined by you.  They also ensure that exceptions are caught,
logged and turned into the appropriate error codes.

Usage
-----
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
error code.  For most exception types this will be `fmiError`.

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

### Logging

FMI includes a logging mechanism which model/slave code can use to
pass messages to the simulation environment.  Mostly, this is used
for error messages, but those are handled automatically by CPPFMU's
exception handling mechanism.  For other cases, such as debugging
information, you can use `cppfmu::Logger`.

An object of this type is passed to `CppfmuInstantiateSlave()` and
must be passed on to any code that is to perform logging.

The `Logger` class is defined and documented in `cppfmu_common.hpp`.

Licence
-------
CPPFMU is subject to the terms of the [Mozilla Public License, v.
2.0](https://www.mozilla.org/MPL/2.0/).  For easily-understandable
information about what this means for you, check out the
[MPL 2.0 FAQ](https://www.mozilla.org/MPL/2.0/FAQ/).
