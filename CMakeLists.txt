cmake_minimum_required(VERSION 3.9)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain.cmake)

project(socket_manager LANGUAGES C CXX VERSION 0.1.0)

add_subdirectory(${CMAKE_SOURCE_DIR}/corrosion)
corrosion_import_crate(MANIFEST_PATH Cargo.toml)
set(triple ${Rust_CARGO_TARGET_ARCH}-${Rust_CARGO_TARGET_VENDOR}-${Rust_CARGO_TARGET_OS}-${Rust_CARGO_TARGET_ENV})
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER_TARGET ${triple})

set(include_dest "include/${PROJECT_NAME}-${PROJECT_VERSION}")
set(main_lib_dest "lib/${PROJECT_NAME}-${PROJECT_VERSION}")

set(Rust_TOOLCHAIN "nightly" CACHE STRING "requires nightly")

add_subdirectory(socket_manager)

option(PACKAGE_TESTS "Build the tests" ON)
if (PACKAGE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
