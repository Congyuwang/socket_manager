cmake_minimum_required(VERSION 3.8)
project(socket_manager LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(include_dest "include/${PROJECT_NAME}-${PROJECT_VERSION}")
set(main_lib_dest "lib/${PROJECT_NAME}-${PROJECT_VERSION}")

set(Rust_TOOLCHAIN "nightly" CACHE STRING "requires nightly")

add_subdirectory(${CMAKE_SOURCE_DIR}/corrosion)
corrosion_import_crate(MANIFEST_PATH Cargo.toml)

add_subdirectory(socket_manager)

option(PACKAGE_TESTS "Build the tests" ON)
if(PACKAGE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
