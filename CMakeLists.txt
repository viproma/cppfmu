cmake_minimum_required(VERSION 3.15)
enable_testing()

file(STRINGS "${CMAKE_SOURCE_DIR}/version.txt" projectVersion)

project("cppfmu-library" VERSION ${projectVersion})

option(CPPFMU_FMI_1 "Use FMI 1.0" OFF)

if(CPPFMU_FMI_1)
    find_package(fmi1 CONFIG REQUIRED)
    set(FMI fmi1::cosim)
else()
    find_package(fmi2 CONFIG REQUIRED)
    set(FMI fmi2::fmi2)
endif()

set(sources ${CMAKE_SOURCE_DIR}/cppfmu_cs.cpp)
# fmi_functions.cpp must be compiled by end user

add_library(cppfmu STATIC ${sources})
target_include_directories(cppfmu PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(cppfmu PUBLIC ${FMI})

if(CPPFMU_FMI_1)
    target_compile_definitions(cppfmu PUBLIC CPPFMU_USE_FMI_1_0)
endif()

install(TARGETS cppfmu ARCHIVE DESTINATION lib RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/cppfmu_common.hpp ${CMAKE_SOURCE_DIR}/cppfmu_cs.hpp DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES ${CMAKE_SOURCE_DIR}/fmi_functions.cpp DESTINATION ${CMAKE_INSTALL_PREFIX}/src)

if(NOT CPPFMU_FMI_1)
    add_executable(cs_test
        "tests/cs_test.cpp"
        "tests/cs_slave.cpp"
        "fmi_functions.cpp"
    )
    target_compile_features(cs_test PRIVATE cxx_std_11)
    target_link_libraries(cs_test PRIVATE cppfmu)
    add_test(NAME "cs_test" COMMAND cs_test)
endif()
