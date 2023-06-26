cmake_minimum_required(VERSION 3.8)
project(socket_manager LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(include_dest "include/${PROJECT_NAME}-${PROJECT_VERSION}")
set(main_lib_dest "lib/${PROJECT_NAME}-${PROJECT_VERSION}")

add_subdirectory(${CMAKE_SOURCE_DIR}/corrosion)
corrosion_import_crate(MANIFEST_PATH Cargo.toml)

add_subdirectory(socket_manager)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
        $<INSTALL_INTERFACE:${include_dest}>
        $<INSTALL_INTERFACE:${lib_dest}>)

configure_file(socket_manager-config-version.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/socket_manager-config-version.cmake @ONLY)
install(FILES ${PROJECT_NAME}-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/socket_manager-config-version.cmake
        DESTINATION ${main_lib_dest})
