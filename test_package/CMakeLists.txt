cmake_minimum_required(VERSION 3.15)

project(cppfmu-test)

find_package(cppfmu REQUIRED)

set(testTarget "tester")

# Get the cppfmu package folder (CMakeDeps sets cppfmu_PACKAGE_FOLDER_RELEASE for single-config)
set(cppfmu_pkg "${cppfmu_PACKAGE_FOLDER_RELEASE}")
if(NOT cppfmu_pkg)
    set(cppfmu_pkg "${cppfmu_PACKAGE_FOLDER_DEBUG}")
endif()
if(NOT cppfmu_pkg)
    set(cppfmu_pkg "${cppfmu_PACKAGE_FOLDER}")
endif()

# Determine which FMI functions file to use based on what exists
if(EXISTS "${cppfmu_pkg}/src/fmi3_functions.cpp")
    set(fmi_funcs "${cppfmu_pkg}/src/fmi3_functions.cpp")
else()
    set(fmi_funcs "${cppfmu_pkg}/src/fmi_functions.cpp")
endif()

add_executable(${testTarget} main.cpp ${fmi_funcs})
target_link_libraries(${testTarget} PUBLIC cppfmu::cppfmu)
