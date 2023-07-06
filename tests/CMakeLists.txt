# tests

enable_testing()

find_package(Threads REQUIRED)

# all files named test*.cpp are included
set(test_files)
file(GLOB test_file_paths test*.cpp)
foreach(test_file_path ${test_file_paths})
    get_filename_component(TestFile ${test_file_path} NAME)
    list(APPEND test_files ${TestFile})
endforeach()

# add test sources
create_test_sourcelist(Tests CommonCxxTests.cxx ${test_files})

# build test driver
add_executable(CommonCxxTests ${Tests})
target_link_libraries(CommonCxxTests PUBLIC socket_manager ${CMAKE_THREAD_LIBS_INIT})

# enable lto for tests
set_property(TARGET CommonCxxTests PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

# Add all the ADD_TEST for each test
set(TestsToRun ${Tests})
list(REMOVE_ITEM TestsToRun CommonCxxTests.cxx)
foreach (test ${TestsToRun})
    get_filename_component(TName ${test} NAME_WE)
    add_test(NAME ${TName} COMMAND CommonCxxTests ${TName})
    set_tests_properties(${TName} PROPERTIES TIMEOUT 5)
endforeach()
