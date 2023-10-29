cmake_minimum_required(VERSION 3.9)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain.cmake)

project(echo_server)

set(CMAKE_CXX_STANDARD 20)
add_executable(echo_server src/echo_server.cpp)
set_property(TARGET echo_server PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

find_package(socket_manager 0.4.0 REQUIRED)
target_link_libraries(echo_server PUBLIC socket_manager)
