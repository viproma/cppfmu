cmake_minimum_required(VERSION 3.15)

project(cppfmu-test)

find_package(cppfmu REQUIRED)

set(testTarget "tester")
add_executable(${testTarget} main.cpp
    ${CMAKE_BINARY_DIR}/cppfmu/fmi_functions.cpp)
target_link_libraries(${testTarget} PUBLIC cppfmu::cppfmu)
