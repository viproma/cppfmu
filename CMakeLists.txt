cmake_minimum_required(VERSION 3.15)
enable_testing()

# Emit a compile_commands.json so clangd and clang-tidy see each source with the
# exact flags it is built with -- in particular the CPPFMU_USE_FMI_3_0 macro on
# the FMI 3.0 sources, without which the tools resolve the wrong fmi*Functions.h.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(STRINGS "${CMAKE_SOURCE_DIR}/version.txt" projectVersion)

project("cppfmu-library" VERSION ${projectVersion})

option(CPPFMU_FMI_1 "Use FMI 1.0" OFF)
option(CPPFMU_FMI_3 "Use FMI 3.0" OFF)
option(CPPFMU_FMI_ALL "Build both FMI 2.0 and FMI 3.0 in one library" OFF)

if(CPPFMU_FMI_1 AND CPPFMU_FMI_3)
    message(FATAL_ERROR "CPPFMU_FMI_1 and CPPFMU_FMI_3 are mutually exclusive; enable at most one.")
endif()
if(CPPFMU_FMI_ALL AND (CPPFMU_FMI_1 OR CPPFMU_FMI_3))
    message(FATAL_ERROR "CPPFMU_FMI_ALL bundles every version; do not combine it with CPPFMU_FMI_1 or CPPFMU_FMI_3.")
endif()

# fmi_functions.cpp / fmi3_functions.cpp must be compiled by the end user

if(CPPFMU_FMI_ALL)
    # Combined build: FMI 2.0 and FMI 3.0 in one library. cppfmu_common.hpp selects
    # the FMI version by preprocessor macro, so each source is compiled with its own
    # -- cppfmu_cs.cpp as FMI 2.0 (no macro), cppfmu_cs_fmi3.cpp as FMI 3.0 -- and the
    # two produce disjoint symbols (SlaveInstance vs SlaveInstance3), so they coexist
    # in the archive. No version macro is put on the target itself: a consumer that
    # links this library sets the macro per module, matching the source it compiles.
    find_package(fmi2 CONFIG REQUIRED)
    find_package(fmi3 CONFIG REQUIRED)

    add_library(cppfmu STATIC
        ${CMAKE_SOURCE_DIR}/cppfmu_cs.cpp
        ${CMAKE_SOURCE_DIR}/cppfmu_cs_fmi3.cpp
        ${CMAKE_SOURCE_DIR}/cppfmu_lifecycle_fmi3.cpp)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/cppfmu_cs_fmi3.cpp
        PROPERTIES COMPILE_DEFINITIONS CPPFMU_USE_FMI_3_0)
    target_include_directories(cppfmu PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(cppfmu PUBLIC fmi2::fmi2 fmi3::fmi3)
else()
    if(CPPFMU_FMI_1)
        find_package(fmi1 CONFIG REQUIRED)
        set(FMI fmi1::cosim)
        set(FMI_SOURCES ${CMAKE_SOURCE_DIR}/cppfmu_cs.cpp)
    elseif(CPPFMU_FMI_3)
        find_package(fmi3 CONFIG REQUIRED)
        set(FMI fmi3::fmi3)
        set(FMI_SOURCES
            ${CMAKE_SOURCE_DIR}/cppfmu_cs_fmi3.cpp
            ${CMAKE_SOURCE_DIR}/cppfmu_lifecycle_fmi3.cpp)
    else()
        find_package(fmi2 CONFIG REQUIRED)
        set(FMI fmi2::fmi2)
        set(FMI_SOURCES ${CMAKE_SOURCE_DIR}/cppfmu_cs.cpp)
    endif()

    add_library(cppfmu STATIC ${FMI_SOURCES})
    target_include_directories(cppfmu PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(cppfmu PUBLIC ${FMI})

    if(CPPFMU_FMI_1)
        target_compile_definitions(cppfmu PUBLIC CPPFMU_USE_FMI_1_0)
    elseif(CPPFMU_FMI_3)
        target_compile_definitions(cppfmu PUBLIC CPPFMU_USE_FMI_3_0)
    endif()
endif()

install(TARGETS cppfmu ARCHIVE DESTINATION lib RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_common.hpp DESTINATION include)
if(CPPFMU_FMI_ALL)
    install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_cs.hpp DESTINATION include)
    install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_cs_fmi3.hpp DESTINATION include)
    install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_lifecycle_fmi3.hpp DESTINATION include)
    install(FILES ${CMAKE_SOURCE_DIR}/fmi_functions.cpp DESTINATION src)
    install(FILES ${CMAKE_SOURCE_DIR}/fmi3_functions.cpp DESTINATION src)
