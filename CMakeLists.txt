cmake_minimum_required(VERSION 3.8)

set(PACKAGE_NAME "socket_manager")
set(PACKAGE_VERSION "0.1.0")
set(CMAKE_CXX_STANDARD 14)
project(socket_manager LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(FetchContent)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/socket_manager.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/socket_manager.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/include/msg_sender.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/include/msg_sender.h)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/corrosion)

# Import targets defined in a package or workspace manifest `Cargo.toml` file
corrosion_import_crate(MANIFEST_PATH Cargo.toml)

target_link_libraries(${PROJECT_NAME} tokio-socket-manager)

set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        IMPORTED_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.a
        PUBLIC_HEADER "include/socket_manager.h;include/msg_sender.h;include/socket_manager_c_api.h")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND libtool -static -o lib${PROJECT_NAME}.a lib${PROJECT_NAME}.a libtokio_socket_manager.a)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
