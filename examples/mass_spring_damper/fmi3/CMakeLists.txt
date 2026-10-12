cmake_minimum_required(VERSION 3.15)
project(mass_spring_damper_fmi3 CXX)

# cppfmu is consumed as a Conan package (see conanfile.py). find_package brings
# in the cppfmu::cppfmu target, which already carries the FMI 3.0 include paths
# and the CPPFMU_USE_FMI_3_0 define.
find_package(cppfmu REQUIRED)

# CPPFMU ships the C API wrapper as a source file rather than compiling it into
# the library, so each FMU module compiles its own copy. CMakeDeps exposes the
# package root, from which we pick up src/fmi3_functions.cpp.
set(cppfmu_pkg "${cppfmu_PACKAGE_FOLDER_RELEASE}")
if(NOT cppfmu_pkg)
    set(cppfmu_pkg "${cppfmu_PACKAGE_FOLDER_DEBUG}")
endif()

# An FMU binary is a shared library (MODULE). Its base name must equal the
# modelIdentifier in modelDescription.xml.
add_library(MassSpringDamper MODULE
    mass_spring_damper.cpp
    "${cppfmu_pkg}/src/fmi3_functions.cpp")
target_compile_features(MassSpringDamper PRIVATE cxx_std_11)
target_link_libraries(MassSpringDamper PRIVATE cppfmu::cppfmu)

# Strip the "lib" prefix so the file is MassSpringDamper.so / .dll, matching
# the modelIdentifier.
set_target_properties(MassSpringDamper PROPERTIES PREFIX "")
