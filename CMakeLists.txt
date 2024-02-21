cmake_minimum_required(VERSION 3.9)
set(CMAKE_CXX_STANDARD 20)

# read toolchain file before project
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain.cmake)

# define project
project(socket_manager LANGUAGES C CXX VERSION 0.5.2)

# set default build type as shared
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# import rust
add_subdirectory(${CMAKE_SOURCE_DIR}/corrosion)
corrosion_import_crate(MANIFEST_PATH Cargo.toml FLAGS --verbose)
set(Rust_TOOLCHAIN "nightly" CACHE STRING "requires nightly")

# enable cross language lto
corrosion_add_target_rustflags(tokio-socket-manager -C linker-plugin-lto)

set(include_dest "include/${PROJECT_NAME}-${PROJECT_VERSION}")
set(main_lib_dest "lib/${PROJECT_NAME}-${PROJECT_VERSION}")

add_subdirectory(socket_manager)

# add tests
option(PACKAGE_TESTS "Build the tests" ON)
if (PACKAGE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
