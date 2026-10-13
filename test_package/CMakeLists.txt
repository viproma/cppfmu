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

# The combined package ships both fmi_functions.cpp and fmi3_functions.cpp, and
# sets no version macro, so each consumer picks its FMI version per target. Build
# an FMI 2.0 tester and an FMI 3.0 tester when both sources are present; otherwise
# build the single tester the one-version package provides.
if(EXISTS "${cppfmu_pkg}/src/fmi_functions.cpp" AND EXISTS "${cppfmu_pkg}/src/fmi3_functions.cpp")
    add_executable(${testTarget} main.cpp "${cppfmu_pkg}/src/fmi_functions.cpp")
    target_link_libraries(${testTarget} PUBLIC cppfmu::cppfmu)

    add_executable(${testTarget}_fmi3 main.cpp "${cppfmu_pkg}/src/fmi3_functions.cpp")
    target_compile_definitions(${testTarget}_fmi3 PRIVATE CPPFMU_USE_FMI_3_0)
    target_link_libraries(${testTarget}_fmi3 PUBLIC cppfmu::cppfmu)
else()
    # Determine which FMI functions file to use based on what exists
    if(EXISTS "${cppfmu_pkg}/src/fmi3_functions.cpp")
        set(fmi_funcs "${cppfmu_pkg}/src/fmi3_functions.cpp")
    else()
        set(fmi_funcs "${cppfmu_pkg}/src/fmi_functions.cpp")
    endif()

    add_executable(${testTarget} main.cpp ${fmi_funcs})
    target_link_libraries(${testTarget} PUBLIC cppfmu::cppfmu)
endif()
