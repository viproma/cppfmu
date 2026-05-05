cmake_minimum_required(VERSION 3.15)
enable_testing()

file(STRINGS "${CMAKE_SOURCE_DIR}/version.txt" projectVersion)

project("cppfmu-library" VERSION ${projectVersion})

option(CPPFMU_FMI_1 "Use FMI 1.0" OFF)
option(CPPFMU_FMI_3 "Use FMI 3.0" OFF)

if(CPPFMU_FMI_1 AND CPPFMU_FMI_3)
    message(FATAL_ERROR "CPPFMU_FMI_1 and CPPFMU_FMI_3 are mutually exclusive; enable at most one.")
endif()

if(CPPFMU_FMI_1)
    find_package(fmi1 CONFIG REQUIRED)
    set(FMI fmi1::cosim)
    set(FMI_SOURCES ${CMAKE_SOURCE_DIR}/cppfmu_cs.cpp)
elseif(CPPFMU_FMI_3)
    find_package(fmi3 CONFIG REQUIRED)
    set(FMI fmi3::fmi3)
    set(FMI_SOURCES ${CMAKE_SOURCE_DIR}/cppfmu_cs_fmi3.cpp)
else()
    find_package(fmi2 CONFIG REQUIRED)
    set(FMI fmi2::fmi2)
    set(FMI_SOURCES ${CMAKE_SOURCE_DIR}/cppfmu_cs.cpp)
endif()

# fmi_functions.cpp / fmi3_functions.cpp must be compiled by end user

add_library(cppfmu STATIC ${FMI_SOURCES})
target_include_directories(cppfmu PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(cppfmu PUBLIC ${FMI})

if(CPPFMU_FMI_1)
    target_compile_definitions(cppfmu PUBLIC CPPFMU_USE_FMI_1_0)
elseif(CPPFMU_FMI_3)
    target_compile_definitions(cppfmu PUBLIC CPPFMU_USE_FMI_3_0)
endif()

install(TARGETS cppfmu ARCHIVE DESTINATION lib RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_common.hpp DESTINATION include)
if(CPPFMU_FMI_3)
    install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_cs_fmi3.hpp DESTINATION include)
    install(FILES ${CMAKE_SOURCE_DIR}/fmi3_functions.cpp DESTINATION src)
else()
    install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_cs.hpp DESTINATION include)
    install(FILES ${CMAKE_SOURCE_DIR}/fmi_functions.cpp DESTINATION src)
endif()

if(CPPFMU_FMI_1)
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
