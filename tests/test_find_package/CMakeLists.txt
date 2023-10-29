cmake_minimum_required(VERSION 3.9)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain.cmake)

project(helloworld_server)

set(CMAKE_CXX_STANDARD 17)
add_executable(helloworld_server helloworld_server.cpp)
set_property(TARGET helloworld_server PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

find_package(socket_manager 0.4.0 REQUIRED)
target_link_libraries(helloworld_server PUBLIC socket_manager)