elseif(CPPFMU_FMI_3)
    install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_cs_fmi3.hpp DESTINATION include)
    install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_lifecycle_fmi3.hpp DESTINATION include)
    install(FILES ${CMAKE_SOURCE_DIR}/fmi3_functions.cpp DESTINATION src)
else()
    install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_cs.hpp DESTINATION include)
    install(FILES ${CMAKE_SOURCE_DIR}/fmi_functions.cpp DESTINATION src)
endif()

if(CPPFMU_FMI_ALL)
    # Both versions are built, so both test suites run.
    add_executable(cs_test
        "tests/cs_test.cpp"
        "tests/cs_slave.cpp"
        "fmi_functions.cpp"
    )
    target_compile_features(cs_test PRIVATE cxx_std_11)
    target_link_libraries(cs_test PRIVATE cppfmu)
    add_test(NAME "cs_test" COMMAND cs_test)

    add_executable(cs_test_fmi3
        "tests/cs_test_fmi3.cpp"
        "tests/cs_slave_fmi3.cpp"
        "fmi3_functions.cpp"
    )
    target_compile_features(cs_test_fmi3 PRIVATE cxx_std_11)
    target_compile_definitions(cs_test_fmi3 PRIVATE CPPFMU_USE_FMI_3_0)
    target_link_libraries(cs_test_fmi3 PRIVATE cppfmu)
    add_test(NAME "cs_test_fmi3" COMMAND cs_test_fmi3)

    add_executable(lifecycle_fmi3_test
        "tests/lifecycle_fmi3_test.cpp"
        "cppfmu_lifecycle_fmi3.cpp"
    )
    target_compile_features(lifecycle_fmi3_test PRIVATE cxx_std_11)
    target_include_directories(lifecycle_fmi3_test PRIVATE ${CMAKE_SOURCE_DIR})
    add_test(NAME "lifecycle_fmi3_test" COMMAND lifecycle_fmi3_test)
elseif(CPPFMU_FMI_1)
    add_executable(cs_test
        "tests/cs_test.cpp"
        "tests/cs_slave.cpp"
        "fmi_functions.cpp"
    )
    target_compile_features(cs_test PRIVATE cxx_std_11)
    target_link_libraries(cs_test PRIVATE cppfmu)
    add_test(NAME "cs_test" COMMAND cs_test)
elseif(CPPFMU_FMI_3)
    add_executable(cs_test
        "tests/cs_test_fmi3.cpp"
        "tests/cs_slave_fmi3.cpp"
        "fmi3_functions.cpp"
    )
    target_compile_features(cs_test PRIVATE cxx_std_11)
    target_link_libraries(cs_test PRIVATE cppfmu)
    add_test(NAME "cs_test" COMMAND cs_test)

    add_executable(lifecycle_fmi3_test
        "tests/lifecycle_fmi3_test.cpp"
        "cppfmu_lifecycle_fmi3.cpp"
    )
    target_compile_features(lifecycle_fmi3_test PRIVATE cxx_std_11)
    target_include_directories(lifecycle_fmi3_test PRIVATE ${CMAKE_SOURCE_DIR})
    add_test(NAME "lifecycle_fmi3_test" COMMAND lifecycle_fmi3_test)
else()
    add_executable(cs_test
        "tests/cs_test.cpp"
        "tests/cs_slave.cpp"
        "fmi_functions.cpp"
    )
    target_compile_features(cs_test PRIVATE cxx_std_11)
    target_link_libraries(cs_test PRIVATE cppfmu)
    add_test(NAME "cs_test" COMMAND cs_test)
endif()
