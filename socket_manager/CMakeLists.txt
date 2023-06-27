add_library(${PROJECT_NAME} STATIC)

set(header_path ${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME})
set(header ${header_path}/socket_manager.h
           ${header_path}/connection.h
           ${header_path}/msg_sender.h)
set(src socket_manager.cc msg_sender.cc connection.cc)

target_sources(${PROJECT_NAME} PRIVATE ${src})
add_dependencies(${PROJECT_NAME} tokio-socket-manager)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/>
        $<INSTALL_INTERFACE:${include_dest}>)

set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        IMPORTED_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.a)

set(tokio_obj_dir ${CMAKE_CURRENT_BINARY_DIR}/tokio_obj)
file(MAKE_DIRECTORY ${tokio_obj_dir})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        WORKING_DIRECTORY ${tokio_obj_dir}
        COMMAND ar -x ${CMAKE_BINARY_DIR}/libtokio_socket_manager.a
        COMMAND ar -qs ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.a *.o)

install(TARGETS ${PROJECT_NAME} DESTINATION ${main_lib_dest})
install(FILES ${header} DESTINATION ${include_dest})
install(FILES ${CMAKE_SOURCE_DIR}/include/socket_manager_c_api.h DESTINATION ${include_dest})
install(TARGETS ${PROJECT_NAME} tokio-socket-manager EXPORT ${PROJECT_NAME} DESTINATION ${main_lib_dest})
install(EXPORT ${PROJECT_NAME} DESTINATION ${main_lib_dest})

configure_file(${CMAKE_SOURCE_DIR}/socket_manager-config-version.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/socket_manager-config-version.cmake @ONLY)
install(FILES ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/socket_manager-config-version.cmake
        DESTINATION ${main_lib_dest})