CPPFMU Examples
===============

Worked, self-contained co-simulation slaves you can build and package into a
`.fmu`. Each example is a complete consumer project: it has its own
`conanfile.py` and `CMakeLists.txt` and does not depend on the CPPFMU build
tree, so you can copy a directory out of this repository and use it as the
starting point for your own model.

| Example | FMI version | Directory |
|---------|-------------|-----------|
| Mass-spring-damper | 2.0 | [`mass_spring_damper/fmi2/`](mass_spring_damper/fmi2/) |
| Mass-spring-damper | 3.0 | [`mass_spring_damper/fmi3/`](mass_spring_damper/fmi3/) |


Mass-spring-damper
------------------

A single mass on a spring and a damper, driven by an external force:

```
m * x'' + c * x' + k * x = F
```

integrated as a first-order system in position `x` and velocity `v` using
semi-implicit (symplectic) Euler, which stays stable for oscillatory systems.

The physics lives in [`mass_spring_damper/model.hpp`](mass_spring_damper/model.hpp),
which is deliberately FMI-agnostic — it mentions neither CPPFMU nor any FMI
type. Both slaves are thin adapters over that one struct. This is the split
worth copying into your own model: **keep the model separate from the FMI
glue**, so it stays testable on its own and can serve more than one FMI
version.

### Variables

The value references are the contract between the C++ source and
`modelDescription.xml`; they must agree exactly, in both files.

| Value reference | Name | Causality | Unit | Meaning |
|-----------------|------|-----------|------|---------|
| 0 | `mass` | parameter | kg | Mass *m* (must be > 0) |
| 1 | `stiffness` | parameter | N/m | Spring stiffness *k* |
| 2 | `damping` | parameter | N·s/m | Damping coefficient *c* |
| 3 | `force` | input | N | External force *F* |
| 4 | `position` | output | m | Position *x* |
| 5 | `velocity` | output | m/s | Velocity *v* |

In the C++ sources these are the `VR_*` constants at the top of
`mass_spring_damper.cpp`.

### What differs between the FMI 2.0 and FMI 3.0 versions

The two `mass_spring_damper.cpp` files are worth diffing against each other —
the differences are exactly the ones you will hit when porting a model:

| | FMI 2.0 | FMI 3.0 |
|---|---------|---------|
| Header | `cppfmu_cs.hpp` | `cppfmu_cs_fmi3.hpp` |
| Base class | `cppfmu::SlaveInstance` | `cppfmu::SlaveInstance3` |
| Real accessors | `GetReal` / `SetReal` | `GetFloat64` / `SetFloat64`, plus an `nValues` argument |
| `DoStep` | one output parameter | four (event handling, termination, early return, last successful time) |
| Allocation | `cppfmu::AllocateUnique` with a `cppfmu::Memory` | `cppfmu::AllocateUnique3`, plain `new`/`delete` |
| Logger | `cppfmu::Logger` | `std::function<void(FMIStatus, FMIString, FMIString)>` |
| Variable declarations | `<ScalarVariable>` with a nested `<Real>` | one element per type, e.g. `<Float64>` |
| Model identity | `guid` | `instantiationToken` |
| `<ModelStructure>` | refers to variables by 1-based index | refers to them by `valueReference` |

### Building

Both examples consume CPPFMU as a Conan package. Create the package from this
repository first (or add the SINTEF Ocean remote, see the main
[README](../README.md#conan-recipe)):

```sh
conan create . --user sintef --channel stable
```

Then build an example:

```sh
cd examples/mass_spring_damper/fmi2      # or .../fmi3
conan build . --build=missing
```

This produces the FMU shared library — `MassSpringDamper.so` on Linux,
`MassSpringDamper.dll` on Windows, `MassSpringDamper.dylib` on macOS — under
the Conan build folder (`build/Release/` by default). Its base name matches the
`modelIdentifier` in `modelDescription.xml`, which is a requirement, not a
convention.

### Packaging into a `.fmu`

Assemble the archive layout described in the main
[README](../README.md#packaging-a-fmu) and zip it. For the FMI 3.0 example on
64-bit Linux:

```sh
cd examples/mass_spring_damper/fmi3
mkdir -p fmu/binaries/x86_64-linux
cp modelDescription.xml fmu/
cp build/Release/MassSpringDamper.so fmu/binaries/x86_64-linux/
cd fmu && zip -r ../MassSpringDamper.fmu . && cd ..
```

For the FMI 2.0 example the only difference is the platform directory name,
which is `linux64` rather than `x86_64-linux`.

The resulting `MassSpringDamper.fmu` can be loaded by any FMI-compliant
importing tool. Validating it with a checker such as
[fmuCheck](https://github.com/modelica-tools/FMUComplianceChecker) before
handing it to a simulation tool is usually the fastest way to catch a
mismatch between the code and `modelDescription.xml`.
