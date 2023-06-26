add_library(${PROJECT_NAME} STATIC)

set(header_path ${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME})
set(header ${header_path}/socket_manager.h
           ${header_path}/msg_sender.h)
set(src socket_manager.cc msg_sender.cc)

target_sources(${PROJECT_NAME} PRIVATE ${src})
add_dependencies(${PROJECT_NAME} tokio-socket-manager)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/>
        $<INSTALL_INTERFACE:${include_dest}>)

set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        IMPORTED_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.a)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND libtool -static -o ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.a ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.a ${CMAKE_BINARY_DIR}/libtokio_socket_manager.a)

install(TARGETS ${PROJECT_NAME} DESTINATION ${main_lib_dest})
install(FILES ${header} DESTINATION ${include_dest})
install(FILES ${CMAKE_SOURCE_DIR}/include/socket_manager_c_api.h DESTINATION ${include_dest})
install(TARGETS ${PROJECT_NAME} tokio-socket-manager EXPORT ${PROJECT_NAME} DESTINATION ${main_lib_dest})
install(EXPORT ${PROJECT_NAME} DESTINATION ${main_lib_dest})
